/* tio.c	8/19/1995
 * Terminal I/O, version 1.0
 *
 * This file provides a set of functions for manipulating a terminal screen
 * and reading from the keyboard.
 * Required:
 *   libtermcap.a or termlib.o
 *   set.o (only if `USE_SET' is set)
 */

/* Copyright (c) 1995,1996 Sascha Demetrio
 * Copyright (c) 2009, 2015 Peter Pentchev
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

#define _GNU_SOURCE

/* ANSI C
 */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <memory.h>

/* UNIX
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#ifndef BSD
#include <termio.h>
#else
#include <termios.h>
#endif
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <curses.h>
#include <term.h>

#ifndef bzero
#define bzero(a, b) memset((a), 0, (b))
#endif

#ifndef USE_SET
#define USE_SET 1
#endif

#ifndef HAVE_NCURSES
#define HAVE_NCURSES 0
#endif

#include "defs.h"
#include "tio.h"
#include "util.h"

#if USE_SET
#include "set.h"
#endif

int tio_readwait_timeout = TIO_READWAIT_TIMEOUT;

#if !HAVE_VASPRINTF
static int my_vasprintf(char ** const dst, const char * const fmt, va_list v)
{
  char * const p = malloc_fatal(2048);
  const int n = vsnprintf(p, 2048, fmt, v);
  if (n < 0 || n >= 2048) {
    free(p);
    return -1;
  }
  *dst = p;
  return n;
}
#undef vasprintf
#define vasprintf(dst, fmt, v) my_vasprintf((dst), (fmt), (v))
#endif

volatile int *tio_interrupt;

  static int
outc(int c)
{ 
  putc(c, stdout);
  return c;
}
/* outc */

static char tio_unread_buffer[4096];
static int tio_unread_count;
  /* Used by `tio_read()' ans `tio_unread()'.
   */

static int tio_unget_buffer[TIO_MAX_UNGET];
static int tio_unget_count = 0;
  /* Used by `tio_getch_()' and `tio_ungetch()'.
   */

static int t_line;
static int t_column;
  /* Current cursor position.  A negative value indicates that the value is
   * unknown.
   */

static int t_echo;
  /* Current echo-state: 1: echo is on; 0: echo is off.
   */

static int t_keypad_transmit;
  /* A non-zero value indicates that the terminal is in keypad transmit mode.
   * The default (set by `tio_init()') is 0 (off).
   */

static int t_application_mode;
  /* A non-zero value indicates that the terminal is application mode.
   * The default (set by `tio_init()') is 0 (off).
   */

static int t_timeout;
  /* Timeout for reading key strings from the keyboard.
   */

/* recover information for `tio_suspend()' and `tio_restart()'.
 */
static struct termios ts_recover;
static int t_keypad_transmit_recover;
static int t_application_mode_recover;

#if HAVE_SIGTYPE_INT
#define SIG_RVAL 0
typedef int sigtype_t;
#else
#define SIG_RVAL
typedef void sigtype_t;
#endif

#ifdef SIGWINCH
static sigtype_t	 sigwinch_handler(int);
#endif
#if 0
static void		 tio_listcaps(void);
#endif
static void		 tio_reset_scrolling_region(void);
static int		 tio_set_scrolling_region(int first, int last);

#define T_REQ_MAX_CAPS 32
  /* The maximum length of a list of requirements.
   */

#define T_TABSTOP 8

const char *terminal_name = 0;

int tio_tite_f = 0;
  /* if `tio_tite_f' is set, no "ti"/"te" commands will be sent to the
   * terminal.
   */

static void tio_error_msg( const char *fmt, ... ) __printflike(1, 2);
static void tio_warning_msg( const char *fmt, ... ) __printflike(1, 2);
void (*error_msg)( const char *, ... ) __printflike(1, 2) = tio_error_msg;
void (*warning_msg)( const char *, ... ) __printflike(1, 2) = tio_warning_msg;
void (*tio_winch)(void);

int hx_lines; /* Number of lines. */
int hx_columns; /* Number of columns. */
int window_changed;
  /* A non-zero value indicates, that the window size has changed.
   */

static int t_fg = 7;  /* foreground color */
static int t_bg = 0;  /* background color */

static char t_bp[4096];
static char t_string_buffer[8192];
static const char *program_name;
static struct termios ts_start;

const char *t_clear_screen;     /* cl */
const char *t_clear_to_eol;     /* ce */
const char *t_clear_to_bol;     /* cb */
const char *t_clear_to_eos;     /* cd */
const char *t_home;             /* ho */
const char *t_last_line;        /* ll */
const char *t_return;           /* cr */
const char *t_reset;            /* rs */
const char *t_bell;             /* bl */
const char *t_visible_bell;     /* vb */

const char *t_tc_init;          /* ti */
const char *t_tc_end;           /* te */
const char *t_kp_transmit_on;   /* ks */
const char *t_kp_transmit_off;  /* ke */

const char *t_blink_on;         /* mb */
const char *t_bold_on;          /* md */
const char *t_half_bright_on;   /* mh */
const char *t_reverse_on;       /* mr */
const char *t_standout_on;      /* so */
const char *t_underscore_on;    /* us */
const char *t_underscore_off;   /* ue */
const char *t_all_off;          /* me */

const char *t_UP;               /* UP */
const char *t_DOwn;             /* DO */
const char *t_LEft;             /* LE */
const char *t_RIght;            /* RI */
const char *t_up;               /* up */
const char *t_down;             /* do */
const char *t_left;             /* le */
const char *t_right;            /* nd */
const char *t_cm;               /* cm */
const char *t_CM;               /* CM */
const char *t_goto_column;      /* ch */
const char *t_goto_line;        /* cv */
const char *t_save_position;    /* sc */
const char *t_restore_position; /* rc */
const char *t_delete_line;      /* dl */
const char *t_delete_lines;     /* DL */
const char *t_insert_line;      /* al */
const char *t_insert_lines;     /* AL */
const char *t_delete_chars;     /* DC */
const char *t_delete_char;      /* dc */
const char *t_delete_mode;      /* dm */
const char *t_end_delete_mode;  /* ed */
const char *t_insert_chars;     /* IC */
const char *t_insert_char;      /* ic */
const char *t_insert_mode;      /* im */
const char *t_end_insert_mode;  /* ei */
const char *t_scroll_fwd_n;     /* SF (NP*) */
const char *t_scroll_backwd_n;  /* SR (NP*) */
const char *t_scroll_fwd;       /* sf */
const char *t_scroll_backwd;    /* sr */
const char *t_change_scroll;    /* cs (NP) */

const char *t_standout_cursor;  /* vs */
const char *t_normal_cursor;    /* ve */
const char *t_invisible_cursor; /* vi */

const char *t_key_delete;       /* kD */
const char *t_key_backspace;    /* kb */
const char *t_key_tab;          /* kT */
const char *t_key_return = "\n";
const char *t_key_tilde = "~";
const char *t_key_hat = "^";
const char *t_key_null = "";
const char *t_key_break = "\3";

const char *t_key_up;           /* ku */
const char *t_key_down;         /* kd */
const char *t_key_left;         /* kl */
const char *t_key_right;        /* kr */
const char *t_key_f0;           /* k0 */
const char *t_key_f1;           /* k1 */
const char *t_key_f2;           /* k2 */
const char *t_key_f3;           /* k3 */
const char *t_key_f4;           /* k4 */
const char *t_key_f5;           /* k5 */
const char *t_key_f6;           /* k6 */
const char *t_key_f7;           /* k7 */
const char *t_key_f8;           /* k8 */
const char *t_key_f9;           /* k9 */
const char *t_key_f10;          /* k; */
const char *t_key_backtab;      /* kB */
const char *t_key_begin;        /* @1 */
const char *t_key_cancel;       /* @2 */
const char *t_key_close;        /* @3 */
const char *t_key_command;      /* @4 */
const char *t_key_copy;         /* @5 */
const char *t_key_create;       /* @6 */
const char *t_key_end;          /* @7 */
const char *t_key_enter;        /* @8 */
const char *t_key_exit;         /* @9 */
const char *t_key_upper_left;   /* K1 */
const char *t_key_upper_right;  /* K3 */
const char *t_key_center;       /* K2 */
const char *t_key_bottom_left;  /* K4 */
const char *t_key_bottom_right; /* K5 */
const char *t_key_home;         /* ho */
const char *t_key_page_up;      /* kP */
const char *t_key_page_down;    /* kN */

