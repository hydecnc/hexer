/* buffer.c	19/8/1995
 * Buffers, version 0.3
 */

/* Copyright (c) 1995,1996 Sascha Demetrio
 * Copyright (c) 2009, 2010, 2015, 2018 Peter Pentchev
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *    If you modify any part of HEXER and redistribute it, you must add
 *    a notice to the `README' file and the modified source files containing
 *    information about the  changes you made.  I do not want to take
 *    credit or be blamed for your modifications.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *    If you modify any part of HEXER and redistribute it in binary form,
 *    you must supply a `README' file containing information about the
 *    changes you made.
 * 3. The name of the developer may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * HEXER WAS DEVELOPED BY SASCHA DEMETRIO.
 * THIS SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT.
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT
 * NOT MAKE USE OF THIS WORK.
 *
 * DISCLAIMER:
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <memory.h>
#include <errno.h>

#include "buffer.h"
#include "defs.h"
#include "util.h"

struct BufferOptions b_default_options = {
  BUFFER_BLOCKSIZE,
  BUFFER_OPT,
  BUFFER_MAX_BLOCKS
};

long last_position = 0;
  /* `last_position' is the position of the first byte in line `last_number'.
   */
long last_number = 1;
  /* `last_number' is the number of the line most recently accessed using one
   * of the following functions.
   */
long last_length = -1;
  /* `last_length' is the length of the line `last_number'.  A value of -1
   * means that the length of that line hasn't been evaluated yet.
   */
Buffer *last_buffer = 0;
  /* `last_buffer' is a pointer to the buffer that the above counters are
   * talking about.
   * NOTE:  If `last_buffer' is modified, the variables `last_position',
    *  `last_number' and `last_length' are set to 0, 1, -1 (respectively).
   */

/* Whenever the buffer is modified, the macro `BUFFER_CHANGED' is called.
 * Purpose: 1. The buffer is marked modified;  2. Discard all information
 *   line numbers that could have become invalid by the modification.
 */
#define BUFFER_CHANGED(buffer) {                                              \
  if (buffer == last_buffer)                                                  \
    last_position = 0, last_number = 1, last_length = -1;                     \
  buffer->modified = 1; }

  unsigned long
count_lines(char *source, unsigned long count)
{
  char *i;
  unsigned long j = 0;

  for (i = source; i < source + count; i++) if (*i == '\n') j++;
  return j;
}
/* count_lines */


/* BufferBlock:
 */

  BufferBlock *
new_buffer_block(const unsigned long blocksize, char * const data)
{
  BufferBlock *new_block;

  new_block = (BufferBlock *)malloc_fatal(sizeof(BufferBlock));
  new_block->next_block = 0;

  if (data != 0) new_block->data = data;
  else new_block->data = (char *)malloc_fatal(blocksize);
  return new_block;
}
/* new_buffer_block */

  void
delete_buffer_block(BufferBlock * const block)
{
  free((char *)block->data);
  free((char *)block);
}
/* delete_buffer_block */


/* Buffer:
 */

  Buffer *
new_buffer(struct BufferOptions *arg_options)
{
  Buffer * new_buf;
  struct BufferOptions options;
  
  options = arg_options ? *arg_options : b_default_options;
  new_buf = (Buffer *)malloc_fatal(sizeof(Buffer));
  new_buf->first_block = 0;
  new_buf->size = 0;
  new_buf->blocksize = options.blocksize;
  new_buf->read_only = options.opt & B_READ_ONLY;
  new_buf->modified = 0;
  return new_buf;
}
/* new_buffer */

  int
delete_buffer(Buffer * const buffer)
{
  BufferBlock *i, *j;

  for (i = buffer->first_block; i; i = j) {
    j = i->next_block;
    delete_buffer_block(i);
  }
  free((char *)buffer);
  return 0;
}
/* delete_buffer */

  BufferBlock *
find_block(Buffer *buffer, unsigned long position)
{
  BufferBlock *i;
  unsigned long block_number;

  if (position >= buffer->size) return 0;
  for (i = buffer->first_block, block_number = 1;
       i != 0 && block_number * buffer->blocksize <= position;
       i = i->next_block, block_number++);
  assert(i);
  return i;
}
/* find_block */

  int
