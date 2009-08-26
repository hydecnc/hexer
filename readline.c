/* readline.c	8/19/1995
 * Read a line of input from the keyboard, version 0.3
 * NOTE:  This work got nothing to do with GNU readline.
 * Required: tio.o map.o
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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "buffer.h"
#include "hexer.h"
#include "readline.h"
#include "tio.h"
#include "util.h"

#ifndef USE_PROTOTYPES
#define USE_PROTOTYPES 1
#endif

#define LINE_MAXLEN 8192        /* maximum length of the line */
#define RL_MAX_CONTEXTS 16      /* maximum number of history contexts */

#define RL_LEFT_MORE "@Ab<@~"   /* character displayed if the line is
                                 * partially scrolled out to the left */
#define RL_RIGHT_MORE "@Ab>@~"  /* character displayed if the line is
                                 * partially scrolled out to the right */
#if ('\e' == 'e')
#define RL_ESC ((char)27)
#else
#define RL_ESC ('\e')
#endif

/* strings:
 * `readline()' returns strings of characters.  special characters (like
 * cursor-keys) are represented as escape-sequences.  an escape-sequence
 * is made up of an escape character `\E' (0x1b) followed by the internal
 * representation of the key minus 254 (== HXKEY_BIAS) (we don't use 256 to
 * avoid null-characters).  we'll write down an escape-sequence as
 * `\E(HXKEY_keyname)'.  all characters will be stored as characters, even if
 * a `HXKEY_keyname' entry in the keylist exists.  the only two exeptions to
 * that rule are the null-character (represented as `\E(HXKEY_NULL)') and the
 * escape-character, which is represented as `\E\001' (that's why an offset
 * of 254 (== HXKEY_BIAS) is used for the other keys).
 */

static char *rl_prompt;    /* the prompt */
static int rl_prompt_len;  /* the length of the prompt in characters */
static char *rl_default;   /* the default answer */

static char rl_line[LINE_MAXLEN];
                           /* buffer for the current line */
static char rl_vline[LINE_MAXLEN];
                           /* buffer for the "visible" line */
static int rl_position;    /* position of the cursor.  the position of the
                            * cursor is number of the character under the
                            * cursor.  note that escape-sequences are
                            * considered to be a single character. */
static int rl_offset;      /* number of characters scrolled out to the
                            * left, so the first character displayed on
                            * screen is `rl_vline[rl_offset]'. */

struct rl_line_s {
  char *line;
  char *vline;
} rl = { rl_line, rl_vline };

#if USE_PROTOTYPES
char **(*completer)(char *prefix, char *command, char *line, int context);
#else
char **(*completer)();
#endif
  /* Pointer to the completer function.  If `completer == 0', completion
   * is disabled.
   */

int rl_backspace_jump;
  /* If the beginning of the line has been scrolled to the left and the cursor
   * is in the leftmost visible column of the line, you can't see the
   * characters as you backspace over them.  This problem can be fixed
   * partially by jump-scrolling the line to the right if you hit backspace
   * in the leftmost visible column.  `rl_backspace_jump' is (if it holds a
   * positive, non-zero value) the number of columns to jump-scroll.
   */

int rl_cancel_on_bs;
  /* if `rl_cancel_on_bs' is non-zero, hitting backspace on an empty line
   * will return a null-pointer - just as if escape had been pressed.
   */

int rl_redisplay;
  /* `readline()' will this flag to 1 if the screen has to be rebuilt.
   */

void (*rl_winch)(void) = 0;
  /* function to call in case of a changed window size
   */

/* history:
 * on the `readline()' prompt it is possible to access the `rl_history_max'
 * most recent lines typed in a specific context.  distinct command histories
 * can be maintained for up to `RL_MAX_CONTEXTS' contexts.
 */

static struct rl_line_s *rl_history[RL_MAX_CONTEXTS];
  /* array of pointers to the history-lists for all contexts.
   */
static int rl_history_max;
  /* maximum length of the history buffer (per context).
   */
static int rl_history_c[RL_MAX_CONTEXTS];
  /* current size of the history buffer for each context.
   */

  static int
rl_history_init(int max)
  /* initialize readline-history.  set the maximum number of lines to
   * `max' (`rl_history_max').
   */
{
  int i, j;

  for (i = 0; i < RL_MAX_CONTEXTS; ++i) {
    rl_history[i] =
      (struct rl_line_s *)malloc(max * sizeof(struct rl_line_s));
    for (j = 0; j < max; ++j) {
      rl_history[i][j].line = (char *)malloc(1);
      *rl_history[i][j].line = 0;
      rl_history[i][j].vline = (char *)malloc(1);
      *rl_history[i][j].vline = 0;
    }
    rl_history_c[i] = 1;
  }
  rl_history_max = max;
  return 0;
}
/* rl_history_init */


