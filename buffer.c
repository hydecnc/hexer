/* buffer.c	19/8/1995
 * Buffers, version 0.3
 */

/* Copyright (c) 1995,1996 Sascha Demetrio
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *    If you modify any part of HEXER and resitribute it, you must add
 *    a notice to the `README' file and the modified source files containing
 *    information about the  changes you made.  I do not want to take
 *    credit or be blamed for your modifications.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *    If you modify any part of HEXER and resitribute it in binary form,
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

#if !HAVE_MEMMOVE
  static void
b_memmove(char *t, const char *s, const long count)
{
  register long i;

  if (t > s)
    for (i = count - 1; i >= 0; --i) t[i] = s[i];
  else
    for (i = 0; i < count; ++i) t[i] = s[i];
}
/* b_memmove */
#else
#define b_memmove(a, b, c) memmove(a, b, c)
#endif

/* Whenever the buffer is modified, the macro `BUFFER_CHANGED' is called.
 * Purpose: 1. The buffer is marked modified;  2. Discard all information
 *   line numbers that could have become invalid by the modification.
 */
#define BUFFER_CHANGED(buffer) {                                              \
  if (buffer == last_buffer)                                                  \
    last_position = 0, last_number = 1, last_length = -1;                     \
  buffer->modified = 1; }

  long
count_lines(source, count)
  char *source;
  long count;
{
  char *i;
  long j = 0;

  for (i = source; i < source + count; i++) if (*i == '\n') j++;
  return j;
}
/* count_lines */


/* BufferBlock:
 */

  BufferBlock *
new_buffer_block(blocksize, data)
  unsigned long blocksize;
  char *data;
{
  BufferBlock *new_block;

  if (!(new_block = (BufferBlock *)malloc(sizeof(BufferBlock)))) return 0;
  new_block->next_block = 0;

  if (data != 0) new_block->data = data;
  else if (!(new_block->data = (char *)malloc(blocksize))) {
    free((char *)new_block);
    return 0;
  }
  return new_block;
}
/* new_buffer_block */

  void
delete_buffer_block(block)
  BufferBlock *block;
{
  free((char *)block->data);
  free((char *)block);
}
/* delete_buffer_block */


/* Buffer:
 */

  Buffer *
new_buffer(arg_options)
  struct BufferOptions *arg_options;
{
  Buffer * new_buffer;
  struct BufferOptions options;
  
  options = arg_options ? *arg_options : b_default_options;
  if (!(new_buffer = (Buffer *)malloc(sizeof(Buffer)))) return 0;
  new_buffer->first_block = 0;
  new_buffer->size = 0;
  new_buffer->blocksize = options.blocksize;
  new_buffer->read_only = options.opt & B_READ_ONLY;
  new_buffer->modified = 0;
  return new_buffer;
}
/* new_buffer */

  int
delete_buffer(buffer)
  Buffer *buffer;
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

  int
copy_buffer(target, source)
  Buffer *target;
  Buffer *source;
  /* NOTE: `copy_buffer()' only copies the contents of `source', i.e.
   *   the blacksize of `target' is kept unchanged.
   */
{
  BufferBlock *i;
  unsigned long position = 0;
  
  assert(!target->read_only);
  if (target == source) return 0;
  b_clear(target); 
  for (i = source->first_block; i; i = i->next_block)
    position += b_write(target, i->data, position, source->blocksize);
  return 0;
}
/* copy_buffer */

  void
clear_buffer(buffer)
  Buffer *buffer;
{
  b_clear(buffer);
}
/* clear_buffer */

  BufferBlock *
find_block(buffer, position)
  Buffer *buffer;
  unsigned long position;
{
  BufferBlock *i;
  long block_number;

  if (position >= buffer->size) return 0;
  for (i = buffer->first_block, block_number = 1;
       i != 0 && block_number * buffer->blocksize <= position;
       i = i->next_block, block_number++);
  assert(i);
  return i;
}
/* find_block */

  int
