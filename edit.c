/* edit.c	8/19/1995
 */

/* Copyright (c) 1995,1996 Sascha Demetrio
 * Copyright (c) 2009, 2014 Peter Pentchev
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
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <curses.h>
#include <term.h>

#include "buffer.h"
#include "hexer.h"
#include "calc.h"
#include "edit.h"
#include "readline.h"
#include "tio.h"

int he_hex_column[] = { 12, 15, 18, 21, 24, 27, 30, 33,
                        37, 40, 43, 46, 49, 52, 55, 58 };
int he_text_column[] = { 62, 63, 64, 65, 66, 67, 68, 69,
                         70, 71, 72, 73, 74, 75, 76, 77 };

#ifndef SEEK_SET
enum seek_e { SEEK_SET, SEEK_CUR, SEEK_END };
#endif

static void he_insert_mode(struct he_s *, int, long);
static int he_clear_get(int);
static void he_set_cursor(const struct he_s *);
static void he_visual_mode(struct he_s *hedit);
static long he_get_counter(struct he_s *hedit);

Buffer *kill_buffer = 0;

const struct he_refresh_s NO_REFRESH =
  { 0, { 0 }, { -1 }, 0, 0, { { 0 } }, 0, 0};

int he_message_wait = 10;

  static char *
he_bigendian(long x)
{
  static char bigendian[4];

  bigendian[0] = (char)(x >> 24);
  bigendian[1] = (char)(x >> 16);
  bigendian[2] = (char)(x >> 8);
  bigendian[3] = (char)x;
  return bigendian;
}
/* he_bigendian */

  static long
he_bigendian_to_long(char *bigendian)
{
  long x;

  x = (unsigned char)bigendian[3]
      + ((unsigned char)bigendian[2] << 8)
      + ((unsigned char)bigendian[1] << 16)
      + ((unsigned char)bigendian[0] << 24);
  return x;
}
/* he_bigendian_to_long */

  static int
he_digit(int key)
{
  if (isdigit(key)) return key - '0';
  key = tolower(key);
  if (key >= 'a' && key <= 'f') return key - 'a' + 10;
  return -1;
}
/* he_digit */


  void
he_refresh_lines(hedit, first, last)
  struct he_s *hedit;
  int first, last;
{
  if (!hedit->refresh.flag) ++hedit->refresh.flag;
  hedit->refresh.first[hedit->refresh.parts] = first;
  hedit->refresh.last[hedit->refresh.parts] = last;
  ++hedit->refresh.parts;
  assert(hedit->refresh.parts <= HE_REFRESH_MAX_PARTS);
}
/* he_refresh_lines */

  void 
he_refresh_part(hedit, pos1, pos2)
  struct he_s *hedit;
  long pos1, pos2;
{
  long i, j, k;
  int update_f = 0;
  int lastline_f = 0;

  if (pos2 < 0) {
    pos2 = hedit->buffer->size - 1;
    ++lastline_f;
  }
  if (pos1 > pos2) i = pos1, pos1 = pos2, pos2 = i;
  j = HE_LINE(pos1 - hedit->screen_offset);
  k = lastline_f ? hx_lines - 2 : HE_LINE(pos2 - hedit->screen_offset);
  if (j < 0) {
    j = 0;
    if (k >= 0) {
      ++update_f;
      if (k > hx_lines - 2) k = hx_lines - 2;
    }
  } else {
    if (j <= hx_lines - 2) {
      ++update_f;
      if (k > hx_lines - 2) k = hx_lines - 2;
    }
  }
  if (update_f) he_refresh_lines(hedit, j, k);
}
/* he_refresh_part */

  int
he_refresh_check(hedit)
  struct he_s *hedit;
{
  return hedit->refresh.flag || he_messages;
}
/* he_refresh_check */

  void
he_refresh_screen(hedit)
  const struct he_s *hedit;
{
  he_refresh_all(current_buffer->hedit);
  he_update_screen(current_buffer->hedit);
  tio_goto_line(hx_lines - 1);
  tio_return();
  tio_clear_to_eol();
  he_set_cursor(hedit);
  tio_flush();
  window_changed = 0;
}
/* he_refresh_screen */


  void
he_free_command(command)
  struct he_command_s *command;
  /* Free the memory allocated by `command' and all the following commands
   * in the list.
   */
{
  if (!command) return;
  if (command->next_command) {
    he_free_command(command->next_command);
    command->next_command = 0;
  }
  if (command->next_subcommand) {
    he_free_command(command->next_subcommand);
    command->next_subcommand = 0;
  }
  if (command->data) free((char *)command->data);
  free((char *)command);
}
/* he_free_command */

  void
he_compound_comand(hedit, command)
  struct he_s *hedit;
  struct he_command_s *command;
{
  char *buf;
  size_t sz, pos;
#define BADD(ptr, size)	memcpy(buf + pos, (ptr), (size)); pos += (size);
#define BADDC(c)	buf[pos++] = (c);

  if (hedit->swapping) {
    /* Remember the file position of `hedit->undo.swapfile'.
     */
    struct he_command_s *i;
    long swap_position = ftell(hedit->undo.swapfile);
    int n;

    /* Write the command to the swapfile.
     */
    sz = 4 + 1 + 4 + 4;
    for (n = 0, i = command; i; ++n, i = i->next_subcommand)
      /* Count the number of subcommands and determine the size of the buffer.
       */
      sz += 1 + 4 + 4 + i->count;
    buf = alloca(sz);
    if (buf == NULL) {
      /* TODO: some kind of error indication */
      fclose(hedit->undo.swapfile);
      hedit->swapping = 0;
      return;
    }
    pos = 0;
    BADD(he_bigendian(n), 4);
    BADDC(command->again);
    for (i = command; i; i = i->next_subcommand) {
      /* Write the subcommands to the swap file.
       */
      BADDC(i->type);
      BADD(he_bigendian(i->position), 4);
      BADD(he_bigendian(i->count), 4);
      BADD(i->data, i->count);
    }
    BADD(he_bigendian(swap_position), 4);
    BADD("\0\0\0\0", 4);
    if (fwrite(buf, 1, pos, hedit->undo.swapfile) != pos ||
	fflush(hedit->undo.swapfile) == EOF ||
	fseek(hedit->undo.swapfile, -4, SEEK_CUR) == EOF) {
      /* TODO: some kind of error indication */
      fclose(hedit->undo.swapfile);
      hedit->swapping = 0;
      return;
    }
    he_free_command(command);
  } else { /* (!hedit->swapping) */
    if (!hedit->command) {
      hedit->command = command;
      if (hedit->undo.list) he_free_command(hedit->undo.list);
      hedit->undo.list = command;
    } else {
      if (hedit->command->next_command)
        he_free_command(hedit->command->next_command);
      hedit->command->next_command = command;
      command->prev_command = hedit->command;
      hedit->command = hedit->command->next_command;
    }
  } /* if */
}
/* he_compound_comand */

  void
he_subcommand(hedit, type, position, count, data)
  struct he_s *hedit;
  int type;
  long position;
  long count;
  char *data;
  /* Update the undo-list of `hedit' by inserting the given subcommand.
   * The command is *not* performed by calling `he_subcommand()'.
   * A sequence of subcommands must be terminated by calling
   * `he_subcommand(hedit, -1, 0, 0, 0)'.
   * NOTE:  Iff the position for all subcommands is the same, the `again'
   *   flag will be set to 1.
   */
{
  static struct he_command_s *command = 0;
    /* Maybe `command' will become part of `struct he_s' instead of being
     * remembered statcially.  To stay compatible `hedit' should be 
     * specified in all calls to `he_subcommand()'.
     */
  static long last_position;
  static int again;
  struct he_command_s *i;

  if (type < 0) {
    command->again = again;
    he_compound_comand(hedit, command);
    command = 0;
  } else { /* (type >= 0) */
    if (!command) {
      command = (struct he_command_s *)malloc(sizeof(*command));
      again = 1;
      last_position = position;
      i = command;
    } else {
      if (again) if (position != last_position) again = 0;
      for (i = command; i->next_subcommand; i = i->next_subcommand);
      i->next_subcommand = (struct he_command_s *)malloc(sizeof(*command));
      i = i->next_subcommand;
    }
    i->next_subcommand = 0;
    i->next_command = 0;
    i->prev_command = 0;
    i->type = type;
    i->position = position;
    i->count = count;
    i->data = data;
  }
}
/* he_subcommand */

  long