/* terminfo variables
 */
const char *t_setab;            /* AB */
const char *t_setaf;            /* AF */


static struct t_strings_s {
  const char *id;
  const char **string;
} t_strings[] = {
  /* Codes sent by special keys.
   */
  { "kd", &t_key_down },
  { "ku", &t_key_up },
  { "kl", &t_key_left },
  { "kr", &t_key_right },
  { "kD", &t_key_delete },
  { "kb", &t_key_backspace },
  { "kT", &t_key_tab },
  { "k0", &t_key_f0 },
  { "k1", &t_key_f1 },
  { "k2", &t_key_f2 },
  { "k3", &t_key_f3 },
  { "k4", &t_key_f4 },
  { "k5", &t_key_f5 },
  { "k6", &t_key_f6 },
  { "k7", &t_key_f7 },
  { "k8", &t_key_f8 },
  { "k9", &t_key_f9 },
  { "k;", &t_key_f10 },
  { "kB", &t_key_backtab },
  { "@1", &t_key_begin },
  { "@2", &t_key_cancel },
  { "@3", &t_key_close },
  { "@4", &t_key_command },
  { "@5", &t_key_copy },
  { "@6", &t_key_create },
  { "@7", &t_key_end },
  { "@8", &t_key_enter },
  { "@9", &t_key_exit },
  { "K1", &t_key_upper_left },
  { "K3", &t_key_upper_right },
  { "K2", &t_key_center },
  { "K4", &t_key_bottom_left },
  { "K5", &t_key_bottom_right },
  { "ho", &t_key_home },
  { "kP", &t_key_page_up },
  { "kN", &t_key_page_down },

  /* Codes to control the terminal.
   */
  { "ce", &t_clear_to_eol },
  { "cb", &t_clear_to_bol }, /* clear to beginning of line */
  { "cd", &t_clear_to_eos }, /* clear to end of screen */
  { "cl", &t_clear_screen },
  { "ho", &t_home },
  { "ll", &t_last_line },
  { "cr", &t_return },
  { "rs", &t_reset },
  { "bl", &t_bell },
  { "vb", &t_visible_bell },
  { "ti", &t_tc_init }, /* begin application mode */
  { "te", &t_tc_end }, /* end application mode */
  { "ks", &t_kp_transmit_on },
  { "ke", &t_kp_transmit_off },
  { "dl", &t_delete_line },
  { "DL", &t_delete_lines },
  { "al", &t_insert_line },
  { "AL", &t_insert_lines },
  { "DC", &t_delete_chars },
  { "dc", &t_delete_char },
  { "dm", &t_delete_mode },
  { "ed", &t_end_delete_mode },
  { "IC", &t_insert_chars },
  { "ic", &t_insert_char },
  { "im", &t_insert_mode },
  { "ei", &t_end_insert_mode },
  { "SF", &t_scroll_fwd_n },
  { "SR", &t_scroll_backwd_n },
  { "sf", &t_scroll_fwd },
  { "sr", &t_scroll_backwd },
  { "cs", &t_change_scroll },

  { "mb", &t_blink_on },
  { "md", &t_bold_on },
  { "mh", &t_half_bright_on },
  { "mr", &t_reverse_on },
  { "so", &t_standout_on },
  { "us", &t_underscore_on },
  { "ue", &t_underscore_off },
  { "me", &t_all_off },
  { "vs", &t_standout_cursor },
  { "ve", &t_normal_cursor },
  { "vi", &t_invisible_cursor },

  /* terminfo capabilities
   */
  { "AB", &t_setab },
  { "AF", &t_setaf },

  /* Moving the cursor.
   */
  { "cm", &t_cm },
  { "CM", &t_CM }, /* relative move */
  { "ch", &t_goto_column },
  { "cv", &t_goto_line },
  { "sc", &t_save_position },
  { "rc", &t_restore_position },
  { "up", &t_up },
  { "do", &t_down },
  { "le", &t_left },
  { "nd", &t_right },
  { "UP", &t_UP },
  { "DO", &t_DOwn },
  { "LE", &t_LEft },
  { "RI", &t_RIght },
  { 0, 0 }
};

static struct t_require_s {
  const char *required[T_REQ_MAX_CAPS];
    /* A null-terminated list of string capabisities.  At least one of the
     * capabilities in the list has to be present in the termcap entry.
     * At most `T_REQ_MAX_CAPS' capabilities may be listed.
     */
  const char *message;
    /* Message to be printed in case the requirement isn't met.  The
     * `message' may contain one or more `%s'.  These are replaced by the
     * capabilities in the `required' list.  At most 8 `%s' may appear in
     * `message'.  The message is prefixed with the string defined by
     * `TERMINAL_CANT' when printed, unless `message' begins with a
     * backspace character ('\b').
     */
  int error;
    /* If `error' is non-zero, the initialization fails if the requirement
     * is not met, else only a warning message is printed.
     */
} t_required[] = {
  { { "cm", 0 }, "move cursor (\"%s\").", 1 },
  { { "ce", 0 }, "clear to end of line.", 0 },
  { { "up", "UP", 0 }, "move up (\"%s\" or \"%s\").", 1 },
  { { "le", "LE", "nd", 0 }, "move left (\"%s\", \"%s\" or \"%s\").", 1 },
  { { "nd", "RI", 0 }, "move right (\"%s\" or \"%s\").", 1 },
  { { "dl", "DL", 0 }, "delete line(s) (\"%s\" or \"%s\").", 0 },
  { { "al", "AL", 0 }, "insert line(s) (\"%s\" or \"%s\").", 0 },
  { { "md", "mh", "mr", "so", "us" },
    "\bNo standout mode (\"%s\", \"%s\", \"%s\", \"%s\" or \"%s\").", 0 },
  { { 0 }, 0, 0 }
};

