/* signal.c	8/19/1995
 * Signal handlers for HEXER
 *
 * NOTE:  the signal handler for SIGWINCH is defined in `tio.c'.
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
#include <unistd.h>
#include <signal.h>

#include "hexer.h"
#include "tio.h"

#if HAVE_SIGTYPE_INT
#define SIG_RVAL 0
typedef int sigtype_t;
#else
#define SIG_RVAL
typedef void sigtype_t;
#endif

volatile int caught_sigint;

#ifdef SIGINT
  static sigtype_t
sigint_handler()
{
  signal(SIGINT, sigint_handler);
  caught_sigint = 1;
  return SIG_RVAL;
}
/* sigint_handler */
#endif

#ifdef SIGTSTP
  static sigtype_t
sigtstp_handler()
{
  signal(SIGTSTP, sigtstp_handler);
  tio_suspend();
  tio_goto_line(hx_lines - 1);
  tio_return();
  tio_clear_to_eol();
  tio_printf("@AbStopped@~.  Type `fg' to restart @Abhexer@~.\n");
  tio_flush();
  kill(0, SIGSTOP);
  return SIG_RVAL;
}
/* sigtstp_handler */
#endif

#ifdef SIGCONT
  static sigtype_t
sigcont_handler()
{
  signal(SIGCONT, sigcont_handler);
  tio_restart();
  he_refresh_all(current_buffer->hedit);
  return SIG_RVAL;
}
/* sigcont_handler */
#endif

#ifdef SIGPIPE
  static sigtype_t
sigpipe_handler()
{
  signal(SIGPIPE, sigpipe_handler);
#if 0
  he_message(0, "broken pipe");
#endif
  return SIG_RVAL;
}
/* sigpipe_handler */
#endif

  void
setup_signal_handlers()
{
  caught_sigint = 0;
#ifdef SIGINT
  signal(SIGINT, sigint_handler);
#endif
#ifdef SIGTSTP
  /* if the shell can't do job control, we'll ignore the signal `SIGTSTP' */
  if ((sigtype_t (*)())signal(SIGTSTP, SIG_IGN) != (sigtype_t (*)())SIG_IGN)
    signal(SIGTSTP, sigtstp_handler);
#endif
#ifdef SIGCONT
  signal(SIGCONT, sigcont_handler);
#endif
#ifdef SIGPIPE
  signal(SIGPIPE, sigpipe_handler);
#endif
}
/* setup_signal_handlers */

/* end of signal.c */


/* VIM configuration: (do not delete this line)
 *
 * vim:bk:nodg:efm=%f\:%l\:%m:hid:icon:
 * vim:sw=2:sm:textwidth=79:ul=1024:wrap:
 */