he_do_command(hedit, command)
  struct he_s *hedit;
  struct he_command_s *command;
  /* Perform the compound command `command'.  The return value is the
   * position of the last change made.
   */
{
  if (command->type) {
    /* Insert.
     */
    b_insert(hedit->buffer, command->position, command->count);
    b_write(hedit->buffer, command->data, command->position, command->count);
    he_refresh_part(hedit, command->position, -1);
  } else {
    /* Delete.
     */
    b_delete(hedit->buffer, command->position, command->count);
    he_refresh_part(hedit, command->position, -1);
  }
  if (command->next_subcommand)
    return he_do_command(hedit, command->next_subcommand);
  return command->position;
}
/* he_do_command */

  static long
he_undo_command(struct he_s *hedit, struct he_command_s *command)
  /* Undo the compound command `command'.  The return value is the position
   * of the last change made.
   */
{
  if (command->next_subcommand)
    he_undo_command(hedit, command->next_subcommand);
    /* The last subcommand of the given compound has to be undone first.
     */
  if (command->type) {
    /* An insert command, we have to delete.
     */
    b_delete(hedit->buffer, command->position, command->count);
    he_refresh_part(hedit, command->position, -1);
  } else {
    /* A delete command, we have to reinsert.
     */
    b_insert(hedit->buffer, command->position, command->count);
    b_write(hedit->buffer, command->data, command->position, command->count);
    he_refresh_part(hedit, command->position, -1);
  }
  return command->position;
}
/* he_undo_command */

  static long
he_rewind_command(struct he_s *hedit)
  /* Rewind the swap file by one compound command.  If the beginning of the
   * file is hit (the rewind failed), the return value is -1, else the
   * current position in the file is returned.
   */
{
  long position;
  char bigendian[4];

  fseek(hedit->undo.swapfile, -4, SEEK_CUR);
  if (fread(bigendian, 4, 1, hedit->undo.swapfile) < 1)
    return -1;
  position = he_bigendian_to_long(bigendian);
  if (!position) return -1;
  fseek(hedit->undo.swapfile, position, SEEK_SET);
  return position;
}
/* he_rewind_command */

  static struct he_command_s *
he_read_command(struct he_s *hedit)
  /* Read a compound command from the swap file.  If the end of the
   * swap file is hit (the read failed), the return value is 0, else
   * a pointer to the command read is returned.
   */
{
  struct he_command_s *command, *c;
  long n_subcommands;
  char bigendian[4];
  int i;

  if (fread(bigendian, 4, 1, hedit->undo.swapfile) != 1)
    return 0;
  n_subcommands = he_bigendian_to_long(bigendian);
  if (!n_subcommands) {
    fseek(hedit->undo.swapfile, -4, SEEK_CUR);
    return 0;
  }
  if (fgetc(hedit->undo.swapfile) == EOF)
    return 0;
  c = command = (struct he_command_s *)malloc(sizeof(struct he_command_s));
  for (i = 1; c; ++i) {
    int n;
    c->next_command = 0;
    c->prev_command = 0;
    n = fgetc(hedit->undo.swapfile);
    if (n == EOF)
      return 0;
    c->type = !!n;
    if (fread(bigendian, 4, 1, hedit->undo.swapfile) != 1)
      return 0;
    c->position = he_bigendian_to_long(bigendian);
    if (fread(bigendian, 4, 1, hedit->undo.swapfile) != 1)
      return 0;
    c->count = he_bigendian_to_long(bigendian);
    c->data = (char *)malloc(c->count);
    if (fread(c->data, 1, c->count, hedit->undo.swapfile) != c->count)
      return 0;
    if (i < n_subcommands) {
      c->next_subcommand =
        (struct he_command_s *)malloc(sizeof(struct he_command_s));
      c = c->next_subcommand;
    } else {
      c->next_subcommand = 0;
      c = 0;
    }
  }
  fseek(hedit->undo.swapfile, 4, SEEK_CUR);
  return command;
}
/* he_read_command */

  static void
he_undo(struct he_s *hedit)
  /* Undo the last command.
   */
{
  struct he_command_s *command;

  if (hedit->swapping) {
    long position;
    position = he_rewind_command(hedit);
    if (position < 0)
      goto fail;
    else {
      command = he_read_command(hedit);
      fseek(hedit->undo.swapfile, position, SEEK_SET);
      hedit->position = he_undo_command(hedit, command);
      he_free_command(command);
      /* if this was the first command in the undo list, we'll clear the
       * modified-flag.
       */
      position = ftell(hedit->undo.swapfile);
      if (he_rewind_command(hedit) < 0)
        hedit->buffer->modified = 0;
      else
        fseek(hedit->undo.swapfile, position, SEEK_SET);
    }
  } else {
    if (!hedit->command)
      goto fail;
    else {
      hedit->position = he_undo_command(hedit, hedit->command);
      hedit->command = hedit->command->prev_command;
      if (!hedit->command) hedit->buffer->modified = 0;
    }
  }
  he_message(0, "undo");
  return;

fail:
  he_message(1, "nothing to undo");
}
/* he_undo */

  static void
he_redo(struct he_s *hedit)
  /* Redo the last undo.
   */
{
  struct he_command_s *command;

  if (hedit->swapping) {
    command = he_read_command(hedit);
    if (!command)
      goto fail;
    else {
      hedit->position = he_do_command(hedit, command);
      he_free_command(command);
    }
  } else {
    if (!hedit->command)
      command = hedit->undo.list;
    else
      command = hedit->command->next_command;
    if (!command)
      goto fail;
    else {
      hedit->command = command;
      hedit->position = he_do_command(hedit, command);
    }
  }
  he_message(0, "redo");
  return;

fail:
  he_message(1, "nothing to redo");
}
/* he_redo */

  static void
he_again(struct he_s *hedit, long position)
  /* Walk backwards through the undo list, until a command is found with
   * the `again' flag set and reperfom this command on position `position'.
   * If no such command is found, a beep is "returned".
   */
{
  struct he_command_s *command;

  if (hedit->swapping) {
    long swap_position = ftell(hedit->undo.swapfile);
    for (command = 0;;) {
      long p;
      p = he_rewind_command(hedit);
      if (p < 0) {
        command = 0;
        break;
      }
      if (command) he_free_command(command);
      command = he_read_command(hedit);
      if (command->again) break;
    }
    if (!command)
      tio_bell();
    else {
      struct he_command_s *i;
      for (i = command; i; i = i->next_subcommand) i->position = position;
      he_do_command(hedit, command);
      he_compound_comand(hedit, command);
    }
    fseek(hedit->undo.swapfile, swap_position, SEEK_SET);
  } else {
    struct he_command_s *i;
    for (i = hedit->undo.list, command = 0; i; i = i->next_command)
      if (i->again) command = i;
    if (!command)
      tio_bell();
    else {
      /* We have to copy the command, because we want to change the
       * positions.  If `command' is an insert command, the field `data'
       * will be copied to `command2'.  If `command' is a delete command,
       * we'll read the `data' field from the buffer.  In this case it may
       * be nesseccary to adjust `j->count' in `command2'.
       */
      struct he_command_s *command2, *j;
      command2 = (struct he_command_s *)malloc(sizeof(struct he_command_s));
      for (i = command, j = command2; i;) {
        *j = *i;
        j->next_command = 0;
        j->prev_command = 0;
        if (i->count) {
          j->data = (char *)malloc(i->count);
          if (i->type) /* insert */
            memcpy(j->data, i->data, i->count);
          else { /* delete */
            long x = b_read(hedit->buffer, j->data, position, j->count);
            if (x < j->count) {
              /* `x > j->count' is impossible here. */
              j->data = (char *)realloc(j->data, x);
              j->count = x;
            }
          }
        }
        if (i->next_subcommand) {
          j->next_subcommand =
            (struct he_command_s *)malloc(sizeof(struct he_command_s));
          j = j->next_subcommand;
          i = i->next_subcommand;
        } else
          i = j->next_subcommand = 0;
      }
      for (i = command2; i; i = i->next_subcommand) i->position = position;
      he_do_command(hedit, command2);
      he_compound_comand(hedit, command2);
      /* The memory allocated by `command2' will not be freed here,
       * because it has been inserted into the undo list.
       */
    }
  }
}
/* he_again */

  static void