/* static functions for manipulating the history buffers.
 */

static int rl_current_context;  /* number of the current context */
static int rl_index;            /* index of the current line */

  static void
rl_history_add(struct rl_line_s line)
  /* add the line `line' to the history of the current context.
   */
{
  int i;

  if (rl_history_c[rl_current_context] + 1 >= rl_history_max) {
    free((char *)rl_history[rl_current_context][0].line);
    for (i = 0; i < rl_history_max - 1; ++i)
      rl_history[rl_current_context][i] =
        rl_history[rl_current_context][i + 1];
    rl_history[rl_current_context][i].line =
      (char *)malloc(strlen(line.line) + 1);
  } else {
    i = rl_history_c[rl_current_context] - 1;
    ++rl_history_c[rl_current_context];
    rl_history[rl_current_context][i].line =
      (char *)realloc(rl_history[rl_current_context][i].line,
                      strlen(line.line) + 1);
  }
  strcpy(rl_history[rl_current_context][i].line, line.line);
}
/* rl_history_add */

  static void
rl_history_reset(int context)
  /* set the current context `rl_current_context' to `context'.
   */
{
  rl_current_context = context;
  rl_index = rl_history_c[context] - 1;
}
/* rl_history_reset */

  static struct rl_line_s *
rl_history_up(void)
  /* move up one line in the history of the current context.
   * return a pointer to that line.
   */
{
  if (--rl_index < 0) {
    rl_index = 0;
    return 0;
  }
  return &rl_history[rl_current_context][rl_index];
}
/* rl_history_up */

  static struct rl_line_s *
rl_history_down(void)
  /* move down one line in the history of the current context.  
   * return a pointer to that line.
   */
{
  int c = rl_history_c[rl_current_context] - 1;

  if (++rl_index > c) {
    rl_index = c;
    return 0;
  }
  return &rl_history[rl_current_context][rl_index];
}
/* rl_history_down */

  static void
rl_history_set(struct rl_line_s line)
  /* set the current history entry of the current context to `line'.
   */
{
  rl_history[rl_current_context][rl_index].line =
    (char *)realloc(rl_history[rl_current_context][rl_index].line,
                    strlen(line.line) + 1);
  strcpy(rl_history[rl_current_context][rl_index].line, line.line);
}
/* rl_history_set */


/* readline:
 */

  static int
rl_query_yn(char *prompt, int dfl)
  /* query the user with a y/n-requester.  `prompt' is the query-prompt.
   * `dfl' is the default answer offered to the user.  the value of `dfl'
   * means:
   *   -1:  no default.
   *    0:  default is no.
   *    1:  default is yes.
   * if `dfl == -1', the return key is ignored.  if case fo a windowchange
   * and if `rl_winch != 0', the function `(*rl_winch)()' is called.
   * return values:
   *   -1:  the ESCAPE-key has been pressed.
   *    0:  no.
   *    1:  yes.
   */
{
  int key;
  int choice;

  tio_keypad(0);
  tio_goto_line(hx_lines - 1);
  tio_return();
  tio_clear_to_eol();
  if (dfl < 0) /* no default answer */
    tio_printf("%s? ", prompt);
  else {
    dfl = !!dfl;
    tio_printf("%s? [%c] ", prompt, dfl ? 'y' : 'n');
  }
  for (;;) {
    key = tio_mgetch(0, 0);
    switch (key) {
      case HXKEY_ERROR:
        if (window_changed && rl_winch) rl_winch();
        window_changed = 0;
        return rl_query_yn(prompt, dfl);
      case HXKEY_ESCAPE:
        tio_printf("escape");
        choice = -1;
	goto exit_rl_query_yn;
      case HXKEY_RETURN:
        if (dfl < 0) break;
	choice = dfl;
	goto exit_rl_query_yn;
      case 'y': case 'Y':
        tio_printf("yes");
        choice = 1;
	goto exit_rl_query_yn;
      case 'n': case 'N':
	tio_printf("no");
	choice = 0;
        goto exit_rl_query_yn;
      default:
        break;
    }
  }

exit_rl_query_yn:
  tio_return();
  tio_keypad(1);
  tio_flush();
  return choice;
}
/* rl_query_yn */

  static int
rl_get_position(void)
  /* return the position of the cursor in `rl.line'.
   */
{
  int i;
  int position;

  for (i = position = 0; rl.line[i] && position < rl_position; ++i) {
    ++position;
    if (rl.line[i] == RL_ESC) ++i;
  }
  return i;
}
/* rl_get_position */

  static int
