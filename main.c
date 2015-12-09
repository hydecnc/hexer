/* main.c	8/19/1995
 * The `main()' function for `hexer'.
 */

/* Copyright (c) 1995,1996 Sascha Demetrio
 * Copyright (c) 2009, 2010, 2015 Peter Pentchev
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
#include <unistd.h>

#ifndef HEXER_LONG_OPTIONS
#define HEXER_LONG_OPTIONS 0
#endif

#if HEXER_LONG_OPTIONS
#include <getopt.h>
#endif

#ifndef HEXER_MAX_STARTUP_COMMANDS
#define HEXER_MAX_STARTUP_COMMANDS 256
#endif

#include "buffer.h"
#include "hexer.h"
#include "exh.h"
#include "readline.h"
#include "regex.h"
#include "set.h"
#include "signal.h"
#include "tio.h"

static const char *usage_message = "\
hexer - a binary file editor\n\
usage: hexer [options] [file [...]]\n\
  -R/--readonly\n\
  -v/--view\n\
      Edit files in read-only mode.\n\
  -r/--recover filename\n\
      Recover  the  file filename after a crash. (not implemented)\n\
  -c/--command command\n\
      Start the editing session by executing the editor command\n\
      `command'.  If command contains whitespace, the whitespace\n\
      should be quoted.\n\
  -t/--tite\n\
      Turn off the usage of the termcap/terminfo ti/te sequence.\n\
  -h/--help\n\
      Print out a short help message and exit.\n\
  +command\n\
      This is equivalent to the -c option.\n\
  Note: The long options are not available on all systems.\n";

#if HEXER_LONG_OPTIONS
static struct option longopts[] = {
  { "readonly", 0, 0, 'R' },
  { "view", 0, 0, 'v' },
  { "recover", 1, 0, 'r' },  /* recover from the given file. */
  { "command", 1, 0, 'c' },  /* the given comand is executed in the first
                              * buffer. */
  { "help", 0, 0, 'h' },     /* print a short help message to `stdout'. */
  { "tite", 0, 0, 't' },     /* tite - turn off the ti/te sequence. */
  { 0, 0, 0, 0 }
};
#endif /* HEXER_LONG_OPTIONS */

static const char *shortopts = "Rvr:c:dth";

static int hexer_readonly;

static char *startup_commands[HEXER_MAX_STARTUP_COMMANDS];
static int startup_commands_n = 0;

  static int
process_args(const int argc, char * const argv[])
{
  int c, i;
  int exit_f = 0;
  char *first_buffer = 0;
  int open_f = 0;
#if HEXER_LONG_OPTIONS
  int longopt_idx;
#endif

  /* set default options */
  hexer_readonly = 0;

  for (; !exit_f;) {
    if (optind >= argc) break;
    if (*argv[optind] == '+') {
      if (!argv[optind][1])
        startup_commands[startup_commands_n++] = argv[++optind];
      else
        startup_commands[startup_commands_n++] = argv[optind] + 1;
      ++optind;
      continue;
    }
#if HEXER_LONG_OPTIONS
    c = getopt_long(argc, argv, shortopts, longopts, &longopt_idx);
#else
    c = getopt(argc, argv, shortopts);
#endif
    if (c < 0) break;
    switch (c) {
      case 'v':
      case 'R': /* readonly */
        hexer_readonly = 1;
	break;
      case 'r': /* recover */
        printf("recover from file `%s'.\n", optarg);
	break;
      case 'c': /* command */
        startup_commands[startup_commands_n++] = optarg;
	break;
      case 'd': /* debug */
        setbuf(stdout, 0);
        break;
      case 't': /* tite - turn off the ti/te sequence */
        ++tio_tite_f;
        break;
      case 'h': /* help */
      default:
        puts(usage_message);
	exit_f = 1;
	break;
    }
  }

  if (!exit_f) {
    if (optind < argc) /* read the files */
      while (optind < argc) {
        if (!he_open_buffer(argv[optind], argv[optind])) {
          open_f = 1;
          if (!first_buffer) first_buffer = argv[optind];
          if (hexer_readonly) he_set_buffer_readonly(argv[optind]);
        }
        ++optind;
      }
    if (!open_f) /* create an empty buffer */
      he_open_buffer("*scratch*", 0);
    if (first_buffer) he_select_buffer(first_buffer);
    /* execute the startup commands (if any).  some of the startup commands
     * may open or close buffers or even quit the editor, so we have to
     * check if `current_buffer' is 0 after each command. */
    hexer_init();
    if (startup_commands_n && current_buffer)
      for (i = 0; i < startup_commands_n; ++i) {
	// I think this is a false positive - current_buffer is global! :)
	// cppcheck-suppress nullPointer
        exh_command(current_buffer->hedit, startup_commands[i]);
        if (!current_buffer) break;
      }
    if (current_buffer) he_status_message(0);
  }
  return exit_f;
}
/* process_args */

  static void
setup_screen(void)
{
  int fg, bg;

  if (tio_have_color()) {
    fg = s_get_option_integer("fg");
    bg = s_get_option_integer("bg");
    tio_set_colors(fg, bg);
    tio_flush();
  }
}
/* setup_screen */

  int
main(const int argc, char * const argv[])
{
  int exit_f;

  /* configure readline */
  rl_backspace_jump = 5;
  rl_cancel_on_bs = 1;
  completer = exh_completer;

  rx_interrupt = &caught_sigint;
  tio_interrupt = &caught_sigint;
  he_messages = 0;
  setup_signal_handlers();
  hexer_version();
  if (tio_init(*argv) < 0) exit(1);
  if ((exit_f = process_args(argc, argv)) ? 1 : !current_buffer) {
    tio_reset();
    exit(exit_f);
  }
  tio_start_application();
  tio_echo(0);
  setup_screen();
  hexer();
  tio_echo(1);
  tio_goto_line(hx_lines - 1);
  tio_return();
  tio_end_application();
  tio_reset();
  return 0;
}
/* main */

/* end of main.c */


/* VIM configuration: (do not delete this line)
 *
 * vim:bk:nodg:efm=%f\:%l\:%m:hid:icon:
 * vim:sw=2:sm:textwidth=79:ul=1024:wrap:
 */