he_delete(struct he_s *hedit, long count, int dont_save)
  /* If `dont_save == 1' the data deleted is not copied to the `kill_buffer'.
   */
{
  long start;
  char *data;

  if (hedit->begin_selection >= 0 &&
      hedit->end_selection >= hedit->begin_selection) {
    start = hedit->begin_selection;
    count = hedit->end_selection - start + 1;
    he_cancel_selection(hedit);
    he_update_screen(hedit);
  } else {
    start = hedit->position;
    if (count < 1) count = 1;
    if (start + count > hedit->buffer->size)
      count = hedit->buffer->size - start;
  }
  if (!count) return;
  data = (char *)malloc(count);
  b_read(hedit->buffer, data, start, count);
  if (!dont_save) {
    if (!kill_buffer)
      kill_buffer = new_buffer(0);
    else
      b_clear(kill_buffer);
    b_insert(kill_buffer, 0, count);
    b_write(kill_buffer, data, 0, count);
  }
  he_subcommand(hedit, 0, start, count, data);
  he_subcommand(hedit, -1, 0, 0, 0);
  b_delete(hedit->buffer, start, count);
  if (hedit->position > start) hedit->position -= count - 1;
  he_refresh_part(hedit, start, hedit->buffer->size + count - 1);

  if (count > 1)
    he_message(0, "0x%lx (%li) bytes deleted%s.", count, count,
               dont_save ? "" : " (cut)");
}
/* he_delete */

  static void
he_paste(struct he_s *hedit, long count)
{
  char *data;
  long x = count > 2 ? count : 1, c = -1;

  he_cancel_selection(hedit);
  if (kill_buffer ? !(c = kill_buffer->size) : 1) return;
  if (count < 1) count = 1;
  while (count--) {
    data = (char *)malloc(c);
    b_read(kill_buffer, data, 0, c);
    b_insert(hedit->buffer, hedit->position, c);
    b_write(hedit->buffer, data, hedit->position, c);
    he_subcommand(hedit, 1, hedit->position, c, data);
  }
  he_subcommand(hedit, -1, 0, 0, 0);
  he_refresh_part(hedit, hedit->position, -1);
  he_message(0, "0x%lx (%li) bytes pasted.", x * c, x * c);
}
/* he_paste */

  static void
he_paste_over(struct he_s *hedit, long count)
{
  char *data_insert, *data_delete;
  long x = count > 2 ? count : 1, c = -1;
  long bytes_deleted;
  int i;

  he_cancel_selection(hedit);
  if (kill_buffer ? !(c = kill_buffer->size) : 1) return;
  if (count < 1) count = 1;

  data_insert = (char *)malloc(c * count);
  data_delete = (char *)malloc(c * count);
  b_read(kill_buffer, data_insert, 0, c);
  for (i = 1; i < count; ++i) memcpy(data_insert + c * i, data_insert, c);
  bytes_deleted = b_read(hedit->buffer, data_delete,
                         hedit->position, c * count);
  b_write_append(hedit->buffer, data_insert, hedit->position, c * count);
  if (bytes_deleted < c * count)
    data_delete =
      (char *)realloc(data_delete, bytes_deleted + !bytes_deleted);
  he_subcommand(hedit, 0, hedit->position, bytes_deleted, data_delete);
  he_subcommand(hedit, 1, hedit->position, c * count, data_insert);
  he_subcommand(hedit, -1, 0, 0, 0);
  he_refresh_part(hedit, hedit->position, -1);
  he_message(0, "0x%lx (%li) bytes replaced.", x * c, x * c);
}
/* he_paste_over */

  static void
he_yank(struct he_s *hedit, long count)
{
  long start;

  if (kill_buffer) b_clear(kill_buffer); else kill_buffer = new_buffer(0);
  if (hedit->begin_selection >= 0 &&
      hedit->end_selection >= hedit->begin_selection) {
    start = hedit->begin_selection;
    count = hedit->end_selection - start + 1;
    he_cancel_selection(hedit);
  } else {
    start = hedit->position;
    if (count < 1) count = 1;
  }
  b_insert(kill_buffer, 0, count);
  b_copy(kill_buffer, hedit->buffer, 0, start, count);
  if (count > 1)
    he_message(0, "0x%lx (%li) bytes yanked.", count, count);
}
/* he_yank */

  static char *
he_line(const struct he_s *hedit, long position)
  /* Create a line suitable for `tio_display()' (including @-commands).
   * `position' ist the number of the first byte in the line to be
   * created.  If `hedit->insert_position' is a positive value and
   * placed in the created line, the place for this position will be left
   * blank in the hex view and replaced by a `?' in the text view.
   * The return value is a pointer to the created line.
   */
{
  static char line[256];
  char buf[16];
  char *p = line;
  int i, bytes_read;
  int inverse = 0;

  if (position > (long)hedit->buffer->size) {
    strcpy(line, "~@C");
    return line;
  }
  if (position >= 0) {
    sprintf(p, " %08lx:", position);
    bytes_read = b_read(hedit->buffer, buf, position, 16);
  } else {
    sprintf(p, "-%08lx:", -position);
    bytes_read = b_read(hedit->buffer, buf - position, 0, 16 + position)
                 - position;
  }
  p += strlen(p);
  for (i = 0; i < 16; ++i) {
    if (hedit->begin_selection >= 0
        && position + i > hedit->end_selection
        && inverse) {
      strcpy(p, "@~");
      p += 2;
      inverse = 0;
    }
    if (!(i % 8)) {
      if (inverse) {
        strcpy(p, "@~  ");
        p += 4;
      } else {
        *p++ = ' ';
        *p++ = ' ';
      }
      inverse = 0;
    } else
      *p++ = ' ';
    if (hedit->begin_selection >= 0
        && position + i >= hedit->begin_selection
        && position + i <= hedit->end_selection
        && hedit->end_selection >= hedit->begin_selection) {
      if (!inverse) {
        strcpy(p, "@Ar");
        p += 3;
        inverse = 1;
      }
    } else
      if (inverse) {
        strcpy(p, "@~");
        p += 2;
        inverse = 0;
      }
    if (position + i < 0 || i >= bytes_read)
      strcpy(p, "--");
    else if (hedit->insert_position >= 0
             && position + i == hedit->insert_position)
      sprintf(p, "  ");
#ifdef HE_UNDERLINE_BEGIN_SELECTION
    else if (position + i == hedit->begin_selection) {
      sprintf(p, "@U%02x@~", (unsigned char)buf[i]);
      if (inverse) {
        p += strlen(p);
        sprintf(p, "@Ar");
      }
    }
#endif
    else 
      sprintf(p, "%02x", (unsigned char)buf[i]);
    p += strlen(p);
  }
  if (inverse) {
    strcpy(p, "@~  ");
    p += 4;
  } else {
    *p++ = ' ';
    *p++ = ' ';
  }
  inverse = 0;
  for (i = 0; i < 16; ++i) {
    if (hedit->begin_selection >= 0
        && position + i >= hedit->begin_selection
        && position + i <= hedit->end_selection
        && hedit->end_selection >= hedit->begin_selection) {
      if (!inverse) {
        strcpy(p, "@Ar");
        p += 3;
        inverse = 1;
      }
    } else
      if (inverse) {
        strcpy(p, "@~");
        p += 2;
        inverse = 0;
      }
    if (position + i < 0 || i >= bytes_read)
      *p++ = '-';
    else if (hedit->insert_position >= 0
             && position + i == hedit->insert_position)
      *p++ = '?';
#ifdef HE_UNDERLINE_BEGIN_SELECTION
    else if (position + i == hedit->begin_selection) {
      sprintf(p, "@U%s%c@~", buf[i] == '@' ? "@" : "",
              tio_isprint(buf[i]) ? buf[i] : HE_ANYCHAR);
      p += strlen(p);
      if (inverse) { strcpy(p, "@Ar"); p += 3; }
    }
#endif
    else {
      if (buf[i] == '@') *p++ = '@';
      *p++ = tio_isprint(buf[i]) ? buf[i] : HE_ANYCHAR;
    }
  }
  if (inverse) {
    strcpy(p, "@~");
    p += 2;
  }
  *p = 0;
  return line;
}
/* he_line */

  static void
