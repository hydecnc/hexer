/* hexer.c	8/19/1995
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
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#if USE_STDARG
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include <sys/types.h>
#include <sys/param.h>

#ifndef PATH_MAX
#define PATH_MAX 63
#endif

#include "hexer.h"
#include "util.h"

const struct buffer_s NO_BUFFER = { 0, 0, 0, 0, 0, 0 };

const char *hexer_ext = ".hexer";

struct buffer_s *current_buffer;
struct buffer_s *buffer_list = 0;
struct he_message_s *he_messages;
char *alternate_buffer;
char *he_pagerprg;

#if USE_STDARG
  void
he_message(int beep, char *fmt, ...)
#else
  void
he_message(beep, fmt, va_alist)
  int beep;
  char *fmt;
  va_dcl
#endif
{
  va_list ap;
  struct he_message_s *m;
  /* int length; */

#if USE_STDARG
  va_start(ap, fmt);
#else
  va_start(ap);
#endif
  /* length = tio_nprintf(fmt, ap); */
  m = (struct he_message_s *)malloc(sizeof(struct he_message_s));
  m->next = he_messages;
  m->beep = beep;
  /* m->message = (char *)malloc(length + 1); */
  m->message = (char *)malloc(512); /* FIXME */
  vsprintf(m->message, fmt, ap);
  he_messages = m;
  va_end(ap);
}
/* he_message */


/* hexer options
 */

  static void
action_ascii(current_value)
  int current_value;
{
  he_refresh_all(current_buffer->hedit);
  if (current_value)
    s_set_option("iso", "false", 1);
  else
    s_set_option("iso", "true", 1);
}
/* action_ascii */

  static void
action_iso(current_value)
  int current_value;
{
  he_refresh_all(current_buffer->hedit);
  if (current_value)
    s_set_option("ascii", "false", 1);
  else
    s_set_option("ascii", "true", 1);
}
/* action_iso */

  static void
action_maxmatch(current_value)
  long current_value;
{
  extern long rx_maxmatch;

  rx_maxmatch = current_value;
}
/* action_maxmatch */

  static void
action_TERM()
{
  tio_init(0);
  he_refresh_all(current_buffer->hedit);
}
/* action_TERM */

  static void
action_specialnl(current_value)
  int current_value;
{
  extern rx_special_nl;

  rx_special_nl = current_value;
}
/* action_specialnl */

  static void
action_mapmagic(current_value)
  int current_value;
{
  extern he_map_special;

  he_map_special = current_value;
}
/* action_mapmagic */

static const struct hexer_options_s {
  char *option;
  enum s_option_e type;
  char *default_value;
  set_fn action;
} hexer_options[] = {
  { "ascii", S_BOOL, "true", (set_fn)action_ascii },
  { "iso", S_BOOL, "false", (set_fn)action_iso },
  { "maxmatch", S_INTEGER, "1024", (set_fn)action_maxmatch },
  { "TERM", S_STRING, "", (set_fn)action_TERM }, /* set in `hexer_init()' */
  { "specialnl", S_BOOL, "false", (set_fn)action_specialnl },
  { "mapmagic", S_BOOL, "false", (set_fn)action_mapmagic },
  { "fg", S_INTEGER, "7", (set_fn)0 },
  { "bg", S_INTEGER, "4", (set_fn)0 },
  { 0, (enum s_option_e)0, 0, 0 }
};


/* hexer buffers
 */

  int