static struct t_keys_s {
  enum t_keys_e key; /* Internal representation for the key. */
  const char **string;     /* String sent by the terminal. */
  const char *name;        /* Name of key. */
  const char *srep;        /* String representation of the key (i.e. ~1F for F1) */
} t_keys[] = {
  { HXKEY_DELETE, &t_key_delete, "delete", "~DEL" },
  { HXKEY_BACKSPACE, &t_key_backspace, "backspace", "~BS" },
  { HXKEY_TAB, &t_key_tab, "tab", "~TAB" },
  { HXKEY_RETURN, &t_key_return, "return", "~RET" },
  { HXKEY_UP, &t_key_up, "up", "~UP" },
  { HXKEY_DOWN, &t_key_down, "down", "~DOWN" },
  { HXKEY_LEFT, &t_key_left, "left", "~LEFT" },
  { HXKEY_RIGHT, &t_key_right, "right", "~RIGHT" },
  { HXKEY_F0, &t_key_f0, "f0", "~0F" },
  { HXKEY_F1, &t_key_f1, "f1", "~1F" },
  { HXKEY_F2, &t_key_f2, "f2", "~2F" },
  { HXKEY_F3, &t_key_f3, "f3", "~3F" },
  { HXKEY_F4, &t_key_f4, "f4", "~4F" },
  { HXKEY_F5, &t_key_f5, "f5", "~5F" },
  { HXKEY_F6, &t_key_f6, "f6", "~6F" },
  { HXKEY_F7, &t_key_f7, "f7", "~7F" },
  { HXKEY_F8, &t_key_f8, "f8", "~8F" },
  { HXKEY_F9, &t_key_f9, "f9", "~9F" },
  { HXKEY_F10, &t_key_f10, "f10", "~10F" },
  { HXKEY_BACKTAB, &t_key_backtab, "backtab", "~BT" },
  { HXKEY_BEGIN, &t_key_begin, "begin", "~BEGIN" },
  { HXKEY_CANCEL, &t_key_cancel, "cancel", "~CANCEL" },
  { HXKEY_CLOSE, &t_key_close, "close", "~CLOSE" },
  { HXKEY_COPY, &t_key_copy, "copy", "~COPY" },
  { HXKEY_CREATE, &t_key_create, "create", "~CREATE" },
  { HXKEY_END, &t_key_end, "end", "~END" },
  { HXKEY_ENTER, &t_key_enter, "enter", "~ENTER" },
  { HXKEY_EXIT, &t_key_exit, "exit", "~EXIT" },
  { HXKEY_UPPER_LEFT, &t_key_upper_left, "upper left", "~UL" },
  { HXKEY_UPPER_RIGHT, &t_key_upper_right, "upper right", "~UR" },
  { HXKEY_CENTER, &t_key_center, "center", "~CENTER" },
  { HXKEY_BOTTOM_LEFT, &t_key_bottom_left, "bottom left", "~BL" },
  { HXKEY_BOTTOM_RIGHT, &t_key_bottom_right, "bottom right", "~BR" },
  { HXKEY_HOME, &t_key_home, "home", "~HOME" },
  { HXKEY_PAGE_UP, &t_key_page_up, "page up", "~PGUP" },
  { HXKEY_PAGE_DOWN, &t_key_page_down, "page down", "~PGDOWN" },
  { (enum t_keys_e)'~', &t_key_tilde, "tilde", "~TILDE" },
  { (enum t_keys_e)'^', &t_key_hat, "hat", "~HAT" },
  { HXKEY_NULL, &t_key_null, "null", "^@" },
  { HXKEY_BREAK, &t_key_break, "break", "~BREAK" },
  { (enum t_keys_e)0, 0, 0, 0 }
};
#define HXKEY_ESCAPE_NAME "escape"

#ifndef HEXER_ASCII_ONLY
#define HEXER_ASCII_ONLY 0
#endif
  /* `HEXER_ASCII_ONLY' is relevant only if `USE_SET == 0'.
   */

  int
tio_isprint(int x)
{
  if (x < 0) x += 0x100;
  if (x < 0x20) return 0;
  if (x > 0xff) return 0;
#if USE_SET
  if (s_get_option_bool("ascii")) {
    if (x > 0x7e) return 0;
  } else
    if (x > 0x7e && x < 0xa1) return 0;
#else
#if HEXER_ASCII_ONLY
  if (x > 0x7e) return 0;
#else /* Assume ISO 8859/1 character set. */
  if (x > 0x7e && x < 0xa1) return 0;
#endif
#endif
  return 1;
}
/* tio_isprint */

  static void
tio_error_msg(const char * const fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  fprintf(stderr, "%s: ", program_name);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
}
/* tio_error_msg */

  static void
tio_warning_msg(const char * const fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  fprintf(stderr, "%s: warning: ", program_name);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
}
/* tio_warning_msg */

  static const char *
tio_cap(const char *id)
  /* Returns the string for capability `id'.  This function woks only
   * for capabilities that have already been read from the termcap entry.
   */
{
  int i;

  for (i = 0; t_strings[i].id ? strcmp(t_strings[i].id, id) : 1; ++i);
  return *t_strings[i].string;
}
/* tio_cap */

#if 0
  static void
tio_listcaps(void)
  /* Prints all capabilities found in termcap to stdout.
   */
{
  int i, j;

  for (i = 0, j = 1; t_strings[i].id; ++i, ++j) {
    if (*t_strings[i].string) tio_printf("%s ", t_strings[i].id);
    if (j > 20) {
      j = 0;
      tio_printf("\n");
    }
  }
  tio_printf("\n");
  tio_flush();
}
/* tio_listcaps */
#endif

  static int
tio_command(const char * const cmd, const int affcnt, ...)
  /* send the command `cmd' to the terminal.
   * return value: 0 if all goes well and -1 on error.
   */
{
  va_list ap;
  int arg1, arg2;

  if (!cmd) return -1;
  va_start(ap, affcnt);
  arg1 = va_arg(ap, int);
  arg2 = va_arg(ap, int);
  tputs(tgoto(cmd, arg2, arg1), affcnt, outc);
  return 0;
}
/* tio_command */

  int
tio_init(const char * const prog)
  /* Initialize.  This function should be called before any other
   * `tio_*'-function.  `prog' should be the name of the application.
   * If you call `tio_init()' with `prog == 0', the program name is set
   * to `"??"' when called fir the first time or left unchanged if
   * `tio_init()' has been called before.
   */
{
  static int init_f = 0;
  static char stdout_buffer[STDOUT_BUFFER_SIZE];
  char *buf = t_string_buffer;
  char **area = &buf;
  int i;

  if (!init_f) {
    if (!(terminal_name = getenv("TERM"))) terminal_name = "dumb";
#if USE_SET
  } else {
    terminal_name = s_get_option_string("TERM");
#endif
  }

  t_line = -1;
  t_column = -1;

  /* Read the termcap entry for `terminal_name' to `t_bp'.
   */
  switch (tgetent(t_bp, terminal_name)) {
  case -1:
    error_msg("no termcap file found.\n");
    goto fail;
  case 0:
    error_msg("can't get termcap entry for `%s'.\n", terminal_name);
    goto fail;
  default:
    break;
  }

  /* Read string capabilities an check if we found all needed capabilies.
   */
  for (i = 0; t_strings[i].id; ++i)
    *(t_strings[i].string) = tgetstr(t_strings[i].id, area);
  if (!t_return) t_return = "\r";
  if (!t_key_backspace) t_key_backspace = "\b";
  if (!t_key_delete) t_key_delete = "\177";
    /* If the string capabilities "cr", "kD" or "kb" are not found in the
     * termcap entry, use ASCII-codes.
     */
  for (i = 0; *t_required[i].required; ++i) {
    const char **p;
    int met;
    for (p = t_required[i].required, met = 0; *p; ++p)
      if (tio_cap(*p)) { met = 1; break; }
    if (!met) {
      if (!init_f) {
        char message[1024], *m = message;
        int j;
        int prefix = 1;
        /* We don't want to redisplay all termcap warnings every time the
         * window is resized...
         */
        for (j = 0; t_required[i].required[j]; ++j);
        for (++j; j < T_REQ_MAX_CAPS; ++j) t_required[i].required[j] = 0;
        if (*t_required[i].message == '\b')
          prefix = 0;
        else {
          sprintf(m, "%s ", TERMINAL_CANT);
          m += strlen(m);
        }
        sprintf(m, t_required[i].message + !prefix, t_required[i].required[0],
                t_required[i].required[1], t_required[i].required[2],
                t_required[i].required[3], t_required[i].required[4],
                t_required[i].required[5], t_required[i].required[6],
                t_required[i].required[7]);
        if (t_required[i].error) {
          error_msg("%s", message);
          goto fail;
        } else
          warning_msg("%s", message);
      }
    }
  }

  /* Try to figure out the size of the screen.
   */
  for (;;) {
    char *p;
    /* Get the screen size via `ioctl()'.
     */
#ifdef TIOCGWINSZ
    struct winsize size1;
#endif
#ifdef TIOCGSIZE
    struct ttysize size2;
    if (!ioctl(0, TIOCGSIZE, &size2)) {
      hx_lines = size2.ts_lines;
      hx_columns = size2.ts_cols;
      if (hx_lines > 0 && hx_columns > 0) break;
    }
#endif
#ifdef TIOCGWINSZ
    if (!ioctl(0, TIOCGWINSZ, &size1)) {
      hx_lines = size1.ws_row;
      hx_columns = size1.ws_col;
      if (hx_lines > 0 && hx_columns > 0) break;
    }
#endif
    /* `ioctl()' didn't work. See if we can get the screen size form the
     * the environment variables `LINES' ans `COLUMNS'.
     */
    if ((p = getenv("LINES"))) hx_lines = atoi(p);
    if ((p = getenv("COLUMNS"))) hx_columns = atoi(p);
    if (hx_lines > 0 && hx_columns > 0) break;
    /* Hmm... didn't work... we gotta believe what termcap tells us :-|
     */
    hx_lines = tgetnum("li");
    hx_columns = tgetnum("co");
    if (hx_lines > 0 && hx_columns > 0) break;
    /* Yuck!  Default the screen size to 80x24.
     */
    hx_lines = 24;
    hx_columns = 80;
    break;
  }

  if (!init_f) {
    /* The `tio_init()' function may be called more than once (i.e. the
     * `SIGWINCH'-handler calls `tio_init()').  The following stuff should
     * be done only when `tio_init()' is called for the first time.
     */
    init_f = 1;
    tcgetattr(0, &ts_start); /* Remember the terminal settings. */
    setvbuf(stdout, stdout_buffer, _IOFBF, STDOUT_BUFFER_SIZE);
      /* Turn on block buffering for `stdout'.
       */
    program_name = "??";
    tio_unread_count = 0;
    window_changed = 0;
    t_keypad_transmit = 0;
    t_application_mode = 0;
    if (t_kp_transmit_off) tio_command(t_kp_transmit_off, 1);
    t_timeout = 10; /* 1 second. */
  }
  if (prog) program_name = prog;

#ifdef SIGWINCH
  {
#ifdef SV_INTERRUPT
    static struct sigvec vec;
    vec.sv_handler = (void (*)(int))sigwinch_handler;
    vec.sv_mask = 0;
    vec.sv_flags = SV_INTERRUPT;
    sigvec(SIGWINCH, &vec, 0);
#else
#ifdef SA_INTERRUPT
    static struct sigaction act;
    act.sa_handler = sigwinch_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_INTERRUPT;
    sigaction(SIGWINCH, &act, 0);
#else
    signal(SIGWINCH, sigwinch_handler);
    /* siginterrupt(SIGWINCH, 1); */
#endif
#endif
  }
#endif
  tio_echo(1);  /* Set echo on. */
  return 0;

fail:
  return -1;
}
/* tio_init */

  void