he_set_cursor(hedit)
  const struct he_s *hedit;
{
  int l, c, i;

  l = HE_LINE(hedit->position - hedit->screen_offset);
  if (l > hx_lines - 2) {
    tio_goto_line(hx_lines - 1);
    tio_return();
    return;
  }
  i = (hedit->position - hedit->screen_offset) % 16;
  c = hedit->text_mode ? he_text_column[i] : he_hex_column[i];
  tio_move(l, c);
}
/* he_set_cursor */

  static void
he_display(const struct he_s *hedit, int start, int end)
{
  int i;

  if (end < 0) end = hx_lines - 2;
  if (start > end) return;
  for (i = start; i <= end; ++i) {
    tio_goto_line(i);
    tio_return();
    tio_display(he_line(hedit, hedit->screen_offset + 16 * i), 0);
    tio_clear_to_eol();
  }
  he_set_cursor(hedit);
}
/* he_display */

  static void
he_motion(struct he_s *hedit, int key, long count)
{
  int i;

  if (count <= 0) count = 1;
  switch (key) {
  case 'k':
  case HXKEY_UP:
    hedit->position -= count << 4;
    if (hedit->position < 0) hedit->position &= 0xf;
    break;
  case 'j':
  case HXKEY_DOWN:
    hedit->position += count << 4;
    if (hedit->position > (long)hedit->buffer->size)
      hedit->position = (long)hedit->buffer->size;
    break;
  case 'h':
  case HXKEY_LEFT:
    hedit->position -= count;
    if (hedit->position < 0) hedit->position = 0;
    break;
  case 'l':
  case HXKEY_RIGHT:
    hedit->position += count;
    if (hedit->position > hedit->buffer->size)
      hedit->position = hedit->buffer->size;
    break;
  case 'f' & 0x1f: /* C-f */
  case HXKEY_PAGE_DOWN:
    i = hedit->position;
    hedit->position += (hx_lines - 2) * 16;
    if (hedit->position > hedit->buffer->size)
      hedit->position = hedit->buffer->size;
    hedit->screen_offset += ((hedit->position - i) / 16) * 16;
    if (hedit->position != i) he_refresh_all(hedit);
    break;
  case 'b' & 0x1f: /* C-b */
  case HXKEY_PAGE_UP:
    i = hedit->position;
    hedit->position -= (hx_lines - 2) * 16;
    if (hedit->position < 0) hedit->position = 0;
    hedit->screen_offset -= ((i - hedit->position) / 16) * 16;
    while (hedit->screen_offset < -15) hedit->screen_offset += 16;
    if (hedit->position != i) he_refresh_all(hedit);
    break;
  case 'd' & 0x1f: /* C-d */
    i = hedit->position;
    hedit->position += ((hx_lines - 1) / 2) * 16;
    if (hedit->position > hedit->buffer->size)
      hedit->position = hedit->buffer->size;
    hedit->screen_offset += ((hedit->position - i) / 16) * 16;
    if (hedit->position != i) he_refresh_all(hedit);
    break;
  case 'u' & 0x1f: /* C-u */
    i = hedit->position;
    hedit->position -= ((hx_lines - 1) / 2) * 16;
    if (hedit->position < 0) hedit->position = 0;
    hedit->screen_offset -= ((i - hedit->position) / 16) * 16;
    while (hedit->screen_offset < -15) hedit->screen_offset += 16;
    if (hedit->position != i) he_refresh_all(hedit);
    break;
  case '>':
    count &= 0xf;
    while (count--) {
      --hedit->screen_offset;
      if (hedit->screen_offset < -15) hedit->screen_offset = 0;
    }
    he_refresh_part(hedit, 0, -1);
    break;
  case '<':
    count &= 0xf;
    while (count--) ++hedit->screen_offset;
    he_refresh_part(hedit, 0, -1);
    break;
  case HXKEY_TAB:
    hedit->text_mode = !hedit->text_mode;
    break;
  }
}
/* he_motion */

#define HE_CASE_MOTION case HXKEY_UP: case HXKEY_DOWN: case HXKEY_LEFT:            \
  case HXKEY_RIGHT: case 'f' & 0x1f: case 'b' & 0x1f: case 'u' & 0x1f:         \
  case 'd' & 0x1f: case HXKEY_PAGE_UP: case HXKEY_PAGE_DOWN: case HXKEY_TAB
#define HE_CASE_MOTION_HJKL case 'h': case 'j': case 'k': case 'l'
#define HE_CASE_MOTION_SHIFT case '<': case '>'

  static void
he_begin_selection(struct he_s *hedit)
  /* Set the beginning of the selected text to the current position in the
   * file.
   */
{
  if (hedit->begin_selection >= 0
      && hedit->end_selection >= hedit->begin_selection)
    he_refresh_part(hedit, hedit->begin_selection, hedit->position);
  else
    he_refresh_part(hedit, hedit->end_selection, hedit->position);
  hedit->begin_selection = hedit->position;
}
/* he_begin_selection */

  static void
he_end_selection(struct he_s *hedit)
  /* Set the end of the selected text to the current position in the
   * buffer.
   */
{
  if (hedit->end_selection >= 0
      && hedit->end_selection >= hedit->begin_selection)
    he_refresh_part(hedit, hedit->position, hedit->end_selection);
  else
    he_refresh_part(hedit, hedit->begin_selection, hedit->position);
  hedit->end_selection = hedit->position;
}
/* he_end_selection */

  void
he_cancel_selection(hedit)
  struct he_s *hedit;
  /* Cancel the current selection.
   */
{
  if (hedit->begin_selection >= 0) {
    if (hedit->end_selection >= hedit->begin_selection)
      he_refresh_part(hedit, hedit->begin_selection, hedit->end_selection);
    else
      he_refresh_part(hedit, hedit->begin_selection, hedit->begin_selection);
  }
  hedit->begin_selection = -1;
  hedit->end_selection = -1;
  hedit->anchor_selection = 0;
}
/* he_cancel_selection */

  void
he_select(hedit, begin, end)
  struct he_s *hedit;
  long begin, end;
{
  if (hedit->begin_selection >= 0) he_cancel_selection(hedit);
  assert(begin <= end);
  he_refresh_part(hedit, begin, end);
  hedit->begin_selection = begin;
  hedit->end_selection = end;
}
/* he_select */

  static int