he_open_buffer(name, path)
  char *name;
  char *path;
{
  struct buffer_s *buffer;
  int no_file_f = 0, read_only = 0;
  struct buffer_s *i;
  FILE *fp;
  char cwd[PATH_MAX + 1];

  if (path) {
    /* check out the read/write permissions */
    if (access(path, R_OK))
      switch (errno) {
      case ENOENT: /* file doesn't exist */
        no_file_f = 1;
        break;
      default:
        he_message(1, "`%s': @Ab%s@~", path, strerror(errno));
        return -1;
      }
    if (!no_file_f ? access(path, W_OK) : 0)
      switch (errno) {
      case EACCES: /* write permission denied */
        read_only = 1;
        break;
      default:
        he_message(1, "`%s': @Ab%s@~", path, strerror(errno));
        return -1;
      }
  }
  *(buffer = (struct buffer_s *)malloc(sizeof(struct buffer_s))) = NO_BUFFER;
  buffer->hedit = (struct he_s *)malloc(sizeof(struct he_s));
  memset(buffer->hedit, 0, sizeof (struct he_s));
  buffer->hedit->begin_selection = -1;
  buffer->hedit->end_selection = -1;
  buffer->hedit->insert_position = -1;
  buffer->hedit->buffer_name = strdup(name);
  if (path && !no_file_f) {
    if (!(fp = fopen(path, "r"))) {
      he_message(1, "`%s': @Ab%s@~", path, strerror(errno));
      free((char *)buffer->hedit->buffer_name);
      free((char *)buffer->hedit);
      free((char *)buffer);
      return -1;
    } else
      fclose(fp);
  } else {
    buffer->hedit->buffer = new_buffer(0);
    buffer->loaded_f = 1;
    buffer->visited_f = 1;
  }
  if (path) {
    buffer->path = strdup(path);
    if (!getcwd(cwd, PATH_MAX)) {
      he_message(0, "@Abcan't get cwd: %s@~", strerror(errno));
      buffer->fullpath = strdup(path);
    } else {
      buffer->fullpath =
        (char *)malloc(strlen(path) + strlen(cwd) + 2);
      sprintf(buffer->fullpath, "%s/%s", cwd, path);
    }
  }
  buffer->hedit->read_only = read_only;
  if (!buffer_list)
    current_buffer = buffer_list = buffer;
  else {
    for (i = buffer_list; i->next; i = i->next);
    i->next = buffer;
  }
  return 0;
}
/* he_open_buffer */

  int
he_select_buffer_(buffer)
  struct buffer_s *buffer;
  /* Set `current_buffer' to `buffer'.  The file for `buffer' is loaded if
   * nessecarry.
   */
{
  struct buffer_s *i;
  char swap_template[8];
  
  strcpy(swap_template, ".XXXXXX");
  for (i = buffer_list; i != buffer && i; i = i->next);
  if (!i) return -1;
  current_buffer = i;
  if (!i->loaded_f) {
    i->hedit->buffer = new_buffer(0);
    /* read the file */
    if (b_read_buffer_from_file(i->hedit->buffer, i->path) < 0) {
      delete_buffer(i->hedit->buffer);
      i->hedit->buffer = 0;
      he_close_buffer(0);
      return -1;
    }
    /* check out if we can open a swapfile */
    i->hedit->swapfile =
      (char *)malloc(strlen(hexer_ext) + strlen(i->path) + 1);
    strcpy(i->hedit->swapfile, i->path);
    strcat(i->hedit->swapfile, hexer_ext);
    if (access(i->hedit->swapfile, R_OK)) {
      if (errno == ENOENT) /* swapfile doesn't exist -- fine */
        if (access(i->hedit->swapfile, W_OK))
          if (errno == ENOENT) {
            if ((i->hedit->undo.swapfile = fopen(i->hedit->swapfile, "w+")))
              i->hedit->swapping = 1;
          } else
            he_message(0, "@Abno swapfile@~");
    } else {
      /* a swapfile does exist */
      int swapfd;
      he_message(1, "@Abwarning: swapfile@~ `%s' @Abexists@~",
                 i->hedit->swapfile);
      i->hedit->swapfile =
        (char *)realloc(i->hedit->swapfile,
                      strlen(i->hedit->swapfile) + strlen(swap_template) + 1);
        strcat(i->hedit->swapfile, swap_template);
      if ((swapfd = mkstemp(i->hedit->swapfile)) < 0)
        he_message(0, "@Abno swapfile@~");
      else {
        i->hedit->undo.swapfile = fdopen(swapfd, "r+");
        i->hedit->swapping = 1;
        he_message(0, "@Abswapping to@~ `%s'", i->hedit->swapfile);
      }
    }
    if (i->hedit->swapping) {
      /* write the swap-header to the swapfile */
      fputs("hexer ", i->hedit->undo.swapfile);
      fputs(HEXER_VERSION, i->hedit->undo.swapfile);
      fputc('\n', i->hedit->undo.swapfile);
      fputs(i->fullpath, i->hedit->undo.swapfile);
      fputc('\n', i->hedit->undo.swapfile);
      fwrite("\0\0\0\0", 4, 1, i->hedit->undo.swapfile);
      fflush(i->hedit->undo.swapfile);
    }
    i->hedit->buffer->modified = 0;
    i->loaded_f = 1;
    i->visited_f = 1;
  }
  return 0;
}
/* he_select_buffer_ */

  int
