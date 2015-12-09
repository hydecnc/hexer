/* hexer.h	8/19/1995
 */

/* Copyright (c) 1995,1996 Sascha Demetrio
 * Copyright (c) 2009 Peter Pentchev
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

#ifndef _HEXER_H_
#define _HEXER_H_

#include "config.h"
#include "defs.h"

#include <stdio.h>
#include <stdarg.h>
#if HAVE_ALLOCA
#if NEED_ALLOCA_H
#include <alloca.h>
#endif
#else
char *alloca(size_t);
#endif

#define TIO_MAP 1

#define HE_REFRESH_MAX_PARTS 64

enum map_e { MAP_COMMAND = 1, MAP_VISUAL, MAP_INSERT, MAP_EXH };

struct he_refresh_s {
  int flag;
  int first[HE_REFRESH_MAX_PARTS];
  int last[HE_REFRESH_MAX_PARTS];
  int parts;
  int message_f;
  char message[HE_REFRESH_MAX_PARTS][256];
  int messages;
  int beep;
};

struct he_s {
  Buffer *buffer;
  char *buffer_name;
  long position;
  long screen_offset;
  long begin_selection;
  long end_selection;
  int anchor_selection;
    /* `anchor_selection == 0': the position is equal to `end_selection'
     * `anchor_selection == 1': the position is equal to `begin_selection'
     */
  long insert_position;
  int text_mode;
  int swapping;
    /* `swapping == 0': no swapfile, all changes are stored in memory.
     * `swapping == 1': use the filestream `swapfile' for swapping.
     */
  char *swapfile;
    /* name of the swapfile */
  union he_undo_u {
    FILE *swapfile;
      /* Filestream of the swapfile.
       */
    struct he_command_s *list;
      /* The first command in the undo list.
       */
  } undo;
  struct he_command_s *command;
    /* Current command in the undo-list.  Used if swapping is turned off.
     */
  struct he_refresh_s refresh;
  int read_only;
};

extern const struct he_refresh_s NO_REFRESH;

struct he_message_s {
  struct he_message_s *next;
  char *message;
  int beep;
};

extern struct he_message_s *he_messages;

void
he_message( int beep, const char *fmt, ... ) __printflike(2, 3);

void
he_refresh_part(struct he_s *hedit, long pos1, long pos2);

void
he_refresh_lines(struct he_s *hedit, int first, int last);

int
he_refresh_check(struct he_s *hedit);

#define he_refresh_all(hedit) ((void)he_refresh_part((hedit), 0, -1))

void
he_refresh_screen(const struct he_s *hedit);

int
he_update_screen(struct he_s *hedit);


struct he_command_s {
  struct he_command_s *next_subcommand;
    /* Multiple commands may be treated as a single command, i.e. a replace
     * command is made up by an insert command and a delete command.  Such
     * a compound command is stored as a list of commands.
     */
  struct he_command_s *next_command;
    /* The next command in the undo list.  This pointer is set only for
     * the first command of a compound command.  A null-value in the first
     * command of a compound command indicates the end of the list.
     * NOTE: For all subcommands that are not the first command in a compound
     *   command, `next_command' has to be set to zero.
     */
  struct he_command_s *prev_command;
    /* The previous command in the undo list.  A null-value in the first
     * command of a compound command indicates the beginning of the list.
     * For all subcommands that are not the first command in a compound
     * command, the value of `prev_command' is ignored.
     */
  int type;
    /* `type == 0': delete command; `type == 1': insert command.
     */
  int again;
    /* `again == 1': This command can be performed again at a different
     *   position in the file.  When the command is repeated with the
     *   `he_again()' function, the position of every subcommand will be
     *   changed to the new position, thus the `again' flag should be set
     *   only if all subcommands perform on the same position in the file.
     * `again == 0': This command can't be performed again at a different
     *   position in the file.
     */
  unsigned long position;
    /* The position of the byte of the inserted or deleted data.
     */
  unsigned long count;
    /* The length of the buffer containing the inserted/deleted data.
     */
  char *data;
    /* A pointer to the inserted/deleted data.
     */
};