b_set_size(buffer, size)
  Buffer *buffer;
  unsigned long size;
{
  BufferBlock *i, *j = 0, *k;
  long block_number;

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
b_read(buffer, target, position, count)
  Buffer *buffer;
  char *target;
  long position;
  long count;
{
  BufferBlock *block;
  unsigned long bs = buffer->blocksize;

  if (!(block = find_block(buffer, position))) return 0;
  if (count + position > buffer->size) count = buffer->size - position;
  if (count <= (bs - position % bs)) {
    b_memmove(target, block->data + position % bs, count);
    return count;
  } else {
    long r = bs - position % bs;
    b_memmove(target, block->data + position % bs, r);
    r += b_read(buffer, target + r, position + r, count - r);
    return r;
  }
}
/* b_read */

  long
b_write(buffer, source, position, count)
  Buffer *buffer;
  char *source;
  long position;
  long count;
{
  BufferBlock *block;
  unsigned long bs = buffer->blocksize;

  assert(!buffer->read_only);
  if (count) BUFFER_CHANGED(buffer);
  if (!(block = find_block(buffer, position))) return 0;
  if (count + position > buffer->size) count = buffer->size - position;
  if (count <= (bs - position % bs)) {
    b_memmove(block->data + position % bs, source, count);
    return count;
  } else {
    long r = bs - position % bs;
    b_memmove(block->data + position % bs, source, r);
    return r + b_write(buffer, source + r, position + r, count - r);
  }
}
/* b_write */

  long
b_write_append(buffer, source, position, count)
  Buffer *buffer;
  char *source;
  long position;
  long count;
{
  assert(!buffer->read_only);
  if (position + count > buffer->size) {
    if (b_set_size(buffer, position + count)) return -1;
    return b_write(buffer, source, position, count);
  } else return b_write(buffer, source, position, count);
}
/* b_write_append */

  long
b_append(buffer, source, count)
  Buffer *buffer;
  char *source;
  long count;
{
  return b_write_append(buffer, source, buffer->size, count);
}
/* b_append */

  long
b_fill(buffer, c, position, count)
  Buffer *buffer;
  char c;
  long position;
  long count;
{
  BufferBlock *block;
  unsigned long bs = buffer->blocksize;

  assert(!buffer->read_only);
  if (count) BUFFER_CHANGED(buffer);
  if (!(block = find_block(buffer, position))) return 0;
  if (count + position > buffer->size) count = buffer->size - position;
  if (count <= (bs - position % bs)) {
    memset(block->data + position % bs, c, count);
    return count;
  } else {
    long r = bs - position % bs;
    memset(block->data + position % bs, c, r);
    return r + b_fill(buffer, c, position + r, count - r);
  }
}
/* b_fill */

  long
b_fill_append(buffer, c, position, count)
  Buffer *buffer;
  char c;
  long position;
  long count;
{
  if (position + count > buffer->size)
    b_set_size(buffer, position + count);
  return b_fill(buffer, c, position, count);
}
/* b_fill_append */

  long
b_count_lines(buffer, position, count)
  Buffer *buffer;
  long position;
  long count;
{
  BufferBlock *block;
  unsigned long bs = buffer->blocksize;

  if (!(block = find_block(buffer, position))) return 0;
  if (count + position > buffer->size) count = buffer->size - position;
  if (count <= bs - position % bs) 
    return count_lines(block->data + position % bs, count);
  else {
    long r = bs - position % bs;
    return count_lines(block->data + position % bs, r)
            + b_count_lines(buffer, position + r, count - r);
  }
}
/* b_count_lines */

  long
b_insert(buffer, position, count)
  Buffer *buffer;
  long position;
  long count;
{
  long i;

  assert(!buffer->read_only);
  if(b_set_size(buffer, buffer->size + count) < 0) return -1;
  if ((i = buffer->size - count - position))
    b_copy(buffer, buffer, position + count, position, i);
  return count;
}
/* b_insert */

  long
b_delete(buffer, position, count)
  Buffer *buffer;
  long position;
  long count;
{
  long size = buffer->size;

  assert(!buffer->read_only);
  b_copy(buffer, buffer, position, position + count,
         buffer->size - position - count);
  if (b_set_size(buffer, buffer->size - count) < 0) return -1;
  return size - buffer->size;
}
/* b_delete */

  long
b_copy(target_buffer, source_buffer, target_position, source_position, count)
  Buffer *target_buffer;
  Buffer *source_buffer;
  long target_position;
  long source_position;
  long count;
{
  BufferBlock *t_block;
  long t_offset = target_position % target_buffer->blocksize;
  long t_blocksize = target_buffer->blocksize;

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
    if (r >= t_blocksize - t_offset)
      r += b_copy(target_buffer, source_buffer,
                  target_position + r, source_position + r, count - r);
    return r;
  }
}
/* b_copy */

  long