b_set_size(Buffer *buffer, unsigned long size)
{
  BufferBlock *i, *j = 0, *k;
  unsigned long block_number;

  assert(!buffer->read_only);
  BUFFER_CHANGED(buffer);
  if (size < buffer->size) {
    if (!(i = find_block(buffer, size))) return -1;
    k = i;
    for (i = i->next_block; i; i = j) {
      j = i->next_block;
      delete_buffer_block(i);
    }
    k->next_block = 0;
  } else {
    if (buffer->first_block == 0) {
      buffer->first_block = new_buffer_block(buffer->blocksize, 0);
    }
    for (i = buffer->first_block, block_number = 1;
	 i->next_block != 0;
	 i = i->next_block, block_number++);
    for (;
         block_number * buffer->blocksize < size;
         i = i->next_block, block_number++) {
      if (!i) return -1;
      i->next_block = new_buffer_block(buffer->blocksize, 0);
    }
  }
  buffer->size = size;
  return 0;
}
/* b_set_size */

  long
b_read(Buffer *buffer, char *target, unsigned long position, unsigned long count)
{
  BufferBlock *block;
  unsigned long bs = buffer->blocksize;
  unsigned long ofs = position % bs;
  unsigned long r = bs - ofs;

  if (!(block = find_block(buffer, position))) return -1;
  if (count + position > buffer->size) count = buffer->size - position;
  if (count <= r) {
    memmove(target, block->data + ofs, count);
    return count;
  } else {
    memmove(target, block->data + ofs, r);
    long more = b_read(buffer, target + r, position + r, count - r);
    if (more < 0)
      return more;
    return r + more;
  }
}
/* b_read */

  unsigned long
b_write(Buffer *buffer, char *source, unsigned long position, unsigned long count)
{
  BufferBlock *block;
  unsigned long bs = buffer->blocksize;

  assert(!buffer->read_only);
  if (count) BUFFER_CHANGED(buffer);
  if (!(block = find_block(buffer, position))) return 0;
  if (count + position > buffer->size) count = buffer->size - position;
  if (count <= (bs - position % bs)) {
    memmove(block->data + position % bs, source, count);
    return count;
  } else {
    unsigned long r = bs - position % bs;
    memmove(block->data + position % bs, source, r);
    return r + b_write(buffer, source + r, position + r, count - r);
  }
}
/* b_write */

  long
b_write_append(Buffer *buffer, char *source, unsigned long position, unsigned long count)
{
  assert(!buffer->read_only);
  if (position + count > buffer->size) {
    if (b_set_size(buffer, position + count)) return -1;
    return b_write(buffer, source, position, count);
  } else return b_write(buffer, source, position, count);
}
/* b_write_append */

  long
b_append(Buffer *buffer, char *source, unsigned long count)
{
  return b_write_append(buffer, source, buffer->size, count);
}
/* b_append */

  unsigned long
b_count_lines(Buffer *buffer, unsigned long position, unsigned long count)
{
  BufferBlock *block;
  unsigned long bs = buffer->blocksize;

  if (!(block = find_block(buffer, position))) return 0;
  if (count + position > buffer->size) count = buffer->size - position;
  if (count <= bs - position % bs) 
    return count_lines(block->data + position % bs, count);
  else {
    unsigned long r = bs - position % bs;
    return count_lines(block->data + position % bs, r)
            + b_count_lines(buffer, position + r, count - r);
  }
}
/* b_count_lines */

  long
b_insert(Buffer *buffer, unsigned long position, unsigned long count)
{
  unsigned long i;

  assert(!buffer->read_only);
  if(b_set_size(buffer, buffer->size + count) < 0) return -1;
  if ((i = buffer->size - count - position))
    b_copy(buffer, buffer, position + count, position, i);
  return count;
}
/* b_insert */

  long
b_delete(Buffer *buffer, unsigned long position, unsigned long count)
{
  unsigned long size = buffer->size;

  assert(!buffer->read_only);
  if (b_copy(buffer, buffer, position, position + count,
         buffer->size - position - count) < 0)
    return -1;
  if (b_set_size(buffer, buffer->size - count) < 0) return -1;
  return size - buffer->size;
}
/* b_delete */

  long