/* The format of the swap file is defined as:
 *  - The string "hexer <version>\n" (without the quotes), where
 *    <version> is the version number of `hedit'.
 *  - The full path and name of the file being edited, followed by a newline.
 *  - 4 zero bytes.
 *  - A list of commands, terminated by 4 zero bytes.
 * Each command is stored as:
 *  - The number of subcommands stored as big-endian long (4 bytes).
 *  - One byte indicating if the command may be performed again at a
 *    different position.
 *    0: `again' flag cleared.
 *    1: `again' flag set.
 *  - A list of subcommands.
 *  - The position of the first byte of the current entry.  This allows
 *    reading backwards through the file.
 * Each subcommand is stored as:
 *  - One byte indicating the type of the command:
 *    0: delete command.
 *    1: insert command.
 *  - The position in the file (big-endian long).
 *  - The number of bytes inserted/deleted (big-endian long).
 *  - The inserted/deleted data.
 */

  void
he_free_command(struct he_command_s *command);
  /* Free the memory allocated by command and all the following commands
   * in the list.
   */

  void
he_compound_comand(struct he_s *hedit, struct he_command_s *command);

  void
he_subcommand(struct he_s *hedit,
                  int type, unsigned long position, unsigned long count, char *data);
  /* Update the undo-list of `hedit' by inserting the given subcommand.
   * The command is *not* performed by calling `he_subcommand()'.
   * A sequence of subcommands must be terminated by calling
   * `he_subcommand(hedit, -1, 0, 0, 0)'.
   * NOTE:  Iff the position for all subcommands is the same, the `again'
   *   flag will be set to 1.
   */

  long
he_do_command(struct he_s *hedit, struct he_command_s *command);
  /* Perform the compound command `command'.  The return value is the
   * position of the last change made.
   */


/* buffers
 */

struct buffer_s {
  struct buffer_s *next;
  struct he_s *hedit;
  char *path;
  char *fullpath;
  int loaded_f;
  int visited_f;
};

extern const struct buffer_s     NO_BUFFER;
extern char             *alternate_buffer;
extern struct buffer_s  *current_buffer;
extern const char       *he_pagerprg;


/* exh commands
 */

typedef const char *(*exh_fn)(struct he_s *, const char *, long, long);

struct exh_cmd_s {
  const char *cmd_name;
  char cmd_char;
  exh_fn cmd;
  int whole_f;
    /* `whole_f == 1': if no range is specified, the whole file is use. */
  int expect;
    /* `expect == 0': no completer available
     * `expect == 1': expects a filename
     * `expect == 2`: expects a buffername
     * `expect == 3': expects a filename or a buffername
     * `expect == 4': expects an option name
     */
};


#define HE_LINE(x) ((long)(x) >> 4)

int
he_open_buffer(const char *name, const char *path);

int
he_select_buffer(const char *name);

int
he_alternate_buffer(void);

int
he_set_buffer_readonly(const char *name);
  /* Return values:
   * -1: no buffer named `name'
   * 0:  ok
   */

int
he_buffer_readonly(char *name);
  /* Return values:
   * -1: no buffer named `name'
   * 0:  buffer is read/write
   * 1:  buffer is read-only
   */

int
he_buffer_modified(char *name);
  /* Return values:
   * -1: no buffer named `name'
   * 0:  buffer saved
   * 1:  buffer modified
   */

int
he_close_buffer(const char *name);
  /* Close the buffer named `name'. If `name == 0', the current buffer
   * is closed.  The return value is 0 if all goes well, 1 if the named
   * buffer doesn't exist and -1 if the `buffer_list' is empty.
   */

void
he_status_message(int verbose);
  /* display name and size of the current buffer.  if `verbose' is set,
   * the current position is also displayed.
   */

void
he_select(struct he_s *hedit, unsigned long begin, unsigned long end);

int
he_select_buffer_(const struct buffer_s *);

void
he_cancel_selection(struct he_s *hedit);

long
he_search(struct he_s *, const char *, const char *, int, int, int, long,
              char **, long *, long *);

void
he_search_command(struct he_s *, char *, int);

char *
he_query_command(const char *, const char *, int);

int
he_query_yn( int dfl, const char *fmt, ... ) __printflike(2, 3);

int
he_mainloop(struct he_s *hedit);

int
hexer(void);

void             hexer_init(void);
void             hexer_version(void);

#endif

/* end of hexer.h */


/* VIM configuration: (do not delete this line)
 *
 * vim:bk:nodg:efm=%f\:%l\:%m:hid:icon:
 * vim:sw=2:sm:textwidth=79:ul=1024:wrap:
 */