he_select_buffer(name)
  char *name;
{
  struct buffer_s *i;

  for (i = buffer_list; i; i = i->next)
    if (!strcmp(name, i->hedit->buffer_name)) break;
  if (!i) return -1;
  return he_select_buffer_(i);
}
/* he_select_buffer */

  int
he_alternate_buffer()
{
  char *ab = alternate_buffer;

  alternate_buffer = current_buffer->hedit->buffer_name;
  if (ab ? he_select_buffer(ab) < 0 : 0) {
    alternate_buffer = 0;
    return -1;
  }
  return 0;
}
/* he_alternate_buffer */

  int
he_set_buffer_readonly(name)
  char *name;
  /* Return values:
   * -1: no buffer named `name'
   * 0:  ok
   */
{
  struct buffer_s *i;

  for (i = buffer_list; i; i = i->next)
    if (!strcmp(name, i->hedit->buffer_name)) break;
  if (!i) return -1;
  i->hedit->read_only = 1;
  return 0;
}
/* he_set_buffer_readonly */

  int
he_buffer_readonly(name)
  char *name;
  /* Return values:
   * -1: no buffer named `name'
   * 0:  buffer is readwrite
   * 1:  buffer is readonly
   */
{
  struct buffer_s *i;

  for (i = buffer_list; i; i = i->next)
    if (!strcmp(name, i->hedit->buffer_name)) break;
  if (!i) return -1;
  return !!i->hedit->read_only;
}
/* he_buffer_readonly */

  int
he_buffer_modified(name)
  char *name;
  /* Return values:
   * -1: no buffer named `name'
   * 0:  buffer saved
   * 1:  buffer modified
   */
{
  struct buffer_s *i;

  for (i = buffer_list; i; i = i->next)
    if (!strcmp(name, i->hedit->buffer_name)) break;
  if (!i) return -1;
  return !!i->hedit->buffer->modified;
}
/* he_buffer_modified */

  int
he_close_buffer(name)
  char *name;
  /* Close the buffer named `name'. If `name == 0', the current buffer
   * is closed.  The return value is 0 if all goes well, 1 if the named
   * buffer doesn't exist and -1 if the `buffer_list' is empty.
   */
{
  struct buffer_s *i, *j;
  int empty = 0;
  int buffer_switched = 0;

  if (!buffer_list) return -1;
  if (!name)
    i = current_buffer;
  else {
    for (i = buffer_list; i; i = i->next)
      if (!strcmp(i->hedit->buffer_name, name)) break;
    if (!i) return -1;
  }
  if (i != buffer_list) {
    for (j = buffer_list; j->next != i; j = j->next);
    if (alternate_buffer
        ? !strcmp(alternate_buffer, i->hedit->buffer_name) : 0)
      alternate_buffer = 0;
    j->next = i->next;
    if (i == current_buffer) {
      if (i->next)
        he_select_buffer_(i->next);
      else
        he_select_buffer_(j);
      buffer_switched = 1;
    }
  } else
    if (!(buffer_list = current_buffer = i->next))
      empty = 1;
    else {
      he_select_buffer_(buffer_list);
      buffer_switched = 1;
    }
  if (i->hedit->buffer_name) free((char *)i->hedit->buffer_name);
  if (i->hedit->buffer) delete_buffer(i->hedit->buffer);
  if (i->hedit->swapping) {
    fclose(i->hedit->undo.swapfile);
    unlink(i->hedit->swapfile);
  } else
    if (i->hedit->undo.list) he_free_command(i->hedit->undo.list);
  free((char *)i->hedit);
  if (i->path) free((char *)i->path);
  free((char *)i);
  if (empty) buffer_list = 0, current_buffer = 0;
  if (buffer_switched) {
    alternate_buffer = 0;
    he_refresh_all(current_buffer->hedit);
  }
  return empty ? -1 : 0;
}
/* he_close_buffer */


/* misc
 */

  void