rl_get_vposition(void)
  /* return the position of the cursor in `rl.vline'.
   */
{
  int i;
  int position;
  int vposition;
  int key;

  for (i = position = vposition = 0;
       rl.line[i] && position < rl_position;
       ++i, ++position) {
    if (rl.line[i] != RL_ESC)
      if (tio_isprint(rl.line[i]))
        ++vposition;
      else
        vposition += strlen(tio_keyrep(rl.line[i]));
    else {
      ++i;
      if (rl.line[i] == 1)
        /* ESCAPE-character */
        vposition += strlen(tio_keyrep(HXKEY_ESCAPE));
      else if (rl.line[i] == (int)HXKEY_NULL - HXKEY_BIAS)
        /* null-character */
        vposition += strlen(tio_keyrep(HXKEY_NULL));
      else {
        key = rl.line[i] + HXKEY_BIAS;
        vposition += strlen(tio_keyrep(key));
      }
    }
  }
  return vposition;
}
/* rl_get_position */

  static int
rl_get_length(struct rl_line_s *rrl)
  /* return the number of logical characters in `rrl->line'.  escape sequences
   * are counted as a single character.
   */
{
  int i;
  int length;

  for (i = length = 0; rrl->line[i]; ++i, ++length)
    if (rrl->line[i] == RL_ESC) ++i;
  return length;
}
/* rl_get_length */

  static char *
rl_make_vline_(struct rl_line_s *rrl)
  /* update the visible line `rrl->vline' from `rrl->line'.
   * the return value is `rrl->vline'.
   */
{
  int i;
  int vposition;
  int key;
  char *rep;

  for (i = vposition = 0; rrl->line[i]; ++i) {
    if (rrl->line[i] != RL_ESC)
      if (tio_isprint(rrl->line[i]))
        rrl->vline[vposition++] = rrl->line[i];
      else {
        strcpy(rrl->vline + vposition, rep = tio_keyrep(rrl->line[i]));
        vposition += strlen(rep);
      }
    else {
      ++i;
      if (rrl->line[i] == 1) {
        /* ESCAPE-character */
        strcpy(rrl->vline + vposition, rep = tio_keyrep(HXKEY_ESCAPE));
        vposition += strlen(rep);
      } else if (rrl->line[i] == (int)HXKEY_NULL - HXKEY_BIAS) {
        /* null-character */
        strcpy(rrl->vline + vposition, rep = tio_keyrep(HXKEY_NULL));
        vposition += strlen(rep);
      } else {
        key = rrl->line[i] + HXKEY_BIAS;
        strcpy(rrl->vline + vposition, rep = tio_keyrep(key));
        vposition += strlen(rep);
      }
    }
  }
  rrl->vline[vposition] = 0;
  return rrl->vline;
}
/* rl_make_vline_ */

  static int
rl_get_vlength(struct rl_line_s *rrl)
  /* return the number of  characters in `rrl->vline'.
   */
{
  rl_make_vline_(rrl);
  return strlen(rrl->vline);
}
/* rl_get_vlength */

  static char *
rl_make_vline(void)
  /* update the visible line `rl.vline' from `rl.line'.
   * the return value is `rl.vline'.
   */
{
  return rl_make_vline_(&rl);
}
/* rl_display_line */

  static int
rl_display_line(int clear_to_eol)
{
  char line[1024];

  if (clear_to_eol) {
    tio_return();
    if (rl_prompt) tio_printf("%s", rl_prompt);
  } else
    tio_goto_column(rl_prompt_len);
  if (clear_to_eol) tio_clear_to_eol();
  rl_make_vline();
  strncpy(line, rl.vline + rl_offset, hx_columns - rl_prompt_len);
  line[hx_columns - rl_prompt_len - 1] = 0;
  if (rl_offset) tio_printf(RL_LEFT_MORE);
  if (strlen(rl.vline) - rl_offset >= hx_columns - rl_prompt_len)
    line[hx_columns - rl_prompt_len - 2] = 0;
  tio_raw_printf("%s", line + !!rl_offset);
  if (rl_get_vlength(&rl) - rl_offset >= hx_columns - rl_prompt_len)
    tio_printf(RL_RIGHT_MORE);
  tio_goto_column(rl_prompt_len + rl_get_vposition() - rl_offset);
  return 0;
}
/* rl_display_line */

  static int