tio_return(void)
{
  tio_command(t_return, 1);
  t_column = 0;
}
/* tio_return */

  void
tio_up(int count)
  /* Move cursor up `count' lines.
   */
{
  if (!count) return;
  if (t_line < 0)
    tio_goto_line(0);
  else {
    if (count > t_line) count = t_line;
    if (!count) return;
    t_line -= count;
    if (t_UP)
      tio_command(t_UP, count, count);
    else
      while (count--) { tio_command(t_up, 1); }
  }
}
/* tio_up */

  void
tio_down(int count)
  /* Move cursor down `count' lines.
   */
{
  if (!count) return;
  if (t_line < 0) {
    tio_goto_line(0);
    tio_down(count);
  } else {
    if (t_line + count > hx_lines - 1) count = hx_lines - t_line;
    if (!count) return;
    t_line += count;
    if (t_DOwn)
      tio_command(t_DOwn, count, count);
    else if (t_down)
      while (count--) { tio_command(t_down, 1); }
    else
      while (count--) { tio_command("\n", 1); }
  }
}
/* tio_down */

  void
tio_left(int count)
  /* Move cursor left `count' lines.
   */
{
  if (!count) return;
  if (t_line < 0)
    tio_goto_column(0);
  else {
    if (count > t_column) count = t_column;
    if (!count) return;
    t_column -= count;
    if (t_LEft)
      tio_command(t_LEft, count, count);
    else if (t_left)
      while (count--) { tio_command(t_left, 1); }
    else
      while (count--) putchar('\b');
  }
}
/* tio_left */

  void
tio_right(int count)
  /* Move cursor right `count' lines.
   */
{
  if (!count) return;
  if (t_column < 0) {
    tio_goto_column(0);
    tio_right(count);
  } else {
    if (t_column + count > hx_columns - 1) count = hx_columns - t_column;
    if (!count) return;
    t_column += count;
    if (t_RIght)
      tio_command(t_RIght, count, count);
    else if (t_right)
      while (count--) { tio_command(t_right, 1); }
    else
      while (count--) putchar(' ');
  }
}
/* tio_right */

  void
tio_start_application(void)
{
  if (!tio_tite_f) tio_command(t_tc_init, 1);
  t_application_mode = 1;
}
/* tio_start_application */

  void
tio_end_application(void)
{
  if (!tio_tite_f) tio_command(t_tc_end, 1);
  t_application_mode = 0;
}
/* tio_end_application */

  void
tio_keypad(const int on)
  /* Set the keypad mode.
   * on=0: numeric keypad.
   * on=1: application keypad.  Select this mode if you want to use the
   *       keypad (i.e. arrow-keys).
   */
{
  if (!on == !t_keypad_transmit) return;
  if (on) {
    if (t_kp_transmit_on) tio_command(t_kp_transmit_on, 1);
    t_keypad_transmit = 1;
  } else {
    if (t_kp_transmit_off) tio_command(t_kp_transmit_off, 1);
    t_keypad_transmit = 0;
  }
}
/* tio_keypad */

/* The functions `tio_read()' and `tio_unread()' are an interface to the
 * standard input stream (stdin).  It is possibe to unread an arbitrary
 * number of characters (at most 4096).  `tio_read()' and `tio_unread()'
 * are called by `tio_getch_()'.
 */
  static long
tio_read(char *buf, long count)
{
  long i = count, j;

  if (count <= tio_unread_count) {
    while (i--) *buf++ = tio_unread_buffer[--tio_unread_count];
    return count;
  }
  while (tio_unread_count--) *buf++ = tio_unread_buffer[tio_unread_count], --i;
  tio_unread_count = 0;
  count -= i - (j = read(0, buf, i));
  return j < 0 ? -1 : count;
}
/* tio_read */

  static void
tio_unread(char *buf, long count)
{
  while (count--) tio_unread_buffer[tio_unread_count++] = buf[count];
}
/* tio_unread */

  int
tio_readmore(void)
{
  return tio_unread_count;
}
/* tio_readmore */

  int
tio_getmore(void)
{
  return tio_unget_count;
}
/* tio_getmore */

  static int