he_status_message(verbose)
  int verbose;
  /* display name and size of the current buffer.  if `verbose' is set,
   * the current position is also displayed.
   */
{
  struct he_s *hedit = current_buffer->hedit;

  if (hedit->buffer->size)
    if (verbose)
      he_message(0, "\"%s\" %s%sat 0x%lx (%li) of 0x%lx (%li) bytes  (%li %%)",
                 hedit->buffer_name,
                 hedit->buffer->modified ? "[modified] " : "",
                 hedit->read_only ? "[readonly] " : "",
                 hedit->position, hedit->position,
                 hedit->buffer->size, hedit->buffer->size,
                 (hedit->position * 100) / hedit->buffer->size);
    else
    if (hedit->buffer->size)
      he_message(0, "\"%s\" %s%s0x%lx (%li) bytes",
                 hedit->buffer_name,
                 hedit->buffer->modified ? "[modified] " : "",
                 hedit->read_only ? "[readonly] " : "",
                 hedit->buffer->size, hedit->buffer->size);
  else
    he_message(0, "\"%s\" %s%s(empty)", hedit->buffer_name,
               hedit->buffer->modified ? "[modified] " : "",
               hedit->read_only ? "[readonly] " : "");
}
/* he_status_message */

  char *
he_query_command(prompt, dfl, context)
  char *prompt;
  char *dfl;
  int context;
  /* Convention:
   * `context == 0': exh-command;
   * `context == 1': shell-command;
   * `context == 2': search-command.
   * `context == 3': calculator.
   */
{
  extern int hx_lines;

  tio_goto_line(hx_lines - 1);
  tio_return();
  return readline(prompt, dfl, context);
}
/* he_query_command */

#if USE_STDARG
  int
he_query_yn(int dfl, char *fmt, ...)
#else
  int
he_query_yn(dfl, fmt, va_alist)
  int dfl;
  char *fmt;
  va_dcl
#endif
{
  va_list ap;
  extern int hx_lines;
  int key;
  int choice;
  extern window_changed;

#if USE_STDARG
  va_start(ap, fmt);
#else
  va_start(ap);
#endif
  tio_keypad(0);
restart:
  tio_goto_line(hx_lines - 1);
  tio_return();
  tio_clear_to_eol();
  if (dfl < 0) { /* no default answer */
    tio_vprintf(fmt, ap);
    tio_printf("? ");
  } else {
    dfl = !!dfl;
    tio_vprintf(fmt, ap);
    tio_printf("? [%c] ", dfl ? 'y' : 'n');
  }
  for (;;) {
    key = tio_getch();
    switch (key) {
    case HXKEY_NONE:
      if (window_changed) he_refresh_screen(current_buffer->hedit);
      goto restart;
    case 'q': case 'Q':
      tio_printf("quit");
      choice = -1;
      goto exit_he_query_yn;
    case HXKEY_ESCAPE:
      tio_printf("escape");
      choice = -1;
      goto exit_he_query_yn;
    case HXKEY_RETURN:
      if (dfl < 0) break;
      choice = dfl;
      switch (choice) {
      case -1: tio_printf("quit"); break;
      case 0: tio_printf("no"); break;
      case 1: tio_printf("yes"); break;
      case 2: tio_printf("always"); break;
      }
      goto exit_he_query_yn;
    case 'y': case 'Y':
      tio_printf("yes");
      choice = 1;
      goto exit_he_query_yn;
    case 'n': case 'N':
      tio_printf("no");
      choice = 0;
      goto exit_he_query_yn;
    case 'a': case 'A':
      tio_printf("always");
      choice = 2;
      goto exit_he_query_yn;
    default:
      break;
    }
  }

exit_he_query_yn:
  tio_return();
  tio_keypad(1);
  tio_flush();
  va_end(ap);
  return choice;
}
/* he_query_yn */


/* I/O wrap-functions for `regex_match()':
 */

static Buffer *rxwrap_current_buffer;
static long rxwrap_position;

  static long
rxwrap_read(buf, count)
  char *buf;
  long count;
{
  long rval = b_read(rxwrap_current_buffer, buf, rxwrap_position, count);
  rxwrap_position += rval;
  return rval;
}
/* rxwrap_read */

  static long
rxwrap_seek(position)
  long position;
{
  return rxwrap_position = position;
}
/* rxwrap_seek */

  static long
rxwrap_tell()
{
  return rxwrap_position;
}
/* rxwrap_tell */

  long