rl_insert(int x)
{
  int i;
  int last_col;
  int position = rl_get_position();
  int vposition;
  int append = (position == strlen(rl.line));
  char *s = strdup(tio_keyrep(x));
  int sl = strlen(s);
  int special_f = 0;
  int redisplay_f = 0;

  if (!tio_isprint(x) || x == RL_ESC || !x) special_f = 1;
  for (i = strlen(rl.line) - special_f; i >= position; --i)
    rl.line[i + 1 + special_f] = rl.line[i];
  if (special_f && (x == RL_ESC || x > 0xff || !x)) {
    rl.line[position] = RL_ESC;
    rl.line[position + 1] =
      (char)(x ? x == RL_ESC ? 1 : x - HXKEY_BIAS : HXKEY_NULL - HXKEY_BIAS);
  } else
    rl.line[position] = (char)x;
  rl_make_vline();
  vposition = rl_get_vposition();
  for (i = 0; i < sl; ++i) {
    if (vposition - rl_offset + 2 >= hx_columns - rl_prompt_len + append) {
      ++rl_offset;
      ++vposition;
      tio_goto_column(rl_prompt_len);
      if (tio_delete_character())
        redisplay_f = 1;
      else {
	tio_printf(RL_LEFT_MORE);
	last_col = rl_prompt_len + strlen(rl.vline) - rl_offset;
	if (last_col > hx_columns - 1) {
	  last_col = hx_columns - 1;
          if (!append) {
            tio_goto_column(last_col - 2);
            tio_clear_to_eol();
            tio_putchar(rl.vline[rl_offset + hx_columns - rl_prompt_len - 3]);
            tio_printf(RL_RIGHT_MORE);
          } else {
            tio_goto_column(last_col - 1);
            tio_clear_to_eol();
            tio_putchar(rl.vline[rl_offset + hx_columns - rl_prompt_len - 2]);
          }
          tio_goto_column(rl_prompt_len + vposition - rl_offset);
	} else {
	  tio_goto_column(last_col - 1);
	  tio_putchar(rl.vline[rl_offset + hx_columns - rl_prompt_len - 2]);
	}
      }
    } else {
      ++vposition;
      if (tio_insert_character(s[i]))
	redisplay_f = 1;
      else {
	last_col = rl_prompt_len + strlen(rl.vline) - rl_offset;
	if (last_col > hx_columns - 1) {
	  last_col = hx_columns - 1;
          if (!append) {
            tio_goto_column(last_col - 1);
            tio_clear_to_eol();
            tio_printf(RL_RIGHT_MORE);
          } else {
            tio_goto_column(last_col);
            tio_clear_to_eol();
          }
	  tio_goto_column(rl_prompt_len + vposition - rl_offset);
	}
      }
    }
  }
  ++rl_position;
  if (redisplay_f) rl_display_line(1);
  free(s);
  return 0;
}
/* rl_insert */

  static int
rl_delete(int under_cursor)
{
  int i;
  int last_col;
  int special_f = 0;
  int replen;
  int position = rl_get_position();
  int vposition;
  int key;

  if (!under_cursor && !position) return 0;
  if ((under_cursor && position < strlen(rl.line)) || !under_cursor) {
    if (!under_cursor) --rl_position;
    position = rl_get_position();
    if (rl.line[position] == RL_ESC) {
      key = rl.line[position + 1] + HXKEY_BIAS;
      if (key == 0xff) key = (int)HXKEY_ESCAPE;
      replen = strlen(tio_keyrep(key));
      special_f = 1;
    } else
      replen = strlen(tio_keyrep(rl.line[position]));
    for (i = position + 1 + special_f; i <= strlen(rl.line); ++i) 
      rl.line[i - 1 - special_f] = rl.line[i];
  } else
    return 0;

  rl_make_vline();
  vposition = rl_get_vposition();
  if (!under_cursor) {
    vposition += replen;
    while (replen--) {
      --vposition;
      if (vposition < rl_offset + 1 + !!rl_backspace_jump) {
	if (rl_backspace_jump && rl_offset) {
	  rl_offset -= rl_backspace_jump;
	  if (rl_backspace_jump == 1) {
	    if (rl_offset < 0) {
	      rl_offset = 0;
	      goto l1;
	    }
	    tio_goto_column(rl_prompt_len + 1);
	    tio_putchar(rl.vline[rl_offset + 1]);
	  } else {
	    if (rl_offset < 0) rl_offset = 0;
	    rl_display_line(0);
	  }
	} else {
	  --rl_offset;
	  if (rl_offset < 0) {
	    rl_offset = 0;
	    goto l1;
	  }
	}
      } else {
  l1:   tio_goto_column(rl_prompt_len + vposition - rl_offset);
        if (tio_delete_character())
          rl_display_line(1);
	last_col = rl_prompt_len + strlen(rl.vline) + replen - rl_offset;
	if (last_col > hx_columns - 1) {
	  last_col = hx_columns - 1;
	  tio_goto_column(last_col - 2);
	  tio_clear_to_eol();
	  tio_putchar(rl.vline[rl_offset + hx_columns - rl_prompt_len - 3
                               - replen]);
	  tio_printf(RL_RIGHT_MORE);
	} else if (last_col == hx_columns - 1) {
	  tio_goto_column(last_col - 2);
	  tio_clear_to_eol();
	  tio_putchar(rl.vline[rl_offset + hx_columns - rl_prompt_len - 3
                               - replen]);
	  tio_putchar(rl.vline[rl_offset + hx_columns - rl_prompt_len - 2
                               - replen]);
	} else if (rl_offset) {
	  --rl_offset;
	  if (!rl_offset) {
	    tio_goto_column(rl_prompt_len);
	    tio_putchar(rl.vline[0]);
	  } else
	    tio_goto_column(rl_prompt_len + 1);
	  if (tio_insert_character(rl.vline[rl_offset + 1]))
            rl_display_line(1);
	}
	tio_goto_column(rl_prompt_len + vposition - rl_offset);
      }
    }
  } else {
    while (replen--) {
      tio_goto_column(rl_prompt_len + vposition - rl_offset);
      if (tio_delete_character())
        rl_display_line(1);
      last_col = rl_prompt_len + strlen(rl.vline) - rl_offset;
      if (last_col > hx_columns - 1) {
	last_col = hx_columns - 1;
	tio_goto_column(last_col - 2);
	tio_clear_to_eol();
	tio_putchar(rl.vline[rl_offset + hx_columns - rl_prompt_len - 3
                             - replen]);
	tio_printf(RL_RIGHT_MORE);
      } else if (last_col == hx_columns - 1) {
	tio_goto_column(last_col - 2);
	tio_clear_to_eol();
	tio_putchar(rl.vline[rl_offset + hx_columns - rl_prompt_len - 3
                             - replen]);
	tio_putchar(rl.vline[rl_offset + hx_columns - rl_prompt_len - 2
                             - replen]);
      } else if (rl_offset) {
	--rl_offset;
	if (!rl_offset) {
	  tio_goto_column(rl_prompt_len);
	  tio_putchar(rl.vline[0]);
	} else
	  tio_goto_column(rl_prompt_len + 1);
	if (tio_insert_character(rl.vline[rl_offset + 1]))
          rl_display_line(1);
      }
      tio_goto_column(rl_prompt_len + vposition - rl_offset);
    }
  }
  rl_make_vline();
  return 0;
}
/* rl_delete */

  static int