b_copy_forward(buffer, target_position, source_position, count)
  Buffer *buffer;
  long target_position;
  long source_position;
  long count;
{
  BufferBlock *t_block, *s_block;
  long bs = buffer->blocksize;
  long t_offset = target_position % bs; /* target offset. */
  long s_offset = source_position % bs; /* source offset. */
  long tr_offset = (target_position + count - 1) % bs + 1;
    /* target reverse offset. */
  long sr_offset = (source_position + count) % bs;
    /* source reverse offset. */

  assert(!buffer->read_only);
  assert(target_position > source_position);
  BUFFER_CHANGED(buffer);
  if (target_position > buffer->size) return -1;
  if (count + target_position > buffer->size)
    count = buffer->size - target_position;
  assert(t_block = find_block(buffer, target_position + count - 1));
  if (count <= bs - t_offset) {
    if (count <= bs - s_offset) {
      assert(s_block = find_block(buffer, source_position));
      b_memmove(t_block->data + t_offset, s_block->data + s_offset, count);
    } else {
      assert(s_offset > t_offset);
      assert(s_block = find_block(buffer, source_position + count));
      b_memmove(t_block->data + tr_offset - sr_offset, s_block->data,
                sr_offset);
      assert(s_block = find_block(buffer, source_position));
      b_memmove(t_block->data + t_offset, s_block->data + s_offset,
                count - sr_offset);
    }
    return count;
  } else {
    long r = tr_offset;
    if (tr_offset >= sr_offset) {
      if (sr_offset) {
        assert(s_block = find_block(buffer, source_position + count));
        b_memmove(t_block->data + tr_offset - sr_offset,
                s_block->data, sr_offset);
      }
      if (sr_offset != tr_offset) {
        assert(s_block =
               find_block(buffer, source_position + count - tr_offset));
        b_memmove(t_block->data, s_block->data + bs - tr_offset + sr_offset,
                tr_offset - sr_offset);
      }
    } else {
      assert(s_block = find_block(buffer, source_position + count));
      b_memmove(t_block->data, s_block->data + sr_offset - tr_offset,
              tr_offset);
    }
    r += b_copy_forward(buffer, target_position, source_position, count - r);
    return r;
  }
}
/* b_copy_forward */

  void
b_clear(buffer)
  Buffer *buffer;
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
b_read_buffer_from_file(buffer, filename)
  Buffer *buffer;
  char *filename;
{
  BufferBlock *i = 0;
  long bytes_read = 0;
  char *tmp;
  int file = open(filename, O_RDONLY);
    
  assert(!buffer->read_only);
  BUFFER_CHANGED(buffer);
  if (file <= 0) return -1;
  b_clear(buffer);
  do {
    tmp = (char *)malloc(buffer->blocksize);
    if (tmp == 0) {
      close(file);
      return -1;
    }
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
      free((char *)tmp);
      break;
    }
  } while (1);
  close(file);
  return buffer->size;
}
/* b_read_buffer_from_file */

  long