he_command(struct he_s *hedit, int key, long count)
  /* Execute a command-mode command.  If `key' is an unknown command,
   * the return value is 0.  If `key' is a quit command, the return value
   * is -1.  In all other cases the return value is 1.
   */
{
  int rval = 1;
  int redisplay = 0;
  char *cmd;

  he_update_screen(hedit);
  switch (key) {
  case 'i': /* enter insert mode */
    he_cancel_selection(hedit);
    he_message(0, "insert at position 0x%lx (%li)", hedit->position,
               hedit->position);
    he_insert_mode(hedit, 0, count);
    break;
  case 'R': /* enter replace mode */
    he_cancel_selection(hedit);
    he_message(0, "replace at position 0x%lx (%li)",
               hedit->position, hedit->position);
    he_insert_mode(hedit, 1, count);
    break;
  case 'r': /* replace `count' characters */
    if (hedit->begin_selection >= 0
        && hedit->end_selection >= hedit->begin_selection) {
      hedit->position = hedit->begin_selection;
      he_update_screen(hedit);
      count = hedit->end_selection - hedit->begin_selection + 1;
    }
    if (count > 0)
      he_message(0, "replace 0x%lx (%li) bytes at position 0x%lx (%li)",
                 count, count, hedit->position, hedit->position);
    else
      he_message(0, "replace byte at position 0x%lx (%li)",
                 hedit->position, hedit->position);
    he_insert_mode(hedit, 2, count);
    hedit->begin_selection = -1;
    break;
  case 'u': /* undo */
    if (count < 0) count = 1;
    while (--count) {
      he_undo(hedit);
      he_update_screen(hedit);
    }
    he_undo(hedit);
    break;
  case 'r' & 0x1f: /* C-r, redo */
    if (count < 0) count = 1;
    while (--count) {
      he_redo(hedit);
      he_update_screen(hedit);
    }
    he_redo(hedit);
    break;
  case 'v': /* enter visual mode for selecting text */
    he_visual_mode(hedit);
    break;
  case 'G': /* goto */
    if (count < 0)
      hedit->position = hedit->buffer->size;
    else {
      if (count > hedit->buffer->size) {
        count = hedit->buffer->size;
        he_message(0, "only 0x%lx (%li) bytes", count, count);
      }
      hedit->position = count;
    }
    break;
  case 'x' & 0x1f: /* C-x, delete (cut) */
    he_delete(hedit, count, 0);
    break;
  case 'x': /* delete */
    he_delete(hedit, count, 1);
    break;
  case 'y': /* yank */
    he_yank(hedit, count);
    break;
  case 'o' & 0x1f: /* paste over */
    he_paste_over(hedit, count);
    break;
  case 'p': /* paste */
    he_paste(hedit, count);
    break;
  case '.': /* again */
    he_again(hedit, hedit->position);
    break;
  case 'g' & 0x1f: /* C-g */
    he_status_message(1);
    break;
  case '/':  /* regex search forward. */
    cmd = he_query_command("/", "", 2);
    redisplay = rl_redisplay;
    if (cmd) he_search_command(hedit, cmd, 1);
    break;
  case '?':  /* regex search backward. */
    cmd = he_query_command("?", "", 2);
    if (cmd) he_search_command(hedit, cmd, -1);
    break;
  case '%':  /* calculator. */
    cmd = he_query_command("%", "", 3);
    he_message(0, "calc: %s", calculator(cmd));
    redisplay = rl_redisplay;
    break;
  case 'l' & 0x1f: /* C-l */
    he_refresh_screen(hedit);
    break;
  default:
    rval = 0;
  }
  if (redisplay) he_refresh_all(hedit);

  return rval;
}
/* he_command */

#define _HE_CASE_COMMAND_NO_V case 'i': case 'r': case 'R':                   \
  case 'u': case 'r' & 0x1f: case 'G': case 'd': case 'y': case 'p':          \
  case '.': case 'x': case 'x' & 0x1f: case 'o' & 0x1f: case '/':             \
  case '?': case '%'
  /* `_HE_CASE_COMMAND_NO_V' is needed only by `he_visual_mode()'.
   */
#define HE_CASE_COMMAND _HE_CASE_COMMAND_NO_V: case 'v'
#define HE_CASE_COMMAND_INSERT case 'l' & 0x1f: case 'g' & 0x1f
  /* Commands that should be availabe in insert mode.
   */

  static void
he_visual_mode(hedit)
  struct he_s *hedit;
{
  int key;
  long anchor = hedit->position;
  long last_position = hedit->position;
  long count = -1;
  int f = 0;
  char map_string[256];
  int motion_f = 0;

  if (!hedit->buffer->size) {
    he_message(1, "@Abthe buffer is empty@~");
    return;
  }
  if (hedit->position == hedit->buffer->size) {
    --hedit->position;
    he_update_screen(hedit);
  }
  tio_set_cursor(0); /* cursor invisible. */
  if (hedit->begin_selection >= 0
      && hedit->end_selection >= hedit->begin_selection) {
    if (hedit->anchor_selection) {
      last_position = hedit->position = hedit->begin_selection;
      anchor = hedit->end_selection;
    } else {
      last_position = hedit->position = hedit->end_selection;
      anchor = hedit->begin_selection;
    }
    motion_f = 1;
  } else {
    he_begin_selection(hedit);
    he_end_selection(hedit);
    he_message(0, "visual");
  }
  tio_ungetch(0);
  for (;;) {
    key = tio_mgetch(MAP_VISUAL, map_string);
    switch (key) {
    _HE_CASE_COMMAND_NO_V:
    HE_CASE_COMMAND_INSERT:
      he_command(hedit, key, count);
      /* If the command cleared the selection, we'll leave the visual
       * mode.
       */
      if (hedit->begin_selection < 0) goto exit_visual_mode;
      /* fall through */
    HE_CASE_MOTION:
    HE_CASE_MOTION_HJKL:
      he_motion(hedit, key, count);
      if (hedit->position == hedit->buffer->size) --hedit->position;
      he_update_screen(hedit);
      if (hedit->position == last_position) break;
      if (anchor > hedit->position) {
        if (anchor < last_position) {
          long x = hedit->position;
          hedit->position = anchor;
          he_end_selection(hedit);
          hedit->position = x;
          he_update_screen(hedit);
        }
        he_begin_selection(hedit);
      } else if (anchor == hedit->position) {
        he_cancel_selection(hedit);
        he_begin_selection(hedit);
        he_end_selection(hedit);
      } else {
        if (anchor > last_position) he_select(hedit, anchor, anchor);
        he_end_selection(hedit);
      }
      last_position = hedit->position;
      motion_f = 1;
      break;
    HE_CASE_MOTION_SHIFT:
      he_motion(hedit, key, count);
      break;
    case ':':
      tio_ungetch(':');
      hedit->anchor_selection = hedit->begin_selection == hedit->position;
      goto exit_visual_mode2;
    case 'v':
    case HXKEY_ESCAPE:
      goto exit_visual_mode;
    case HXKEY_ERROR:
      if (window_changed) he_refresh_screen(hedit);
      if (strlen(map_string)) {
        tio_move(hx_lines, 40);
        tio_raw_printf("%s", map_string);
      }
      break;
    default:
      if (key > 0xff) break;
      if (isdigit(key)) {
        tio_ungetch(key);
        count = he_get_counter(hedit);
        continue;
      }
    }
    he_clear_get(1);
    count = -1;
    he_update_screen(hedit);
    tio_goto_line(hx_lines - 1);
    tio_return();
    if (motion_f) {
      motion_f = 0;
      tio_clear_to_eol();
      tio_printf(" visual selection:  0x%08lx - 0x%08lx  0x%lx (%li) bytes@r",
                 hedit->begin_selection, hedit->end_selection,
                 hedit->end_selection - hedit->begin_selection + 1,
                 hedit->end_selection - hedit->begin_selection + 1);
    }
    tio_flush();
  }

exit_visual_mode:
  he_cancel_selection(hedit);
exit_visual_mode2:
  tio_set_cursor(1); /* normal cursor. */
  f = hedit->refresh.message_f;
  he_update_screen(hedit);
  if (!f) he_message(0, "");
}
/* he_visual_mode */

  static long
he_get_counter(hedit)
  struct he_s *hedit;
  /* Read a counter from the keyboard.  If this is terminated by pressing
   * <esc>, the return value is -1.  If another non-digit key is read,
   * it is put back via `tio_ungetch()'.  Reading the counter can be
   * terminated by pressing <return>.
   */
{
  long count = -1;
  int key, digit;
  enum mode_e { OCT = 8, DEC = 10, HEX = 16 } mode = DEC;
  char *fmt = 0, *prefix = 0;

  tio_goto_line(hx_lines - 1);
  tio_return();
  tio_clear_to_eol();
  for (;;) {
    key = tio_mgetch(0, 0);
    switch (key) {
    case HXKEY_ERROR:
      if (window_changed) he_refresh_screen(hedit);
      break;
    HE_CASE_COMMAND_INSERT:
      /* The commands that are availabe in insert mode do not affect and
       * are not affected by the counter.
       */
      he_command(hedit, key, 0);
      break;
    case HXKEY_BACKSPACE:
    case HXKEY_DELETE:
      if (count > 0)
        count /= (long)mode;
      else {
        count = -1;
        switch (mode) {
        case DEC: goto exit_get_counter;
        case OCT: mode = DEC; break;
        case HEX: mode = OCT;
        }
      }
      break;
    case HXKEY_RETURN:
      goto exit_get_counter;
    default:
      if (key == '0' && count < 0) {
        mode = OCT;
        break;
      }
      if (key == 'x' && count < 0 && mode == OCT) {
        mode = HEX;
        break;
      }
      digit = he_digit(key);
      if (digit >= 0 && digit < (int)mode) {
        if (count < 0) count = 0;
        count *= (long)mode;
        count += digit;
      } else {
        tio_ungetch(key);
        if (mode == OCT && count < 0) count = 0;
        goto exit_get_counter;
      }
    }
    switch (mode) {
    case DEC: fmt = "arg:%li"; prefix = "arg:"; break;
    case OCT: fmt = "arg:0%lo"; prefix = "arg:0"; break;
    case HEX: fmt = "arg:0x%lx"; prefix = "arg:0x";
    }
    if (count > 0) {
      char arg[256];
      int indent = 60;
      sprintf(arg, fmt, count);
      tio_goto_line(hx_lines - 1);
      if (indent + strlen(arg) > hx_columns - 1)
        indent = hx_columns - 1 - strlen(arg);
      if (indent < 0) indent = 0;
      tio_return();
      tio_right(indent);
      tio_printf(arg);
      tio_clear_to_eol();
    } else {
      int indent = 60;
      tio_goto_line(hx_lines - 1);
      if (indent + strlen(prefix) > hx_columns - 1)
        indent = hx_columns - 1 - strlen(prefix);
      if (indent < 0) indent = 0;
      tio_return();
      tio_right(indent);
      tio_printf(prefix);
      tio_clear_to_eol();
    }
    tio_flush();
  }

exit_get_counter:
  tio_goto_line(hx_lines - 1);
  tio_return();
  tio_clear_to_eol();
  he_set_cursor(hedit);
  return count;
}
/* he_get_counter */

  static int