he_search(hedit, exp, replace, direction, wrap, increment, end,
          replace_str, replace_len, match_len)
  struct he_s *hedit;
  char *exp;
    /* regular expression.
     */
  char *replace;
    /* replace template.  the replace template may contain back references to
     * the regular expression (`\0', ... `\9').
     */
  int direction;
    /* `direction >= 0': forward search.
     * `direction < 0': reverse search.
     */
  int wrap;
    /* if `wrap' is set, the search continues at the top of the buffer/file
     * if the bottom has been (or vice versa, depending on `direction').
     */
  int increment;
    /* if `increment' is set, the search starts at `hedit->position + 1'
     * rather than at `hedit->position'.  if the direction is set to reverse
     * search, the `increment'-flag has no effect.
     */
  long end;
    /* if `wrap' is not set and `end' is not negative, the search ends at
     * position `end'.
     */
  char **replace_str;
    /* if `replace_str' is non-zero and a match was found, the replace
     * string generated from `replace' will be copied to `*replace_str'.
     * the memory for that replace string will be allocated via `malloc()'.
     * NOTE: the replace string wont be terminated with a null-character
     *   since it may contain null characters.
     */
  long *replace_len;
    /* the length of the replace sting is written to `*replace_len'.
     */
  long *match_len;
    /* the length of the match is written to `*replace_len'.
     */
  /* RETURN VALUE:  if a match was found, the position of the match is
   *   returned;  -1: search failed;  -2: illegal expression (check out
   *   `rx_error'/`rx_error_msg').
   * NOTE:  if the returned value is positive, `*replace_str' has to be 
   *   `free()'d by the caller.
   */
{
  static long *regex;
  long position;

  /* setup the regex I/O */
  regex_init(rxwrap_read, rxwrap_seek, rxwrap_tell);
  rxwrap_current_buffer = hedit->buffer;
  regex_reset();

  if (wrap || end < 0) end = hedit->buffer->size;

  if (exp) if (!(regex = regex_compile(exp, replace))) return -2;
  if (direction < 0) direction = -1; else direction = 1;
  if (direction > 0) {
    position = regex_search(regex, 0, end, hedit->position + !!increment,
                            1, replace_str, replace_len, match_len);
    if (wrap && position < 0) {
      position = regex_search(regex, 0, end, 0, 1,
                              replace_str, replace_len, match_len);
      if (position >= 0) he_message(0, "@Abwrapped@~");
    }
  } else {
    position = regex_search(regex, 0, end, hedit->position - !!hedit->position,
                            -1, replace_str, replace_len, match_len);
    if (wrap && position < 0) {
      position = regex_search(regex, 0, end, hedit->buffer->size - 1,
                              -1, replace_str, replace_len, match_len);
      if (position >= 0) he_message(0, "@Abwrapped@~");
    }
  }
  return position;
}
/* he_search */

  void
he_search_command(hedit, exp, dir)
  struct he_s *hedit;
  char *exp;
  int dir;
{
  long position;
  char *rs;
  long rl, ml;
  static char last_exp[4096] = "";

  if (!*exp)
    if (!*last_exp) {
      he_message(0, "@Abno previous expression@~");
      goto exit;
    } else
      exp = last_exp;
  switch ((position = he_search(hedit, exp, "", dir, 1, 1, -1,
                                &rs, &rl, &ml))) {
  case -2:  /* invalid expression */
    he_message(0, "@Abinvalid expression:@~ %s", rx_error_msg[rx_error]);
    goto exit;
  case -1:  /* no match */
    if (!rx_error)
      he_message(0, "no match");
    else
      he_message(0, "@Abregexp error:@~ %s", rx_error_msg[rx_error]);
    break;
  default:  /* match */
    hedit->position = position;
    free((char *)rs);
    break;
  }
  if (exp != last_exp) strcpy(last_exp, exp);

exit:
  return;
}
/* he_search_command */

  extern int
exh_command( /* struct he_s *hedit, char *cmd */ );

  static void
he_refresh()
{
  he_refresh_all(current_buffer->hedit);
  he_update_screen(current_buffer->hedit);
}
/* he_refresh */

  void
hexer_version()
{
  he_message(0, "@AbHexer@~ version @U%s@u", HEXER_VERSION);
}
/* hexer_version */

  void