rl_ck(void)
  /* clear all up to the end of line.
   */
{
  int length = strlen(rl.line);
  int i;

  for (i = rl_get_position(); i < length; ++i) rl.line[i] = 0;
  rl_make_vline();
  tio_clear_to_eol();
  return 0;
}
/* rl_ck */

  static int
rl_cu(void)
  /* clear all up to the beginning of the line.
   */
{
  int position = rl_get_position();
  int length = strlen(rl.line);
  int i;

  if (position) {
    for (i = 0; i < length - position; ++i)
      rl.line[i] = rl.line[i + position];
    for (; i < length; ++i) rl.line[i] = 0;
    rl_position = 0;
    rl_display_line(1);
  }
  return 0;
}
/* rl_cu */

  static int
rl_begin(void)
{
  rl_position = 0;
  if (rl_offset) {
    rl_offset = 0;
    rl_display_line(1);
  } else
    tio_goto_column(rl_prompt_len);
  return 0;
}
/* rl_begin */

  static int
rl_end(void)
{
  int length;
  int vposition;

  length = rl_get_length(&rl);
  rl_position = length;
  vposition = rl_get_vposition();
  if (rl_offset < vposition - hx_columns + rl_prompt_len + 1) {
    rl_offset = vposition - hx_columns + rl_prompt_len + 1;
    if (rl_offset < 0) rl_offset = 0;
    rl_display_line(1);
  } else
    tio_goto_column(rl_prompt_len + vposition - rl_offset);
  return 0;
}
/* rl_end */
  static int