tio_getch_(void)
  /* Read a character or keypad key from the keyboard.  Keypad keys
   * (like arrow keys, function keys, ...) are recognized only if the
   * terminal is in keypad transmit mode.  (Use `tio_keypad()' to set or
   * reset the keypad transmit mode.)  This function is called by
   * `tio_get()' and `tio_getch()'.
   */
{
  struct termios ts;
  char cbuf[256];
  int i, j;

  tio_flush();

  /* Check if C-c has been hit.
   */
  if (*tio_interrupt) {
    *tio_interrupt = 0;
    return (int)HXKEY_BREAK;
  }

  /* See if there is something in the unget buffer.
   */
  if (tio_unget_count--) return tio_unget_buffer[tio_unget_count];
  tio_unget_count = 0;

  /* Try to read one character from the input stream.  If the terminal is not
   * in keypad transmit mode, the character is returned.
   */
  switch (tio_read(cbuf, 1)) {
  case -1:
    return (int)HXKEY_ERROR;
      /* The occurrence of an error in `tio_read()' is likely to be caused by
       * the signal `SIGWINCH', indicating that the size of the window has
       * changed.
       */
  case 0:
    return (int)HXKEY_NONE;
  default:
    if (!t_keypad_transmit) {
      if (*cbuf == '\n' || *cbuf == '\r')
        return (int)HXKEY_RETURN;
      else
        return *cbuf ? (int)(unsigned char)*cbuf : (int)HXKEY_NULL;
    }
  }

  /* Check if the character read is the first character of a known key
   * string.  It is *not* assumed that all key strings start with `\e'
   * (escape).
   */
  for (i = 0; t_keys[i].key; ++i)
    if (*t_keys[i].string ? *cbuf == **t_keys[i].string : 0) {
      if (strlen(*t_keys[i].string) == 1)
        return (int)t_keys[i].key;
          /* The read character is a known key string. */
      else
        break;
    }
  if (!t_keys[i].key) {
    if (*cbuf == '\n' || *cbuf == '\r')
      return (int)HXKEY_RETURN;
    else
      return *cbuf ? (int)(unsigned char)*cbuf : (int)HXKEY_NULL;
  }

  /* If the read character is a prefix of a known key string, characters
   * are read from the input stream until a key string is recoginzed,
   * the read string is no longer a prefix of a known key string or no
   * characters arrived for `t_timeout' tenths of a second.
   */
  ts = ts_start;
  ts.c_lflag &= ~(ECHO | ICANON | IEXTEN);
  ts.c_cc[VMIN] = 0;
  ts.c_cc[VTIME] = t_timeout;
  tcsetattr(0, TCSANOW, &ts);
  for (j = 0;;) {
    j += i = tio_read(cbuf + j + 1, 1);
    if (i < 0) return (int)HXKEY_ERROR;
    cbuf[j + 1] = 0;
    if (!i) {
      /* On timeout return the first character of the read string and put
       * the rest of the read string back to the input stream.
       */
      if (j) tio_unread(cbuf + 1, j);
      return *cbuf ? (int)(unsigned char)*cbuf : (int)HXKEY_NULL;
    } else {
      int prefix_f = 0;
      for (i = 0; t_keys[i].key; ++i)
        if (*t_keys[i].string ? !strcmp(cbuf, *t_keys[i].string) : 0)
          return (int)t_keys[i].key;
      for (i = 0; t_keys[i].key; ++i)
        if (*t_keys[i].string ?
            !strncmp(cbuf, *t_keys[i].string, j + 1) : 0) {
          prefix_f = 1;
          break;
        }
      if (!prefix_f) {
        tio_unread(cbuf + 1, j);
        return *cbuf ? (int)(unsigned char)*cbuf : (int)HXKEY_NULL;
      }
    }
  } /* for */
}
/* tio_getch_ */

  int
tio_getch(void)
  /* Read a character or keypad key from the keyboard.  `tio_getch()' waits
   * for input and returns the key pressed or `HXKEY_ERROR' (-1) on error.
   * This function calls `tio_getch_()' to read the character.
   */
{
  struct termios ts, ts_rec;
  int key;

  tcgetattr(0, &ts_rec);
  ts = ts_start;
  ts.c_lflag &= ~(ECHO | ICANON | IEXTEN);
  ts.c_cc[VMIN] = 1;
  ts.c_cc[VTIME] = 0;
  tcsetattr(0, TCSANOW, &ts);
  for (;;) {
    switch (tio_getwait(tio_readwait_timeout)) {
      case -1:
#ifdef TCFLSH
        ioctl(0, TCFLSH, TCIFLUSH);
#else
        tcflush(0, TCIFLUSH);
#endif
        return -1;
      case 0:
        if (window_changed) return -1; else continue;
      default:
        break;
    }
    break;
  }
  key = tio_getch_();
  tcsetattr(0, TCSANOW, &ts_rec);
  return key;
}
/* tio_getch */

  int
tio_get(void)
  /* Like `tio_getch()', but doesn't wait input.  `tio_get()' returns
   * `HXKEY_NONE' (0) if no input is available.
   */
{
  struct termios ts, ts_rec;
  int key;

  tcgetattr(0, &ts_rec);
  ts = ts_start;
  ts.c_lflag &= ~(ECHO | ICANON | IEXTEN);
  ts.c_cc[VMIN] = 0;
  ts.c_cc[VTIME] = 0;
  tcsetattr(0, TCSANOW, &ts);
  key = tio_getch_();
  tcsetattr(0, TCSANOW, &ts_rec);
  return key;
}
/* tio_get */

  int
tio_tget(int tmout)
  /* Like `tio_get()', but waits `timeout' tenths of a second for input.
   * `tio_tget()' returns `HXKEY_NONE' (0) if nothing has been read.
   */
{
  struct termios ts, ts_rec;
  int key;

  tcgetattr(0, &ts_rec);
  ts = ts_start;
  ts.c_lflag &= ~(ECHO | ICANON | IEXTEN);
  ts.c_cc[VMIN] = 1;
  ts.c_cc[VTIME] = 0;
  tcsetattr(0, TCSANOW, &ts);
  switch (tio_getwait(tmout * 100)) {
    case -1:
#ifdef TCFLSH
      ioctl(0, TCFLSH, TCIFLUSH);
#else
      tcflush(0, TCIFLUSH);
#endif
      return -1;
    case 0:
      if (window_changed) return (int)HXKEY_ERROR; else return (int)HXKEY_NONE;
    default:
      break;
  }
  key = tio_getch_();
  tcsetattr(0, TCSANOW, &ts_rec);
  return key;
}
/* tio_tget */

  int
tio_ungetch(int x)
  /* Put the character `x' back into the input stream.  At most
   * `TIO_MAX_UNGET' characters can be ungetch.  The return value is `x'
   * or -1 on error.
   */
{
  if (tio_unget_count >= TIO_MAX_UNGET) return -1;
  tio_unget_buffer[tio_unget_count++] = x;
  return x;
}
/* tio_ungetch */

  int
tio_ungets(int *x)
  /* Put the key string `x' back into the input stream using
   * `tio_ungetch()'.  The return value is 0 and -1 on error.
   */
{
  int i;

  for (i = 0; x[i]; ++i);
  while (i) if (tio_ungetch(x[--i]) < 0) return -1;
  return 0;
}
/* tio_ungets */

  int
tio_testkey(int key)
  /* Returns 1, if a termcap entry for the requested key exists, else 0.
   * The function return always 1 for the keys `HXKEY_BACKSPACE', `HXKEY_TAB',
   * `HXKEY_RETURN', `HXKEY_ESCAPE', `HXKEY_DELETE', `HXKEY_NONE' and `HXKEY_ERROR'.
   */
{
  int i;

  if (key < 0x100) return 1;
  for (i = 0; t_keys[i].key; ++i)
    if ((int)t_keys[i].key == key) return !!*t_keys[i].string;
  return 0;
}
/* tio_testkey */

  const char *
tio_keyname(int key)
  /* Returns the name of the key `key'.  If `key' is a printable character,
   * it is returned as a string.  If `key' is a special key, the name of
   * that key is returned.  If `key' is unknown and greater than 0xff "??"
   * is returned, else a `\x??' hexadecimal code.
   */
{
  int i;
  static char name[8];

  for (i = 0; t_keys[i].key; ++i)
    if ((int)t_keys[i].key == key) return t_keys[i].name;
  if (tio_isprint(key)) {
    name[0] = (char)key;
    name[1] = 0;
    return name;
  }
  if (key > 0xff) return "??";
  if (key == (int)HXKEY_ESCAPE) return HXKEY_ESCAPE_NAME;
  sprintf(name, "\\x%02x", (unsigned char)key);
  return name;
}
/* tio_keyname */

  const char *