hexer_init()
  /* this function is called by `process_args()' (main.c) before executing
   * the commands given at the command line.
   */
{
  int i;
  extern he_map_special;
  extern char *terminal_name;
  char *hexerinit, *home;
  char path[1024];
  char line[1024];
  extern char *exh_initialize[];
  FILE *fp;

  he_pagerprg = getenv("PAGER");
  if (!he_pagerprg) he_pagerprg = HE_DEFAULT_PAGER;
  for (i = 0; hexer_options[i].option; ++i) {
    s_set_type(hexer_options[i].option, hexer_options[i].type);
    s_set_option(hexer_options[i].option, hexer_options[i].default_value, 1);
    s_set_action(hexer_options[i].option, hexer_options[i].action);
  }
  s_set_option("TERM", terminal_name, 1);

  he_map_special = 1;
  for (i = 0; exh_initialize[i]; ++i)
    exh_command(current_buffer->hedit, exh_initialize[i]);

  if (!(home = getenv("HOME"))) home = ".";
  if (!(hexerinit = getenv("HEXERRC"))) hexerinit = HEXERINIT_FILE;
  strcpy(path, home);
  strcat(path, "/");
  strcat(path, hexerinit);
  if ((fp = fopen(path, "r"))) {
    while (!feof(fp)) {
      fgets(line, 1024, fp);
      if (!*line) break;
      line[strlen(line) - 1] = 0; /* discard the trailing newline */
      if (*line && *line != '"')
        exh_command(current_buffer->hedit, line, 0);
      /* the command might have quit the editor, so we gotta check */
      if (!current_buffer) {
        fprintf(stderr,
                "warning: a command in your `%s' causes the editor to quit.\n",
                hexerinit);
        break;
      }
    }
  }
}
/* hexer_init */

  int
hexer()
{
  int key;
  char *cmd;
  char dfl[256], buf[256];
  long begin, end;
  int anchor, anchor_f = 0;
  extern int rl_redisplay;
  extern void (*rl_winch)( /* void */ );
  extern void (*tio_winch)( /* void */ );
  int redisplay;

  tio_winch = rl_winch = he_refresh;
  he_refresh_part(current_buffer->hedit, 0, -1);
  for (; buffer_list;) {
    key = he_mainloop(current_buffer->hedit);
    redisplay = 0;
    switch (key) {
    case '!':  /* shell-command. */
      he_cancel_selection(current_buffer->hedit);
      he_update_screen(current_buffer->hedit);
      cmd = he_query_command("!", "", 1);
      redisplay = rl_redisplay;
      if (cmd) {
        strcpy(buf + 1, cmd);
        *buf = '!';
        exh_command(current_buffer->hedit, buf);
      }
      break;
    case ':':  /* exh command. */
      begin = current_buffer->hedit->begin_selection;
      end = current_buffer->hedit->end_selection;
      anchor = current_buffer->hedit->anchor_selection;
      if (begin >= 0 && end >= begin) {
        anchor_f = 1;
	sprintf(dfl, "0x%lx,0x%lx ", begin, end);
      } else {
	*dfl = 0;
        anchor_f = 0;
      }
      he_cancel_selection(current_buffer->hedit);
      cmd = he_query_command(":", dfl, 0);
      redisplay = rl_redisplay;
      if (cmd ? *cmd : 0)
        exh_command(current_buffer->hedit, cmd);
      if (current_buffer && anchor_f)
        if (current_buffer->hedit->begin_selection >= 0)
          current_buffer->hedit->anchor_selection = anchor;
      break;
    case '^' & 0x1f: /* C-^ - switch to alternate buffer */
      he_cancel_selection(current_buffer->hedit);
      if (he_alternate_buffer() < 0) 
	he_message(0, "no alternate buffer");
      else {
	he_refresh_part(current_buffer->hedit, 0, -1);
	tio_ungetch('g' & 0x1f);  /* FIXME */
      }
      break;
    default:
      he_cancel_selection(current_buffer->hedit);
      he_update_screen(current_buffer->hedit);
    }
    if (current_buffer) {
      if (redisplay) he_refresh_all(current_buffer->hedit);
      he_update_screen(current_buffer->hedit);
    }
  }
  return 0;
}
/* hexer */

/* end of hexer.c */


/* VIM configuration: (do not delete this line)
 *
 * vim:aw:bk:ch=2:nodg:efm=%f\:%l\:%m:et:hid:icon:
 * vim:sw=2:sc:sm:si:textwidth=79:to:ul=1024:wh=12:wrap:wb:
 */