rl_left(void)
{
  int last_col;
  int position;
  int vposition;
  int skip;
  int key;
  int redisplay_f = 0;

  if (!rl_position) return 0;
  --rl_position;
  position = rl_get_position();
  if (rl.line[position] == RL_ESC) {
    key = rl.line[position + 1] + HXKEY_BIAS;
    if (key == 0xff) key = (int)HXKEY_ESCAPE;
      /* translate `RL_ESC' to `HXKEY_ESCAPE' */
    skip = strlen(tio_keyrep(key));
  } else
    skip = strlen(tio_keyrep(rl.line[position]));
  while (skip--) {
    vposition = rl_get_vposition();
    if (vposition < rl_offset + 1) {
      --rl_offset;
      if (rl_offset < 0) {
	rl_offset = 0;
	tio_left(1);
      } else {
	if (!rl_offset) {
	  tio_goto_column(rl_prompt_len);
	  tio_putchar(rl.vline[0]);
	} else
	  tio_goto_column(rl_prompt_len + 1);
	if (tio_insert_character(rl.vline[rl_offset + 1])) redisplay_f = 1;
      }
      last_col = rl_prompt_len + strlen(rl.vline) - rl_offset;
      if (last_col > hx_columns - 1) {
	last_col = hx_columns - 1;
	tio_goto_column(last_col - 2);
	tio_clear_to_eol();
	tio_putchar(rl.vline[rl_offset + hx_columns + skip - rl_prompt_len - 3]);
	tio_printf(RL_RIGHT_MORE);
	tio_goto_column(rl_prompt_len + vposition + skip - rl_offset);
      } else if (last_col == hx_columns - 1) {
	tio_goto_column(last_col - 2);
	tio_clear_to_eol();
	tio_putchar(rl.vline[rl_offset + hx_columns + skip - rl_prompt_len - 3]);
	tio_putchar(rl.vline[rl_offset + hx_columns + skip - rl_prompt_len - 2]);
	tio_goto_column(rl_prompt_len + vposition + skip - rl_offset);
      }
    } else
      tio_left(1);
  };
  if (redisplay_f) rl_display_line(1);
  return 0;
}
/* rl_left */

  static int
rl_right(void)
{
  int last_col;
  int append = 0;
  int redisplay_f = 0;
  int skip;
  int position;
  int vposition;
  int key;

  if (rl_position == rl_get_length(&rl)) return 0;
  position = rl_get_position();
  if (rl.line[position] == RL_ESC) {
    key = rl.line[position + 1] + HXKEY_BIAS;
    if (key == 0xff) key = (int)HXKEY_ESCAPE;
    skip = strlen(tio_keyrep(key));
  } else
    skip = strlen(tio_keyrep(rl.line[position]));
  ++rl_position;
  position = rl_get_position();
  vposition = rl_get_vposition();
  while (skip--) {
    append = (position == strlen(rl.line));
    if (vposition - rl_offset + 1 >= hx_columns - rl_prompt_len + append) {
      ++rl_offset;
      tio_goto_column(rl_prompt_len);
      if (tio_delete_character()) redisplay_f = 1;
      else {
	tio_printf(RL_LEFT_MORE);
	last_col = rl_prompt_len + strlen(rl.vline) - rl_offset;
	if (last_col > hx_columns - 1) {
	  last_col = hx_columns - 1;
	  tio_goto_column(last_col - 2);
	  tio_clear_to_eol();
	  tio_putchar(rl.vline[rl_offset + hx_columns - rl_prompt_len - 3]);
	  tio_printf(RL_RIGHT_MORE);
	  tio_goto_column(rl_prompt_len + vposition - rl_offset);
	} else if (last_col == hx_columns - 1) {
	  tio_goto_column(last_col - 2);
	  tio_clear_to_eol();
	  tio_putchar(rl.vline[rl_offset + hx_columns - rl_prompt_len - 3]);
	  tio_putchar(rl.vline[rl_offset + hx_columns - rl_prompt_len - 2]);
	  tio_left(1);
	} else {
	  tio_goto_column(last_col - 1);
	  tio_putchar(rl.vline[rl_offset + hx_columns - rl_prompt_len - 3]);
	}
      }
    } else
      tio_right(1);
  };
  if (redisplay_f) rl_display_line(1);
  return 0;
}
/* rl_right */ 

  static int