tio_keyrep(int key)
{
  static char rep[8];
  int i;

  if (key < 0x100) {
    if (key <= 0x1f) {
      rep[0] = '^';
      rep[1] = (char)(key + '@');
      rep[2] = 0;
      return rep;
    }
    if (tio_isprint(key)) {
      rep[0] = (char)key;
      rep[1] = 0;
      return rep;
    }
  }
  for (i = 0; t_keys[i].key; ++i)
    if ((int)t_keys[i].key == key) return t_keys[i].srep;
  return "";
}
/* tio_keyrep */

  const char *
tio_vkeyrep(int key)
{
  int i;

  for (i = 0; t_keys[i].key; ++i)
    if ((int)t_keys[i].key == key) return t_keys[i].srep;
  return tio_keyrep(key);
}
/* tio_vkeyrep */

  char *
tio_keyscan(int *key, char *s, int mode)
  /* Check if `s' is a sting representation of a key.
   * the keycode is written to `*key' and a pointer to the first
   * character after the srep is returned.
   * mode & 1:  scan for escapes starting with a `^'.
   * mode & 2:  scan for escapes starting with a `~'.
   * mode == 0:  scan for all known escapes.
   */
{
  int i;

  if (!mode) mode = ~0;
  if ((mode & 1) && *s == '^' && s[1]) {
    if (s[1] == '?') *key = (int)HXKEY_DELETE;
    if (!(*key = s[1] & 0x1f)) *key = (int)HXKEY_NULL;
    return s + 2;
  }
  if ((mode & 2) && *s == '~')
    for (i = 0; t_keys[i].key; ++i) {
      if (!strncmp(s, t_keys[i].srep, strlen(t_keys[i].srep))) {
        *key = (int)t_keys[i].key;
        return s + strlen(t_keys[i].srep);
      }
    }
  *key = *s;
  return s + !!*s;
}
/* tio_keyscan */

  int
tio_echo(const int on)
  /* on=1:  characters are echoed as they are typed.
   * on=0:  characters are not echoed.
   * Thie echo-option has no effect on other `tio_*'-functions,
   * i.e. `tio_getch()' won't ever echo the character it reads.
   * RETURN VALUE:  `tio_echo()' returns the previous echo-state; that is
   *   1 if echo was set and 0 if echo was clear.
   * NOTE:  You may want to turn of the echo if your program is makeing
   *   several successive calls to `tio_getch()', so the characters typed
   *   between two calls to `tio_getch()' won't be echoed on the screen.
   */
{
  struct termios ts;
  int prev_echo = t_echo;

  tcgetattr(0, &ts);
  if (on)
    ts.c_lflag |= ECHO;
  else
    ts.c_lflag &= ~ECHO;
  tcsetattr(0, TCSANOW, &ts);
  t_echo = !!on;
  return prev_echo;
}
/* tio_echo */

  void
tio_home(void)
  /* Cursor home. */
{
  if (t_home)
    tio_command(t_home, 1);
  else {
    tio_goto_line(0);
    if (t_column > 0) tio_goto_column(0);
  }
  t_column = 0;
  t_line = 0;
}
/* tio_home */

  void
tio_last_line(void)
  /* Move the cursor to the last line, first column.  */
{
  if (t_last_line) {
    tio_command(t_last_line, 1);
    t_line = hx_lines - 1;
    t_column = 0;
  } else
    tio_move(-1, 0);
}
/* tio_last_line */

  void
tio_goto_line(const int p_line)
  /* Move cursor to line `line'.
   */
{
  char *cmd;

  const int line = p_line > hx_lines - 1? hx_lines - 1: p_line;
  if (t_goto_line)
    tio_command(t_goto_line, 1, line);
  else {
    if (t_column < 0) t_column = 0;
    cmd = tgoto(t_cm, t_column, line);
    tio_command(cmd, 1);
  }
  t_line = line;
}
/* tio_goto_line */

  void
tio_goto_column(int column)
  /* Move cursor to column `column'.
   */
{
  if (column > hx_columns - 1) column = column - 1;
  if (t_column >= 0)
    if (abs(t_column - column) <= column) {
      tio_rel_move(0, column - t_column);
      return;
    }
  if (t_goto_column)
    tio_command(t_goto_column, 1, column);
  else {
    tio_return();
    tio_right(column);
  }
  t_column = column;
}
/* tio_goto_column */

  void
tio_move(int lin, int col)
  /* Move the cursor to position `line'/`column'.
   */
{
  char *cmd;

  if (lin > hx_lines - 1 || lin < 0) lin = hx_lines - 1;
  if (col > hx_columns - 1 || col < 0) col = hx_columns - 1;
  cmd = tgoto(t_cm, col, lin);
  tio_command(cmd, 1);
  t_line = lin;
  t_column = col;
}
/* tio_move */

  void
tio_rel_move(int lin, int col)
  /* Move the cursor relative to the cursor position.
   */
{
  if (lin) { if (lin < 0) tio_up(-lin); else tio_down(lin); }
  if (col) { if (col < 0) tio_left(-col); else tio_right(col); }
}
/* tio_rel_move */

  static int
tio_set_scrolling_region(int first, int last)
{
  if (t_change_scroll) {
    if (last < 0) last = hx_lines - 1;
    tio_command(t_change_scroll, 1, first, last);
    return 0;
  }
  return -1;
}
/* tio_set_scrolling_region */

  static void
tio_reset_scrolling_region(void)
{
  if (t_change_scroll) tio_command(t_change_scroll, 1, 0, hx_lines - 1);
}
/* tio_reset_scrolling_region */

  int
tio_scroll_up(int count, int first, int last)
  /* Scroll up (forward) `count' lines.  returns -1 if the terminal
   * can't scroll.
   */
{
  int c;

  assert(count);
  if (last < 0 || last == hx_lines - 1) {
    last = hx_lines - 1;
    goto scroll_all;
  }
  if (tio_set_scrolling_region(first, last)) {
    if ((!t_delete_lines && !t_delete_line) ||
        (!t_insert_lines && !t_insert_line))
      return -1;
    tio_goto_line(first);
    if (!(count == 1 && t_delete_line) && t_delete_lines)
      tio_command(t_delete_lines, hx_lines, count);
    else {
      for (c = count; c--;) tio_command(t_delete_line, hx_lines);
    }
    tio_goto_line(last - count + 1);
    if (!(count == 1 && t_insert_line) && t_insert_lines)
      tio_command(t_insert_lines, hx_lines, count);
    else {
      for (c = count; c--;) tio_command(t_insert_line, hx_lines);
    }
  } else {
    if (t_scroll_fwd || t_scroll_fwd_n) {
        tio_goto_line(last);
      if (!(count == 1 && t_scroll_fwd) && t_scroll_fwd_n)
        tio_command(t_scroll_fwd_n, count, count);
      else
        for (c = count; c--;) tio_command(t_scroll_fwd, 1);
    } else {
scroll_all:
      if (t_delete_line || t_delete_lines) {
        tio_goto_line(first);
        if (!(count == 1 && t_delete_line) && t_delete_lines)
          tio_command(t_delete_lines, hx_lines, count);
        else
          for (c = count; c--;) tio_command(t_delete_line, hx_lines);
      } else {
        tio_goto_line(last);
        tio_putchar('\n');
      }
    }
    tio_reset_scrolling_region();
  }
  return 0;
}
/* tio_scroll_up */

  int