b_write_buffer_to_file(buffer, filename)
  Buffer *buffer;
  char *filename;
{
  BufferBlock *i;
  long bytes_wrote = 0;
  long bs = buffer->blocksize;
  long blocks;
  int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);

  for (i = buffer->first_block, blocks = 1;
       i;
       i = i->next_block, blocks++) {
    long bytes;
    bytes = write(file,
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
b_copy_to_file(buffer, filename, position, count)
  Buffer *buffer;
  char *filename;
  long position;
  long count;
{
  char *tmp = malloc(buffer->blocksize);
  long bytes_read = 0, bytes_wrote = 0;
  int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);

  if (file < 0 || !tmp) {
    if (tmp) free((char *)tmp);
    return -1;
  }
  do {
    bytes_read =
      b_read(buffer, tmp, position,
	     (buffer->blocksize < count) ? buffer->blocksize : count);
    if (write(file, tmp, bytes_read) < 0) {
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
b_paste_from_file(buffer, filename, position)
  Buffer *buffer;
  char *filename;
  long position;
{
  char *tmp = malloc(buffer->blocksize);
  long bytes_read, bytes_wrote = 0;
  int file = open(filename, O_RDONLY);
  
  assert(!buffer->read_only);
  if (file < 0 || !tmp) {
    if (tmp) free((char *)tmp);
    return -1;
  }
  BUFFER_CHANGED(buffer);
  do {
    bytes_read = read(file, tmp, buffer->blocksize);
    if (b_insert(buffer, position, bytes_read) < 0) {
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

  long
b_no_lines(buffer)
  Buffer *buffer;
{
  return b_count_lines(buffer, 0, buffer -> size);
}
/* b_no_lines */

  long
b_goto_line(buffer, number)
  Buffer *buffer;
  long number;
{
  BufferBlock *i;
  long newlines = 0, blocks = 0;
  char *c;

  if (number == 1) return 0;
  if (buffer == last_buffer && number == last_number) return last_position;
  for (i = buffer -> first_block; i; i = i -> next_block) {
    for (c = i -> data; c < i -> data + buffer -> blocksize; c++)
      if (*c == '\n')
        if (++newlines == number - 1) {
          long block_position = blocks * buffer -> blocksize;
          last_position = block_position + (long)c - (long)(i -> data) + 1;
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

  long
b_get_linenumber(buffer, position)
  Buffer *buffer;
  long position;
{
  long number;

  if (buffer == last_buffer && position == last_position)
    return last_number;
  number = b_count_lines(buffer, 0, position - 1) - 1;
  if (number > 0) {
    last_number = number;
    last_position = position;
    last_length = -1;
    last_buffer = buffer;
  }
  return number;
}
/* b_get_linenumber */

  long
b_line_start(buffer, position)
  Buffer *buffer;
  long position;
{
  const long bs = buffer -> blocksize;
  BufferBlock *i;
  char *c;

  if (position < 0) return 0;
  if (!(i = find_block(buffer, position))) return -1;
  for (c = i -> data + position; c >= i -> data; c--)
    if (*c == '\n')
      return (position / bs) * bs + (long)c - (long)(i -> data) + 1;
  return b_line_start(buffer, (position / bs) * bs - 1);
}
/* b_line_start */

  long
b_line_end(buffer, position)
  Buffer *buffer;
  long position;
{
  const long bs = buffer -> blocksize;
  BufferBlock *i;
  char *c;

  if (position >= buffer -> size) return buffer -> size;
  if (!(i = find_block(buffer, position))) return -1;
  for (c = i -> data + position % bs; c < i -> data + bs; c++)
    if (*c == '\n')
      return (position / bs) * bs + (long)c - (long)(i -> data);
  return b_line_end(buffer, (position / bs + 1) * bs);
}
/* b_line_end */

  long
b_length_of_line(buffer, number)
  Buffer *buffer;
  long number;
{
  long position, length;

  if (buffer == last_buffer && number == last_number && last_length != -1)
    return last_length;
  position = b_goto_line(buffer, number);
  length = b_line_end(buffer, position) - position + 1;
  return last_length = length;
    /* The values of `last_position' and `last_number' have been set already
     * by `b_goto_line()'. */
}
/* b_length_of_line */

  long
b_length_of_text_block(buffer, number, count)
  Buffer *buffer;
  long number;
  long count;
{
  long start, end;

  start = b_goto_line(buffer, number);
  end = start;
  for (; count > 0 && end != buffer -> size; count--) {
    end = b_line_end(buffer, end) + 1;
  }
  return end - start;
}
/* b_length_of_text_block */

  long
b_read_line(buffer, line, number)
  Buffer *buffer;
  char *line;
  long number;
{
  long position = b_goto_line(buffer, number);
  long length = b_length_of_line(buffer, number);
  long bytes_read = b_read(buffer, line, position, length);

  if (bytes_read < length) bytes_read++;
  line[bytes_read - 1] = '\0';
  return bytes_read; 
}
/* b_read_line */

  long
b_read_text_block(buffer, target, number, count)
  Buffer *buffer;
  char *target;
  long number;
  long count;
{
  long start = b_goto_line(buffer, number);
  long end = start;
  long bytes_read;

  bytes_read = b_read(buffer, target, start,
                      b_length_of_text_block(buffer, number, count));
  if (bytes_read < end - start) bytes_read++;
  target[bytes_read - 1] = '\0';
  return bytes_read;
}
/* b_read_text_block */

  long
b_delete_line(buffer, number)
  Buffer *buffer;
  long number;
{
  long position = b_goto_line(buffer, number);
  long length = b_length_of_line(buffer, number);

  return b_delete(buffer, position, length);
}
/* b_delete_line */
  
  long
b_delete_text_block(buffer, number, count)
  Buffer *buffer;
  long number;
  long count;
{
  long start = b_goto_line(buffer, number);
  long end = start;

  for (; count > 0; count--) end = b_line_end(buffer, end) + 1;
  return b_delete(buffer, start, end - start);
}
/* b_delete_text_block */

  long
b_clear_line(buffer, number)
  Buffer *buffer;
  long number;
{
  long position = b_goto_line(buffer, number);
  long length = b_length_of_line(buffer, number);

  return b_delete(buffer, position, length ? length - 1 : 0);
}
/* b_clear_line */

  long
b_insert_text_block(buffer, source, number)
  Buffer *buffer;
  char *source;
  long number;
{
  long position, bytes_inserted;

  bytes_inserted = b_insert(buffer, position = b_goto_line(buffer, number),
                            strlen(source) + 1);
  if (bytes_inserted < 0) return -1;
  source[bytes_inserted - 1] = '\n';
  return b_write(buffer, source, position, bytes_inserted);
}
/* b_insert_text_block */

/* end of buffer.c */


/* VIM configuration: (do not delete this line)
 *
 * vim:bk:nodg:efm=%f\:%l\:%m:hid:icon:
 * vim:sw=2:sm:textwidth=79:ul=1024:wrap:
 */