he_verbatim(struct he_s *hedit)
{
  int key;

  tio_goto_line(hx_lines - 1);
  tio_return();
  tio_right(60);
  tio_clear_to_eol();
  tio_printf("verbatim");
  he_set_cursor(hedit);
restart:
  if ((key = tio_mgetch(0, 0)) == (int)HXKEY_ERROR) {
    if (window_changed) {
      tio_ungetch('v' & 0x1f);
      tio_ungetch(HXKEY_ERROR);
      tio_goto_line(hx_lines - 1);
      tio_return();
      tio_right(60);
      tio_clear_to_eol();
      return (int)HXKEY_NONE;
    } else
      goto restart;
  }
  tio_goto_line(hx_lines - 1);
  tio_return();
  tio_right(60);
  tio_clear_to_eol();
  return key < 0x100 ? key : (int)HXKEY_NONE;
}
/* he_verbatim */

  static void
he_insert_mode(hedit, replace_mode, count)
  struct he_s *hedit;
  int replace_mode;
  long count;
  /* `replace_mode == 0':  Normal insert mode.
   * `replace_mode == 1':  Replace mode.  Characters are appended if the
   *                       end of the buffer is reached.
   * `replace_mode == 2':  Replace a single character.
   */
{
  Buffer *insert = new_buffer(0);
  Buffer *replace = new_buffer(0);
  char *data;
  int insert_state = 0;
  int insert_write = 0;
  unsigned char hx_insert_character;
  long position = hedit->position;
  long size = hedit->buffer->size;
  long i;
  int key, x = 0;
  char map_string[256];

  if (!count) count = 1;
  if (he_refresh_check(hedit)) tio_ungetch(HXKEY_NONE);
  for (;;) {
    key = tio_mgetch(MAP_INSERT, map_string);
    switch (key) {
    HE_CASE_MOTION:
      if (!(count > 2 && replace_mode == 2) && !insert_state) {
        i = hedit->position;
        he_motion(hedit, key, count);
        if (i != hedit->position) count = 1;
          /* Moving the cursor eliminates the counter.
           */
        if (i != hedit->position && insert->size) {
          data = (char *)malloc(insert->size);
          b_read(insert, data, 0, insert->size);
          if (replace_mode && replace->size) {
            char *data_replace;
            data_replace = (char *)malloc(replace->size);
            b_read(replace, data_replace, 0, replace->size);
            he_subcommand(hedit, 0, position, replace->size, data_replace);
            b_clear(replace);
          }
          he_subcommand(hedit, 1, position, insert->size, data);
          he_subcommand(hedit, -1, 0, 0, 0);
          b_clear(insert);
        }
        position = hedit->position;
      }
      break;
    HE_CASE_COMMAND_INSERT:
      he_command(hedit, key, -1);
      break;
    case HXKEY_ERROR:
      if (window_changed) he_refresh_screen(hedit);
      if (strlen(map_string)) {
        tio_move(hx_lines, 40);
        tio_raw_printf("%s", map_string);
      }
      break;
    case HXKEY_BACKSPACE:
    case HXKEY_DELETE:
      if (replace_mode)
        if (hedit->text_mode)
          if (insert->size) {
            --hedit->position;
            if (hedit->position >= size)
              b_set_size(hedit->buffer, hedit->position);
            else {
              b_copy(hedit->buffer, replace,
                     hedit->position, replace->size - 1, 1);
              b_set_size(replace, replace->size - 1);
            }
            b_set_size(insert, insert->size - 1);
            he_refresh_part(hedit, hedit->position, hedit->position);
          } else
            tio_bell();
        else /* (!hedit->text_mode) */
          if (insert_state) {
            insert_state = 0;
            hedit->insert_position = -1;
            b_copy(hedit->buffer, replace,
                   hedit->position, replace->size - 1, 1);
            if (replace->size > 0)
                b_set_size(replace, replace->size - 1);
            if (insert->size > 0)
                b_set_size(insert, insert->size - 1);
            he_refresh_part(hedit, hedit->position, hedit->position);
          } else
            if (insert->size) {
              insert_state = 1;
              b_read(insert, (char *)&hx_insert_character, insert->size - 1, 1);
              hx_insert_character &= (char)0xf0;
              x = (unsigned char)hx_insert_character;
              key = (unsigned char)hx_insert_character >> 4;
              --hedit->position;
              hedit->insert_position = hedit->position;
              he_refresh_part(hedit, hedit->position, hedit->position);
            } else
              tio_bell();
      else /* (!replace_mode) */
        if (hedit->text_mode)
          if (insert->size) {
            --hedit->position;
            b_delete(hedit->buffer, hedit->position, 1);
            b_set_size(insert, insert->size - 1);
            he_refresh_part(hedit, hedit->position, hedit->buffer->size - 1);
          } else
            tio_bell();
        else
          if (insert_state) {
            insert_state = 0;
            hedit->insert_position = -1;
            b_delete(hedit->buffer, hedit->position, 1);
            if (insert->size > 0)
              b_set_size(insert, insert->size - 1);
            he_refresh_part(hedit, hedit->position, hedit->buffer->size - 1);
          } else
            if (insert->size) {
              insert_state = 1;
              b_read(insert, (char *)&hx_insert_character, insert->size - 1, 1);
              hx_insert_character &= (char)0xf0;
              x = (unsigned char)hx_insert_character >> 4;
              --hedit->position;
              hedit->insert_position = hedit->position;
              he_refresh_part(hedit, hedit->position, hedit->position);
            } else
              tio_bell();
      break;
    case HXKEY_ESCAPE:
      goto hx_exit_insert_mode;
        /* The refresh has to be performed by the calling function.
         */
    case 'v' & 0x1f: /* C-v */
      if (hedit->text_mode) key = he_verbatim(hedit);
      goto Default;
    case HXKEY_RETURN:
      if (key == (int)HXKEY_RETURN) key = 0x0a;
      /* fall through */
    default: Default:
      if (!key || key > 0xff) break;
      if (key == (int)HXKEY_NULL) key = 0;
      insert_write = 0;
      if (hedit->text_mode) {
        hx_insert_character = (unsigned char)key;
        if (!replace_mode) {
          b_insert(hedit->buffer, hedit->position, 1);
          he_refresh_part(hedit, hedit->position, hedit->buffer->size - 1);
        } else
          he_refresh_part(hedit, hedit->position, hedit->position);
        insert_write = 1;
      } else {
        x = (unsigned char)tolower(key);
        if (isdigit(x) || (x >= 'a' && x <= 'f')) {
          x -= isdigit(x) ? '0' : ('a' - 0xa);
          if (!insert_state) {
            hx_insert_character = (unsigned char)x << 4;
            if (!replace_mode) {
              b_insert(hedit->buffer, hedit->position, 1);
              hedit->insert_position = hedit->position;
              he_refresh_part(hedit, hedit->position, hedit->buffer->size - 1);
            } else
              he_refresh_part(hedit, hedit->position, hedit->position);
          } else {
            hx_insert_character |= (unsigned char)x;
            insert_write = 1;
            hedit->insert_position = -1;
            he_refresh_part(hedit, hedit->position, hedit->position);
          }
          insert_state = !insert_state;
        } else
          continue;
      }
      if (insert_write) {
        if (replace_mode) {
          if (hedit->position < size) {
            char c;
            b_read(hedit->buffer, &c, hedit->position, 1);
            b_append(replace, &c, 1);
          }
        }
        b_write_append(hedit->buffer, (char *)&hx_insert_character, hedit->position, 1);
        b_append(insert, (char *)&hx_insert_character, 1);
        if (replace_mode == 2) goto hx_exit_insert_mode;
        ++hedit->position;
      }
    } /* switch */
    if (he_clear_get(0)) continue;
    if (!he_update_screen(hedit) && replace_mode != 2) {
      if (replace_mode)
        he_message(0, "replace");
      else
        he_message(0, "insert");
      he_update_screen(hedit);
    }
    if (insert_state) tio_printf("%x", (unsigned)x);
  } /* for */

hx_exit_insert_mode:
  if (insert->size) {
    char *data_replace;
    long c;
    if (count > 1) {
      /* If there's a counter > 1, we'll undo the changes made and then
       * redo them `count' times.  This way it is easier to create a single
       * entry in the undo list.
       */
      long replace_size2 = 0;
      b_delete(hedit->buffer, position, insert->size);
      if (replace_mode) {
        b_insert(hedit->buffer, position, replace->size);
        b_copy(hedit->buffer, replace, position, 0, replace->size);
        data_replace = (char *)malloc(replace->size * count);
        replace_size2 =
          b_read(hedit->buffer, data_replace, position, replace->size * count);
        if (replace_size2 < replace->size * count)
          /* We don't want to eat up more memory than needed.
           */
          data_replace = (char *)realloc(data_replace, replace_size2);
        b_delete(hedit->buffer, position, replace_size2);
        he_subcommand(hedit, 0, position, replace_size2, data_replace);
      }
      data = (char *)malloc(insert->size);
      b_read(insert, data, 0, insert->size);
      data = (char *)realloc(data, insert->size * count);
      for (c = insert->size * (count - 1); c > 0; c -= insert->size)
        memcpy(data + c, data, insert->size);
      b_insert(hedit->buffer, position, insert->size * count);
      b_write(hedit->buffer, data, position, insert->size * count);
      he_subcommand(hedit, 1, position, insert->size * count, data);
      
      if (replace_mode) {
        he_refresh_part(hedit, hedit->position,
                        hedit->position + replace_size2);
      } else
        he_refresh_part(hedit, hedit->position, hedit->buffer->size - 1);
    } else {
      data = (char *)malloc(insert->size);
      b_read(insert, data, 0, insert->size);
      if (replace_mode)
        if (replace->size) {
          data_replace = (char *)malloc(replace->size);
          b_read(replace, data_replace, 0, replace->size);
          he_subcommand(hedit, 0, position, replace->size, data_replace);
        }
      he_subcommand(hedit, 1, position, insert->size, data);
      if (insert_state && !replace_mode) {
        b_delete(hedit->buffer, hedit->position, 1);
        hedit->insert_position = -1;
        he_refresh_part(hedit, hedit->position, hedit->buffer->size - 1);
      } else
        he_refresh_part(hedit, hedit->position, hedit->position);
    }
    he_subcommand(hedit, -1, 0, 0, 0);
  }
  delete_buffer(insert);
  delete_buffer(replace);
  tio_goto_line(hx_lines - 1);
  tio_return();
  tio_clear_to_eol();
}
/* he_insert_mode */

  void