tio_scroll_down(int count, int first, int last)
  /* Scroll down (reverse) `count' lines.  returns -1 if the terminal can't
   * scroll backwards.
   */
{
  int c;

  assert(count);
  if (last < 0) last = hx_lines - 1;
  if (!t_insert_line && !t_insert_lines
      && !t_scroll_backwd &&!t_scroll_backwd_n) return -1;
  if (tio_set_scrolling_region(first, last)) {
    if ((!t_delete_lines && !t_delete_line) ||
        (!t_insert_lines && !t_insert_line))
      return -1;
    tio_goto_line(last);
    if (!(count == 1 && t_delete_line) && t_delete_lines)
      tio_command(t_delete_lines, hx_lines, count);
    else {
      for (c = count; c--;) tio_command(t_delete_line, hx_lines);
    }
    tio_goto_line(first);
    if (!(count == 1 && t_insert_line) && t_insert_lines)
      tio_command(t_insert_lines, hx_lines, count);
    else
      for (c = count; c--;) tio_command(t_insert_line, hx_lines);
  } else {
    if (!(count == 1 && t_scroll_backwd) && t_scroll_backwd_n)
      tio_command(t_scroll_backwd_n, count, count);
    else if (t_scroll_backwd)
      for (c = count; c--;) tio_command(t_scroll_backwd, 1);
    else {
      tio_goto_line(first);
      if (!(count == 1 && t_insert_line) && t_insert_lines)
        tio_command(t_insert_lines, hx_lines, count);
      else
        for (c = count; c--;) tio_command(t_insert_line, hx_lines);
    }
    tio_reset_scrolling_region();
  }
  return 0;
}
/* tio_scroll_down */

  int
tio_puts(const char *s)
  /* Like `fputs(s, stdout)'.
   */
{
  const char *c1;
  char *c2;
  char *out;
  int mask_f, x;

  if (t_line < 0) tio_goto_line(0);
  if (t_column < 0) tio_goto_column(0);
  c1 = s;
  c2 = out = (char *)malloc_fatal(strlen(s) + 1);
  for (; *c1; ++c1) {
    mask_f = 0;
    switch (*c1) {
    case '\n':
      if (++t_line >= hx_lines) t_line = hx_lines - 1;
      t_column = 0;
      break;
    case '\r':
      t_column = 0;
      break;
    case '\b':
      if (!--t_line) t_line = 0;
      break;
    case '\t':
      x = t_column;
      t_column += T_TABSTOP;
      if (x % T_TABSTOP) while (!(t_column % T_TABSTOP)) --t_column;
      if (t_column >= hx_columns) {
        t_column = hx_columns - 1;
        while (x++ < t_column) *c2++ = ' ';
        mask_f = 1;
      }
      break;
    default:
      if (++t_column >= hx_columns) {
        t_column = hx_columns - 1;
        mask_f = 1;
      }
      break;
    }
    if (!mask_f) *c2++ = *c1;
  } /* for */
  *c2 = 0;
  fputs(out, stdout);
  free((char *)out);
  return 0;
}
/* tio_puts */

  int
tio_putchar(int x)
  /* Like `putchar(x)'.
   */
{
  char out[2];

  out[0] = (char)x;
  out[1] = 0;
  tio_puts(out);
  return (unsigned char)*out;
}
/* tio_putchar */

  void
tio_bell(void)
{
  if (t_bell) tio_command(t_bell, 1); else putchar('\a');
}
/* tio_bell */

  void
tio_visible_bell(void)
{
  tio_command(t_visible_bell, 1);
}
/* tio_visible_bell */

  void
tio_underscore(void)
{
  tio_command(t_underscore_on, 1);
}
/* tio_underscore */

  void
tio_underscore_off(void)
{
  tio_command(t_underscore_off, 1);
  tio_set_colors(t_fg, t_bg);
}
/* tio_underscore_off */

  void
tio_bold(void)
{
  tio_command(t_bold_on, 1);
}
/* tio_bold */

  void
tio_blink(void)
{
  tio_command(t_blink_on, 1);
}
/* tio_blink */

  void
tio_half_bright(void)
{
  tio_command(t_half_bright_on, 1);
}
/* tio_half_bright */

  void
tio_reverse(void)
{
  tio_command(t_reverse_on, 1);
}
/* tio_reverse */

  void
tio_normal(void)
{
  tio_command(t_all_off, 1);
  tio_command(t_underscore_off, 1);
  tio_set_colors(t_fg, t_bg);
}
/* tio_normal */

  void
tio_reset_attributes(void)
{
  tio_command(t_all_off, 1);
  tio_clear_to_eol();
}
/* tio_reset_attributes */

  void
tio_set_cursor(int mode)
  /* Set the visibility of the cursor.
   * `mode == 0':  invisible.
   * `mode == 1':  normal.
   * `mode == 2':  standout.
   */
{
  switch (mode) {
  case 0:
    tio_command(t_invisible_cursor, 1);
    break;
  case 1:
    tio_command(t_normal_cursor, 1);
    break;
  case 2:
    tio_command(t_standout_cursor, 1);
    break;
  }
}
/* tio_set_cursor */

  int
tio_have_color(void)
  /* returns a non-zero value if color is available, 0 else.
   */
{
#if !HAVE_NCURSES
  return 0;
#else
  return !!t_setab && !!t_setaf;
#endif
}
/* tio_have_color */

  void
tio_set_colors(int fg, int bg)
{
  tio_set_fg(fg);
  tio_set_bg(bg);
}
/* tio_set_colors */

  void
tio_set_fg(int color)
  /* set the foreground color to `color'.
   */
{
#if HAVE_NCURSES
  tio_command(t_setaf, 1, color);
#endif
  t_fg = color;
}
/* tio_set_af */

  void
tio_set_bg(int color)
  /* set the background color to `color'.
   */
{
#if HAVE_NCURSES
  tio_command(t_setab, 1, color);
#endif
  t_bg = color;
}
/* tio_set_ab */

  int
tio_get_fg(void)
{
  return t_fg;
}
/* tio_get_af */

  int
tio_get_bg(void)
{
  return t_bg;
}
/* tio_get_bg */

  void
tio_clear(void)
{
  tio_command(t_clear_screen, 1);
  t_line = 0;
  t_column = 0;
}
/* tio_clear */

  void
tio_clear_to_eol(void)
{
  tio_command(t_clear_to_eol, 1);
}
/* clear_to_eol */

  void
tio_display(char *text, int indent_arg)
  /* Send the string `text' to the terminal.  The string may contain the
   * following `@*'-commands:
   *
   *  @@                   @
   *  @c                   clear screen.
   *  @C                   clear to end of line.
   *  @M[+-]rrr[+-]ccc     absolute move. 
   *                       Move the cursor to position row rrr, column ccc.
   *                       If a value is prefixed by a `-', it is interpreted
   *                       relative to the bottom-right corner of the screen.
   *  @m[+-]rrr[+-]ccc     relative move.
   *  @Iiii                set indent.
   *  @b                   bell.
   *  @v                   flash, visible bell.
   *  @U                   underline on.
   *  @u                   underline off.
   *  @Am                  set attribute m, where m is one of the
   *                       following:
   *                        b  bold.
   *                        l  blink.
   *                        h  half bright.
   *                        r  reverse.
   *                        u  underline (equivalent to `@U').
   *                        s  standout.
   *                        ~  normal.
   *  @r                   return.
   *  @n                   newline.
   *
   * `indent_arg' sets the indent.  If `indent_arg < 0', the indent stays
   * unchanged.
   */
{
  int i, j;
  int lin, col;
  int eol_f, back_f, absolute_f;
  char *s = (char *)malloc_fatal(strlen(text) + 1);
  static int indent = 0;
  int maxcol = hx_columns - 1;

  if (indent_arg >= 0) indent = indent_arg;
  for (i = 0; i < indent && t_column < maxcol; ++i) tio_putchar(' ');
    /* Note that `t_column' is incremented by calling `tio_putchar()'. */
  strcpy(s, text);
  for (eol_f = 0, i = 0;;) {
    for (j = 0; s[i] != '@' && s[i]; ++i, ++j);
    if (!s[i]) eol_f = 1;
    s[i] = 0;
    if (s[i - j]) tio_puts(s + (i - j));
    if (eol_f) break;
    absolute_f = 0;
    switch (s[i + 1]) {
      case '@': tio_putchar('@'); break;
      case 'a': tio_bell(); break;
      case 'b': tio_left(1); break;
      case 'c': tio_clear(); break;
      case 'C': tio_clear_to_eol(); break;
      case 'v': tio_visible_bell(); break;
      case 'U': tio_underscore(); break;
      case 'u': tio_underscore_off(); break;
      case 'r': tio_return(); break;
      case 'n':
        tio_putchar('\n');
        for (j = 0; j < indent && t_column < maxcol; ++j) tio_putchar(' ');
          /* Note that `t_column' is incremented by calling `tio_putchar()'. */
        break;
      case 'A':
        if (s[i + 1]) {
          switch (s[i + 2]) {
          case 'b': tio_bold(); break;
          case 'l': tio_blink(); break;
          case 'h': tio_half_bright(); break;
          case 'r': tio_reverse(); break;
          case 'u': tio_underscore(); break;
          case '~': tio_normal(); break;
          default: break;
          }
        }
        ++i;
        break;
      case '~': tio_normal(); break;
      case 'I':
        i += 2;
        j = indent;
        if (!sscanf(s + i, "%03d", &indent)) indent = j;
        if (indent != j) tio_rel_move(0, indent - j);
        ++i;
        break;
      case 'M':
        absolute_f = 1;
      case 'm':
        i += 2;
        back_f = 0;
        if (s[i] == '+')
          ++i;
        else if (s[i] == '-') {
          ++i;
          back_f = 1;
        }
        if (!sscanf(s + i, "%03d", &lin)) lin = 0;
        i += 3;
        if (back_f) lin = -lin;
        back_f = 0;
        if (s[i] == '+')
          ++i;
        else if (s[i] == '-') {
          ++i;
          back_f = 1;
        }
        if (!sscanf(s + i, "%03d", &col)) col = 0;
        i += 3;
        if (back_f) col = -col;
        if (absolute_f) tio_move(lin, col); else tio_rel_move(lin, col);
        i -= 2;
        break;
      default:
        break;
    }
    i += 2;
  }
  tio_normal();
  free((char *)s);
}
/* tio_display */

  void