b_copy(Buffer *target_buffer, Buffer *source_buffer, unsigned long target_position, unsigned long source_position, unsigned long count)
{
  BufferBlock *t_block;
  unsigned long t_offset = target_position % target_buffer->blocksize;
  unsigned long t_blocksize = target_buffer->blocksize;

  assert(!target_buffer->read_only);
  if (count) BUFFER_CHANGED(target_buffer);
  if ((target_buffer == source_buffer) &&
      (target_position > source_position)) {
    long r;
    r = b_copy_forward(target_buffer, target_position,
                       source_position, count);
    return r;
  }
  if (!(t_block = find_block(target_buffer, target_position))) return -1;
  if (count <= t_blocksize - t_offset) {
    long r = b_read(source_buffer, t_block->data + t_offset,
                    source_position, count);
    return r;
  } else {
    long r = b_read(source_buffer, t_block->data + t_offset,
                    source_position, t_blocksize - t_offset);
    if (r > 0 && (unsigned long)r >= t_blocksize - t_offset)
    {
      long more = b_copy(target_buffer, source_buffer,
                  target_position + r, source_position + r, count - r);
      if (more < 0)
	return more;
      else
	r += more;
    }
    return r;
  }
}
/* b_copy */

  long
b_copy_forward(Buffer *buffer, unsigned long target_position, unsigned long source_position, unsigned long count)
{
  BufferBlock *t_block, *s_block;
  unsigned long bs = buffer->blocksize;
  unsigned long t_offset = target_position % bs; /* target offset. */
  unsigned long s_offset = source_position % bs; /* source offset. */
  unsigned long tr_offset = (target_position + count - 1) % bs + 1;
    /* target reverse offset. */
  unsigned long sr_offset = (source_position + count) % bs;
    /* source reverse offset. */

  assert(!buffer->read_only);
  assert(target_position > source_position);
  BUFFER_CHANGED(buffer);
  if (target_position > buffer->size) return -1;
  if (count + target_position > buffer->size)
    count = buffer->size - target_position;
  t_block = find_block(buffer, target_position + count - 1);
  assert(t_block != NULL);
  if (count <= bs - t_offset) {
    if (count <= bs - s_offset) {
      s_block = find_block(buffer, source_position);
      assert(s_block != NULL);
      memmove(t_block->data + t_offset, s_block->data + s_offset, count);
    } else {
      assert(s_offset > t_offset);
      s_block = find_block(buffer, source_position + count);
      assert(s_block != NULL);
      memmove(t_block->data + tr_offset - sr_offset, s_block->data,
                sr_offset);
      s_block = find_block(buffer, source_position);
      assert(s_block != NULL);
      memmove(t_block->data + t_offset, s_block->data + s_offset,
                count - sr_offset);
    }
    return count;
  } else {
    unsigned long r = tr_offset;
    if (tr_offset >= sr_offset) {
      if (sr_offset) {
        s_block = find_block(buffer, source_position + count);
        assert(s_block != NULL);
        memmove(t_block->data + tr_offset - sr_offset,
                s_block->data, sr_offset);
      }
      if (sr_offset != tr_offset) {
        s_block = find_block(buffer, source_position + count - tr_offset);
        assert(s_block != NULL);
        memmove(t_block->data, s_block->data + bs - tr_offset + sr_offset,
                tr_offset - sr_offset);
      }
    } else {
      s_block = find_block(buffer, source_position + count);
      assert(s_block != NULL);
      memmove(t_block->data, s_block->data + sr_offset - tr_offset,
              tr_offset);
    }
    r += b_copy_forward(buffer, target_position, source_position, count - r);
    return r;
  }
}
/* b_copy_forward */

  void
b_clear(Buffer * const buffer)
{
  BufferBlock *i, *j;

  assert(!buffer->read_only);
  BUFFER_CHANGED(buffer);
  for (i = buffer->first_block; i; i = j) {
    j = i->next_block;
    delete_buffer_block(i);
  }
  buffer->first_block = 0;
  buffer->size = 0;
}
/* b_clear */

  long
b_read_buffer_from_file(Buffer * const buffer, const char * const filename)
{
  BufferBlock *i = 0;
  char *tmp;
  int file = open(filename, O_RDONLY);
  int err_f = 0;
    
  assert(!buffer->read_only);
  BUFFER_CHANGED(buffer);
  if (file <= 0) {
    close(file);
    return -1;
  }
  b_clear(buffer);
  do {
    ssize_t bytes_read;

    tmp = (char *)malloc_fatal(buffer->blocksize);
    bytes_read = read(file, tmp, buffer->blocksize);
    if (bytes_read > 0)	{
      if (buffer->first_block == 0) {
	buffer->first_block =
	  new_buffer_block(buffer->blocksize, tmp);
	i = buffer->first_block;
      } else {
	i->next_block = new_buffer_block(buffer->blocksize, tmp);
	i = i->next_block;
      }
      buffer->size += bytes_read;
    } else {
      err_f = bytes_read < 0;
      free((char *)tmp);
      break;
    }
  } while (1);
  close(file);
  if (err_f) {
    b_clear(buffer);
    return -1;
  }
  return buffer->size;
}
/* b_read_buffer_from_file */

  long