rl_complete(int context, int again)
{
  char **list;
  char prefix[1024];
  char command[1024];
  char line[1024];
  char *p;
  int i, j;
  int prefix_len;
  int stop_f = 0;

  if (!completer) return 0;
  strcpy(line, rl.line);
  line[rl_position] = 0;
  for (i = 0, p = rl.line + rl_position;
       p > rl.line ? (p[-1] != ' ' && p[-1] != '\t') : 0;
       --p, ++i);
  if (i) { strncpy(prefix, p, i); prefix[i] = 0; } else *prefix = 0;
  prefix_len = strlen(prefix);
  while (p > rl.line) {
    if (p[-1] != ' ' && p[-1] != '\t') break;
    --p;
  }
  if (p == rl.line)
    /* the word typed is the first word in the line, so we'll complete
     * a command. */
    list = completer(prefix, "", line, context);
  else {
    for (i = 0; p > rl.line ? isalpha(p[-1]) : 0; --p, ++i);
    if (i) { strncpy(command, p, i); command[i] = 0; } else *command = 0;
    list = completer(prefix, command, line, context);
  }
  if (*list) { /* found at least one completion */
    if (list[1]) { /* more than one completion */
      char c;
      int len;
      /* find the longest unique prefix of all completions */
      for (i = 0, len = strlen(prefix); i < len; ++i)
        for (j = 0; list[j + 1]; ++j) assert(list[j][i] == list[j + 1][i]);
      for (stop_f = 1;; ++i, stop_f = 0) {
	if (!(c = list[0][i])) break;
        for (j = 1; list[j] ? list[j][i] == c : 0; ++j);
        if (list[j]) break;
        prefix[i] = c;
      }
      prefix[i] = 0;
      if (stop_f) {
	if (again) {
	  /* the completer-key has been hit twice on the same position.
	   * print out a completion list. */
	  int k, l, m, n;
	  char **list2;
	  rl_redisplay = 1;
	  for (n = 0; list[n]; ++n);
	  if (n > 99) {
	    char query[256];
	    int yn;
	    sprintf(query, "\nthere are %i possibilities - list them all", n);
	    yn = rl_query_yn(query, 0);
	    if (yn <= 0) goto dont_list;
	  }
	  /* find the maximum length of a completion */
	  for (i = l = 0; list[i]; ++i) if ((j = strlen(list[i])) > l) l = j;
	  m = hx_columns / ++l; /* number of hx_columns */
	  m += !m;
          util_strsort(list);
	  k = (n - 1) / m + 1; /* number of lines */
	  /* rearrange the list sorted in hx_columns */
	  list2 = (char **)malloc(m * k * sizeof(char *));
	  memset(list2, 0, m * k * sizeof(char *));
	  for (i = 0, j = 0; list[i]; ++i) {
	    list2[m * (i % k) + j] = list[i];
	    if (!((i + 1) % k)) ++j;
	  }
	  /* display the list */
	  for (i = 0; i < m * k; ++i) {
	    if (!(i % m)) tio_putchar('\n');
	    if (list2[i]) {
	      tio_printf("%s", list2[i]);
	      j = strlen(list2[i]);
	    } else
	      j = 0;
	    for (; j < l; ++j) tio_putchar(' ');
	  }
	  free((char *)list2);
dont_list:
	  tio_putchar('\n');
	  tio_return();
	  tio_puts(rl_prompt);
	  rl_display_line(0);
	} else
	  tio_bell();
      } else {
	/* comlpete as far as unique */
	int k = strlen(prefix);
	assert(k > prefix_len);
	for (i = strlen(rl.line) + (j = k - prefix_len);
             i >= rl_position;
             --i)
	  rl.line[i + j] = rl.line[i];
	for (i = 0; i < j; ++i)
	  rl.line[rl_position + i] = prefix[prefix_len + i];
	rl_position += j;
	while (rl_position - rl_offset + 1 >= hx_columns - rl_prompt_len)
	  ++rl_offset;
	tio_return();
	tio_puts(rl_prompt);
	rl_display_line(0);
	tio_bell();
      }
    } else { /* exactly one completion */
      int dir_f;
      j = strlen(*list) - prefix_len + 1;
      dir_f = (*list)[strlen(*list) - 1] == '/';
      if (dir_f) --j;
      for (i = strlen(rl.line) + j; i >= rl_position; --i)
	rl.line[i + j] = rl.line[i];
      for (i = 0; i < j - !dir_f; ++i)
	rl.line[rl_position + i] = list[0][prefix_len + i];
      if (!dir_f) rl.line[rl_position + i] = ' ';
      rl_position += j;
      while (rl_position - rl_offset + 1 >= hx_columns - rl_prompt_len)
	++rl_offset;
      tio_return();
      tio_puts(rl_prompt);
      rl_display_line(0);
    }
  } else /* no match */
    tio_bell();
  for (i = 0; list[i]; ++i) free((char *)list[i]);
  free((char *)list);
  return stop_f;
}
/* rl_complete */

  static void
rl_verbatim(void)
{
  int key;
  int position = rl_get_position();
  int append = (position == strlen(rl.line));
  int vposition = rl_get_vposition();
  int last_col;

  if (vposition - rl_offset + 2 >= hx_columns - rl_prompt_len + append) {
    ++rl_offset;
    tio_goto_column(rl_prompt_len);
    if (tio_delete_character())
      rl_display_line(1);
    else {
      tio_printf(RL_LEFT_MORE);
      last_col = rl_prompt_len + strlen(rl.vline) - rl_offset;
      if (last_col > hx_columns - 1) {
        last_col = hx_columns - 1;
        tio_goto_column(last_col - 1);
        tio_clear_to_eol();
        tio_printf(RL_RIGHT_MORE);
        tio_goto_column(rl_prompt_len + vposition - rl_offset);
      } else {
        tio_goto_column(last_col);
        tio_putchar(rl.vline[rl_offset + hx_columns - rl_prompt_len - 2]);
      }
    }
  }
  tio_printf("^@m-000-001");
restart:
  if ((key = tio_mgetch(MAP_EXH, 0)) == (int)HXKEY_ERROR) {
    if (window_changed) {
      tio_ungetch('v' & 0x1f);
      tio_ungetch(HXKEY_ERROR);
      return;
    } else
      goto restart;
  }
  rl_make_vline();
  vposition = rl_get_vposition();
  tio_printf("%c@m-000-001", rl.vline[vposition] ? rl.vline[vposition] : ' ');
  rl_insert(key);
}
/* rl_verbatim */

  char *