tio_message(char **message, int indent)
  /* displays the array of strings `message' via `tio_display()' providing
   * a simple pager.  this function assumes, that every string in
   * `message' displays as a single line.
   */
{
  int i;

  for (i = 1; *message; ++message, ++i) {
    tio_display(*message, indent);
    indent = -1;
    if (i > hx_lines - 2) {
      /* tio_printf(" @Ar -- more -- @~@r"); */
      tio_raw_printf(" [ more ]\r");
      switch (tio_getch()) {
        case HXKEY_DOWN:
        case 'd':
        case 'd' & 0x1f:
          i -= hx_lines / 2;
          break;
        case ' ':
        case 'f' & 0x1f:
          i = 0;
          break;
        case HXKEY_ERROR:
          if (i > hx_lines - 2) i = hx_lines - 2;
          tio_goto_line(i);
          window_changed = 0;
          break;
        case HXKEY_RETURN:
        default:
          --i;
          break;
      }
      tio_clear_to_eol();
    }
  }
}
/* tio_message */

  int
tio_printf(const char * const fmt, ...)
{
  va_list ap;
  int rval;

  va_start(ap, fmt);
  rval = tio_vprintf(fmt, ap);
  va_end(ap);
  return rval;
}
/* tio_printf */

  int
tio_vprintf(const char * const fmt, va_list ap)
  /* Similar to `printf()'.  `tio_printf()' understands the same @-commands
   * as `tio_display()'.  Note that @-commands in strings inserted via %s
   * are executed.  Use `tio_raw_printf()' if you don't wan't @-commands to
   * be executed.
   */
{
  char *s;
  int rval;

  rval = vasprintf(&s, fmt, ap);
  if (rval == -1)
    return rval;
  tio_display(s, 0);
  free((char *)s);
  return rval;
}
/* tio_vprintf */

  int
tio_raw_printf(const char *fmt, ...)
{
  va_list ap;
  int rval;

  va_start(ap, fmt);
  rval = tio_raw_vprintf(fmt, ap);
  va_end(ap);
  return rval;
}
/* tio_raw_printf */

  int
tio_raw_vprintf(const char *fmt, va_list ap)
  /* Like `printf()'.  No @-commands.
   */
{
  char *s;
  int rval;

  rval = vasprintf(&s, fmt, ap);
  if (rval == -1)
    return rval;
  tio_puts(s);
  free((char *)s);
  return rval;
}
/* tio_raw_vprintf */

  int
tio_insert_character(char x)
  /* Insert the character `x' at the current position.
   */
{
  int insert_mode = 0;

  if (t_insert_char) {
    tio_command(t_insert_char, 1);
  } else if (t_insert_chars) {
    tio_command(t_insert_chars, 1, 1);
  } else if (t_insert_mode) {
    tio_command(t_insert_mode, 1);
    insert_mode = 1;
  } else
    return -1;

  tio_putchar(x);
  if (insert_mode) tio_command(t_end_insert_mode, 1);
  return 0;
}
/* tio_insert_character */

  int
tio_delete_character(void)
  /* Delete the character under the cursor.
   */
{
  if (t_delete_char)
    tio_command(t_delete_char, 1);
  else if (t_delete_chars)
    tio_command(t_delete_chars, 1, 1);
  else
    return -1;
  return 0;
}
/* tio_delete_character */

#ifdef SIGWINCH
  static sigtype_t
sigwinch_handler(int sig __unused)
  /* Signal handler for signal `SIGWINCH'.
   */
{
  /* signal(SIGWINCH, sigwinch_handler); */
  tio_init(0);
  if (t_line > hx_lines - 1) {
    t_line = -1;
    tio_goto_line(hx_lines - 1);
  }
  if (t_column > hx_columns) {
    t_column = -1;
    tio_goto_column(hx_columns - 1);
  }
  window_changed = 1;
}
/* sigwinch_handler */
#endif

  int
tio_raw_readwait(int tmout)
{
#ifdef FD_ZERO
  fd_set fdset;
  struct timeval tv_, *tv = &tv_;

  FD_ZERO(&fdset);
  FD_SET(0, &fdset);
  if (tmout < 0)
    tv = 0;
  else {
    tv->tv_sec = tmout / 1000;
    tv->tv_usec = tmout % 1000;
  }
  tio_flush();
  return select(1, &fdset, 0, 0, tv);
#else
#ifdef POLLIN
  struct pollfd fd;

  fd.fd = 0;
  fd.events = POLLIN;
  tio_flush();
  return poll(&fd, 1, tmout);
#else
  tio_flush();
  return 1;
    /* perhaps somebody can tell me a better way to handle this? */
#endif
#endif
}
/* tio_raw_readwait */

  int
tio_readwait(int tmout)
{
  if (tio_unread_count) return 1;
  return tio_raw_readwait(tmout);
}
/* tio_readwait */

  int
tio_getwait(int tmout)
{
  if (tio_unget_count) return 1;
  return tio_readwait(tmout);
}
/* tio_getwait */

  void
tio_reset(void)
{
  tcsetattr(0, TCSANOW, &ts_start);
}
/* tio_reset */

  void
tio_suspend(void)
{
  if ((t_keypad_transmit_recover = t_keypad_transmit))
    tio_keypad(0);
  if ((t_application_mode_recover = t_application_mode))
    tio_end_application();
  tcgetattr(0, &ts_recover);
  tcsetattr(0, TCSANOW, &ts_start);
}
/* tio_suspend */

  void
tio_restart(void)
{
  if (t_keypad_transmit_recover) tio_keypad(1);
  if (t_application_mode_recover) tio_start_application();
  tcsetattr(0, TCSANOW, &ts_recover);
  tio_init(0);
  tio_normal();
}
/* tio_restart */

/* end of tio.c */


/* VIM configuration: (do not delete this line)
 *
 * vim:bk:nodg:efm=%f\:%l\:%m:hid:icon:
 * vim:sw=2:sm:textwidth=79:ul=1024:wrap:
 */