he_scroll_down(hedit, count)
  struct he_s *hedit;
  int count;
{
  if (!count) return;
  assert(count > 0);
  while (count--) {
    if (hedit->position >= hedit->screen_offset + (hx_lines - 2) * 16) break;
    if (hedit->screen_offset < 16) break;
    hedit->screen_offset -= 16;
    if (!hedit->refresh.flag) {
      if (tio_scroll_down(1, 0, hx_lines - 2)) {
        /* Hmm... the terminal can't scroll backwards.  We'll have to
         * update the whole screen.
         */
        hedit->refresh.flag = 1;
        hedit->refresh.first[0] = 0;
        hedit->refresh.last[0] = -1;
        hedit->refresh.parts = 1;
      } else {
        tio_home();
        tio_display(he_line(hedit, hedit->screen_offset), 0);
      }
    } else {
      hedit->refresh.first[0] = 0;
      hedit->refresh.last[0] = -1;
      hedit->refresh.parts = 1;
    }
  }
}
/* he_scroll_down */

  void
he_scroll_up(hedit, count)
  struct he_s *hedit;
  int count;
{
  if (!count) return;
  assert(count > 0);
  while (count--) {
    if (hedit->position < hedit->screen_offset + 16) break;
    hedit->screen_offset += 16;
    if (!hedit->refresh.flag) {
      if (tio_scroll_up(1, 0, hx_lines - 2)) {
        /* The terminal can't scroll forward... */
        hedit->refresh.flag = 1;
        hedit->refresh.first[0] = 0;
        hedit->refresh.last[0] = -1;
        hedit->refresh.parts = 1;
      }
      tio_goto_line(hx_lines - 2);
      tio_return();
      tio_display(he_line(hedit, hedit->screen_offset + (hx_lines - 2) * 16), 0);
    } else {
      hedit->refresh.first[0] = 0;
      hedit->refresh.last[0] = -1;
      hedit->refresh.parts = 1;
    }
  }
}
/* he_scroll_up */

  int