readline(prompt, default_val, context)
  char *prompt;
  char *default_val;
  int context;
{
  int key;
  int escape_f = 0;
  int echo_state;
  struct rl_line_s *hist;
  static int history_initialized = 0;
  int stop_f = 0;

  rl_redisplay = 0;
  if (!history_initialized) rl_history_init(16), history_initialized = 1;
  rl_history_reset(context);

  tio_keypad(1);
  echo_state = tio_echo(0);
  tio_return();
  tio_clear_to_eol();
  tio_normal();
  tio_set_cursor(1);
  if (prompt) tio_printf("%s", prompt);
  memset(rl.line, 0, LINE_MAXLEN);
  rl_prompt = prompt;
  rl_prompt_len = strlen(prompt);
  rl_position = 0;
  rl_redisplay = 0;
  if (default_val ? *default_val : 0) {
    rl_default = default_val;
    strncpy(rl.line, default_val, LINE_MAXLEN - 1);
    rl_make_vline();
    if (rl_prompt_len + rl_position > hx_columns - 2)
      rl_offset = hx_columns - 1 - rl_prompt_len - strlen(rl.vline);
    tio_raw_printf("%s", rl.line + rl_offset);
    tio_goto_column(rl_prompt_len + strlen(rl.vline) - rl_offset);
  }
  for (;;) {
    key = tio_mgetch(MAP_EXH, 0);
    switch (key) {
    case 'a' & 0x1f: /* C-a */
      rl_begin();
      break;
    case 'e' & 0x1f: /* C-e */
      rl_end();
      break;
    case 'k' & 0x1f: /* C-k */
      rl_ck();
      break;
    case 'u' & 0x1f: /* C-u */
      rl_cu();
      break;
    case HXKEY_LEFT:
      rl_left();
      break;
    case HXKEY_RIGHT:
      rl_right();
      break;
    case HXKEY_UP:
      rl_history_set(rl);
      if (!(hist = rl_history_up())) break;
      strcpy(rl.line, hist->line);
      rl_offset = 0;
      if (rl_prompt_len + rl_position > hx_columns - 2)
        rl_offset = rl_prompt_len + strlen(rl.vline) - hx_columns + 1;
      rl_display_line(1);
      break;
    case HXKEY_DOWN:
      rl_history_set(rl);
      if (!(hist = rl_history_down())) break;
      strcpy(rl.line, hist->line);
      rl_offset = 0;
      if (rl_prompt_len + rl_position > hx_columns - 2)
        rl_offset = hx_columns - 2 - rl_prompt_len - strlen(rl.vline);
      rl_display_line(1);
      break;
    case HXKEY_ESCAPE:
      escape_f = 1;
      /* fall through */
    case HXKEY_RETURN:
      tio_return();
      tio_flush();
      goto exit_readline;
    case HXKEY_BACKSPACE:
    case HXKEY_DELETE:
      if (!*rl.line && rl_cancel_on_bs) {
        escape_f = 1;
        goto exit_readline;
      }
      rl_delete(0);
      break;
    case 'd' & 0x1f: /* C-d */
      rl_delete(1);
      break;
    case 'l' & 0x1f: /* C-l */
      rl_display_line(1);
      break;
    case 'v' & 0x1f: /* C-v */
      rl_verbatim();
      break;
    case HXKEY_TAB:
      stop_f = rl_complete(context, stop_f);
      if (stop_f) continue;
      break;
    case HXKEY_ERROR:
      if (window_changed) {
        rl_winch();
        tio_goto_line(hx_lines - 1);
        tio_return();
        tio_clear_to_eol();
        rl_display_line(1);
        window_changed = 0;
      }
      break;
    default:
      if (tio_isprint(key)) rl_insert((char)key);
      break;
    }
    stop_f = 0;
  }
exit_readline:
  if (!escape_f && *rl.line) rl_history_add(rl);
  tio_echo(echo_state);
  return escape_f ? 0 : rl.line;
}
/* readline */

/* end of readline.c */


/* VIM configuration: (do not delete this line)
 *
 * vim:bk:nodg:efm=%f\:%l\:%m:hid:icon:
 * vim:sw=2:sm:textwidth=79:ul=1024:wrap:
 */