b_write_buffer_to_file(Buffer *buffer, char *filename)
{
  BufferBlock *i;
  unsigned long bytes_wrote = 0;
  unsigned long bs = buffer->blocksize;
  unsigned long blocks;
  int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);

  if (file < 0)
    return -1;
  for (i = buffer->first_block, blocks = 1;
       i;
       i = i->next_block, blocks++) {
    ssize_t bytes;
    bytes = write_buf(file,
                  i->data,
                  (blocks * bs > buffer->size) ? buffer->size % bs : bs);
    if (bytes < 0) {
      close(file);
      return -1;
    }
    bytes_wrote += bytes;
  }
  close(file);
  return bytes_wrote;
}
/* b_write_buffer_to_file */

  long
b_copy_to_file(Buffer *buffer, const char *filename, unsigned long position, unsigned long count)
{
  char *tmp = malloc_fatal(buffer->blocksize);
  long bytes_read = 0, bytes_wrote = 0;
  int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);

  if (file < 0) {
    free((char *)tmp);
    if (file >= 0) close(file);
    return -1;
  }
  do {
    bytes_read =
      b_read(buffer, tmp, position,
	     (buffer->blocksize < count) ? buffer->blocksize : count);
    if (bytes_read < 0 || write_buf(file, tmp, bytes_read) < 0) {
      free((char *)tmp);
      close(file);
      return -1;
    }
    count -= bytes_read;
    bytes_wrote += bytes_read;
    position += bytes_read;
    if (bytes_read == 0 || count <= 0) break;
  } while (1);
  free((char *)tmp);
  close(file);
  return bytes_wrote;
}
/* b_copy_to_file */

  long
b_paste_from_file(Buffer *buffer, const char *filename, unsigned long position)
{
  char *tmp = malloc_fatal(buffer->blocksize);
  long bytes_read, bytes_wrote = 0;
  int file = open(filename, O_RDONLY);
  
  assert(!buffer->read_only);
  if (file < 0) {
    free((char *)tmp);
    if (file >= 0) close(file);
    return -1;
  }
  BUFFER_CHANGED(buffer);
  do {
    bytes_read = read(file, tmp, buffer->blocksize);
    if (bytes_read < 0 || b_insert(buffer, position, bytes_read) < 0) {
      free((char *)tmp);
      close(file);
      return -1;
    }
    bytes_wrote += b_write(buffer, tmp, position, bytes_read);
    position += bytes_read;
  } while (bytes_read > 0);
  free((char *)tmp);
  close(file);
  return bytes_wrote;
}
/* b_paste_from_file */


/* The following set of functions is for dealing with text-buffers.
 */

  unsigned long
b_no_lines(Buffer *buffer)
{
  return b_count_lines(buffer, 0, buffer -> size);
}
/* b_no_lines */

  long
b_goto_line(Buffer *buffer, unsigned long number)
{
  BufferBlock *i;
  unsigned long newlines = 0, blocks = 0;

  if (number == 1) return 0;
  if (buffer == last_buffer && last_number >= 0 && number == (unsigned long)last_number) return last_position;
  for (i = buffer -> first_block; i; i = i -> next_block) {
    size_t ofs;
    for (ofs = 0; ofs < buffer -> blocksize; ofs++)
      if (i->data[ofs] == '\n')
        if (++newlines == number - 1) {
          unsigned long block_position = blocks * buffer -> blocksize;
          last_position = block_position + ofs + 1;
          last_number = number;
          last_buffer = buffer;
          last_length = -1;
          return last_position;
        }
    blocks++;
  }
  return -1;
}
/* b_goto_line */

/* end of buffer.c */


/* VIM configuration: (do not delete this line)
 *
 * vim:bk:nodg:efm=%f\:%l\:%m:hid:icon:
 * vim:sw=2:sm:textwidth=79:ul=1024:wrap:
 */