he_update_screen(hedit)
  struct he_s *hedit;
  /* Update the screen.  If the position of the point is inside the
   * window, the `screen_offset' is adjusted and the screen scrolled.
   * If `refresh.flag' is set, the lines specified by `refresh.first'
   * and `refresh.last' are refreshed.  If `refresh.message_f' is set
   * the message is displayed on the bottom line of the screen. 
   * `*refresh' is set to `NO_REFRESH'.
   * The return value is 1 if a message was displayed, 0 else.
   */
{
  int rval = 0;
  struct he_message_s *m, *n;

  if (!hedit->refresh.flag) {
    hedit->refresh.first[0] = 0;
    hedit->refresh.last[0] = -1;
    hedit->refresh.parts = 0;
  }
  if (window_changed) {
    window_changed = 0;
    hedit->refresh.flag = 1;
  }

  /* If the position of the point is outside the screen, we'll do a full
   * refresh, if the number of lines to scroll is greater than half the
   * height of the window.
   */
  if (hedit->position < hedit->screen_offset)
    if (-HE_LINE(hedit->position - hedit->screen_offset) > hx_lines / 2 + 2) {
      hedit->refresh.flag = 1;
      hedit->refresh.first[0] = 0;
      hedit->refresh.last[0] = -1;
      hedit->refresh.parts = 1;
    }
  if (hedit->position >= hedit->screen_offset + (hx_lines - 2) * 16) {
    long k;
    k = HE_LINE(hedit->position - hedit->screen_offset) - hx_lines + 2;
    if (k > hx_lines / 2 + 2) {
      hedit->refresh.flag = 1;
      hedit->refresh.first[0] = 0;
      hedit->refresh.last[0] = -1;
      hedit->refresh.parts = 1;
    }
  }

  while (hedit->position < hedit->screen_offset) {
    hedit->screen_offset -= 16;
    if (!hedit->refresh.flag) {
      if (tio_scroll_down(1, 0, hx_lines - 2)) {
        /* Hmm... the terminal can't scroll backwards.  We'll have to
         * update the whole screen.
         */
        hedit->refresh.flag = 1;
        hedit->refresh.first[0] = 0;
        hedit->refresh.last[0] = -1;
        hedit->refresh.parts = 1;
      } else {
        tio_home();
        tio_display(he_line(hedit, hedit->screen_offset), 0);
      }
    } else {
      hedit->refresh.first[0] = 0;
      hedit->refresh.last[0] = -1;
      hedit->refresh.parts = 1;
    }
  }
  while (hedit->position >= hedit->screen_offset + (hx_lines - 1) * 16) {
    hedit->screen_offset += 16;
    if (!hedit->refresh.flag) {
      if (tio_scroll_up(1, 0, hx_lines - 2)) {
        /* The terminal can't scroll forward... */
        hedit->refresh.flag = 1;
        hedit->refresh.first[0] = 0;
        hedit->refresh.last[0] = -1;
        hedit->refresh.parts = 1;
      }
      tio_goto_line(hx_lines - 2);
      tio_return();
      tio_display(he_line(hedit, hedit->screen_offset + (hx_lines - 2) * 16), 0);
    } else {
      hedit->refresh.first[0] = 0;
      hedit->refresh.last[0] = -1;
      hedit->refresh.parts = 1;
    }
  }
  if (hedit->refresh.flag) {
    /* first we'r gonna check if some of the refresh commands can be
     * combined or omitted.  A cancelled command is marked with a negative
     * `first' value.
     */
    int i, j;
    hedit->refresh.parts += !hedit->refresh.parts;
    for (i = 0; i < hedit->refresh.parts; ++i)
      if (hedit->refresh.last[i] < 0) hedit->refresh.last[i] = hx_lines - 2;
    for (i = 0; i < hedit->refresh.parts - 1; ++i) {
      if (hedit->refresh.first[i] < 0) continue;
      for (j = i + 1; j < hedit->refresh.parts; ++j) {
        if (hedit->refresh.first[j] < 0) continue;
        if (hedit->refresh.first[i] < hedit->refresh.first[j]) {
          if (hedit->refresh.last[i] < hedit->refresh.first[j] - 1)
            /* these can't be combined */
            break;
          else {
            /* combine */
            hedit->refresh.first[j] = -1; /* cancel */
            if (hedit->refresh.last[j] > hedit->refresh.last[i])
              hedit->refresh.last[i] = hedit->refresh.last[j];
            --i;
            break;
          }
        } else {
          if (hedit->refresh.last[j] < hedit->refresh.first[i] - 1)
            /* can't combine */
            break;
          else {
            /* combine */
            hedit->refresh.first[i] = -1; /* cancel */
            if (hedit->refresh.last[i] > hedit->refresh.last[j])
              hedit->refresh.last[j] = hedit->refresh.last[i];
            --i;
            break;
          }
        }
      }
    }
    /* do the update */
    for (i = 0; i < hedit->refresh.parts; ++i) {
      if (hedit->refresh.first[i] >= 0)
        he_display(hedit, hedit->refresh.first[i], hedit->refresh.last[i]);
    }
  }
  /*
  if (hedit->refresh.messages) {
    int i, key;
    tio_goto_line(hx_lines - 1);
    tio_return();
    tio_clear_to_eol();
    tio_display(hedit->refresh.message[0], 1);
    for (i = 1, key = 0; i < hedit->refresh.messages; ++i) {
      if (i < hedit->refresh.messages) tio_printf(" ...\r");
      key = tio_tget(he_message_wait);
      if (key) /+ display the last message only. +/
        i = hedit->refresh.messages - 1;
      tio_goto_line(hx_lines - 1);
      tio_return();
      tio_clear_to_eol();
      tio_display(hedit->refresh.message[i], 1);
    }
    if (key) tio_ungetch(key);
    rval = 1;
  }
  if (hedit->refresh.beep) tio_bell();
  */
  if (he_messages) {
    int key;
    tio_goto_line(hx_lines - 1);
    tio_return();
    tio_clear_to_eol();
    for (m = he_messages, n = 0; m->next; n = m, m = m->next);
    tio_display(m->message, 1);
    if (m->beep) tio_bell();
    free((char *)m->message);
    free((char *)m);
    if (n) n->next = 0; else he_messages = 0;
    while (he_messages) {
      tio_printf(" ...\r");
      key = tio_tget(he_message_wait);
      if (key) /* display the last message only. */
        if (he_messages->next) {
          for (n = he_messages->next, m = n->next; n;
               n = m, m = m ? m->next : 0) {
            free((char *)n->message);
            free((char *)n);
          }
          he_messages->next = 0;
        }
      tio_goto_line(hx_lines - 1);
      tio_return();
      tio_clear_to_eol();
      for (m = he_messages, n = 0; m->next; n = m, m = m->next);
      tio_display(m->message, 1);
      if (m->beep) tio_bell();
      free((char *)m->message);
      free((char *)m);
      if (n) n->next = 0; else he_messages = 0;
    }
  }

  he_set_cursor(hedit);
  hedit->refresh = NO_REFRESH;
  return rval;
}
/* he_update_screen */

  static int
he_clear_get(command_mode)
  int command_mode;
  /* Some users tend to hold down keys like cursor keys or page up/down
   * keys when moving through the buffer.  If the screen update is slower
   * than the repeat rate of the keyboard, `he_clear_get()' takes those
   * keystrokes away from the input stream.  `he_clear_get()' stops
   * when a non-cursor-or-screen-movement key is read or the input queue
   * is empty.  The last key read is put back into the input stream.
   * If `command_mode' is set, the keys `h', `j', `k', `l', `<' and `>'
   * are taken as cursor-or-screen-movement keys.
   * The return value is 0 if no keys were read, else 1.
   */
{
  int key1, key2 = 0;

  tio_ungetch(tio_tget(1));
  while ((key1 = tio_get())) {
    key2 = key1;
    switch (key1) {
    HE_CASE_MOTION:
      break;
    HE_CASE_MOTION_SHIFT:
    HE_CASE_MOTION_HJKL:
      if (command_mode) break;
    default:
      key1 = 0;
    }
    if (key1) continue;
    break;
  }
  if (key2) {
    tio_ungetch(key2);
    return 1;
  }
  return 0;
}
/* he_clear_get */

  int
he_mainloop(hedit)
  struct he_s *hedit;
{
  long count = -1;
  int key;
  int display_status_f = 0;
  int map_string_f = 0;
  char map_string[256];

  tio_echo(0);
  tio_keypad(1);
  he_set_cursor(hedit);
  if (he_refresh_check(hedit)) tio_ungetch(HXKEY_NONE);
  if (hedit->begin_selection >= 0
      && hedit->end_selection >= hedit->begin_selection)
    tio_ungetch('v');
  for (;;) {
    key = tio_mgetch(MAP_COMMAND, map_string);
    switch (key) {
    HE_CASE_MOTION:
    HE_CASE_MOTION_SHIFT:
    HE_CASE_MOTION_HJKL:
      he_motion(hedit, key, count);
      break;
    HE_CASE_COMMAND:
    HE_CASE_COMMAND_INSERT:
      if (he_command(hedit, key, count) < 0) goto exit_mainloop;
      break;
    case HXKEY_ERROR:
      if (window_changed) he_refresh_screen(hedit);
      if (strlen(map_string)) {
        tio_goto_line(hx_lines - 1);
        tio_return();
        tio_clear_to_eol();
        tio_right(40);
        tio_raw_printf("%s", map_string);
        he_set_cursor(hedit);
        map_string_f = 1;
        continue;
      }
      break;
    default:
      if (key > 0xff) break;
      if (isdigit(key)) {
        tio_ungetch(key);
        count = he_get_counter(hedit);
        continue;
      } else
        goto exit_mainloop;
    }
    if (map_string_f) {
      tio_goto_line(hx_lines - 1);
      tio_return();
      tio_clear_to_eol();
      map_string_f = 0;
    }
    display_status_f = !he_update_screen(hedit);
    if (he_clear_get(1)) continue;
    if (display_status_f) {
      display_status_f = 0;
      /* Hier muesste der code fuer die statuszeile hin.
       */
    }
    count = -1;
  }

exit_mainloop:
  return key;
}
/* he_mainloop */

/* end of edit.c */


/* VIM configuration: (do not delete this line)
 *
 * vim:bk:nodg:efm=%f\:%l\:%m:hid:icon:
 * vim:sw=2:sm:textwidth=79:ul=1024:wrap:
 */
