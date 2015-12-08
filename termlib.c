/* termlib.c	8/19/1995
 * 
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/param.h>
#ifdef BSD
#include <termio.h>
#endif

static char *t_termcap_entry;
static char t_PC = 0;  /* padding character */
static int t_speed;  /* line speed, measured in bps */

#define TL_TERMCAP "/etc/termcap"
#define TL_ENTRY_MAX 1024

static const int speeds[] = { 0, 50, 75, 110, 134, 150, 200, 300, 600,
                              1200, 1800, 2400, 4800, 9600 };

  static void
t_tinit(void)
  /* initialize the global variables `t_PC' and `t_speed';
   * called by `tgetent()'.
   */
{
  static char *t_tgetwhatever( /* char * */ );
  char *pc = t_tgetwhatever("pc");
  struct termios ts;
  int cfspeed;

  if (pc) { /* set the padding character */
    if (*pc++ != '=')
      t_PC = 0;
    else {
      if (*pc == '\\') {
        switch (*++pc) {
	  case 'E':
	  case 'e': t_PC = '\e'; break;
	  case 'n': t_PC = '\n'; break;
	  case 'r': t_PC = '\r'; break;
	  case 't': t_PC = '\t'; break;
	  case 'b': t_PC = '\b'; break;
	  case 'f': t_PC = '\f'; break;
	  case '0': case '1': case '2': case '3': case '4':
	  case '5': case '6': case '7':  /* octal escapes */
	    for (t_PC = 0; *pc >= '0' && *pc <= '7'; ++pc)
	      t_PC <<= 3, t_PC = *pc - '0';
	    break;
	  default: t_PC = *pc;
        }
      } else if (*pc == '^')
        t_PC = *pc & 0x1f;
      else
        t_PC = *pc;
    }
  } else /* default padding character '\0' */
    t_PC = 0;
  
  /* get the line speed */
  tcgetattr(0, &ts);
  cfspeed = cfgetospeed(&ts);
  if (cfspeed <= 13)
    t_speed = speeds[cfspeed];
  else { /* these are beyond the POSIX spec. */
    switch (cfspeed) {
#ifdef B19200
      case B19200: t_speed = 19200; break;
#else
#ifdef EXTA
      case EXTA: t_speed = 19200; break;
#endif
#endif
#ifdef B38400 
      case B38400: t_speed = 38400; break;
#else
#ifdef EXTB
      case EXTB: t_speed = 38400; break;
#endif
#endif
#ifdef B57600
      case B57600: t_speed = 57600; break;
#endif
#ifdef B115200
      case B115200: t_speed = 115200; break;
#endif
#ifdef B230400
      case B230400: t_speed = 230400; break;
#endif
      default: t_speed = 0; /* hmm... */
    }
  }
}
/* t_tinit */

  static int
t_tgetent(char *tbuf, char *terminal, FILE *fp, int i)
{
  char *p, *q, c;
  char tnames[1024], *tn;
  char *termcap_file = TL_TERMCAP;
  int close_fp = 0;

  if (!fp) {
    if ((p = getenv("TERMCAP")))
      if (*p == '/') /* termcap file */
	termcap_file = p;
      else {
	/* check if `TERMCAP' holds the correct entry */
	while (*p && *p != ':') {
	  while (*p == '|') ++p;
	  if (!strncmp(p, terminal, strlen(terminal))) { /* match */
	    while (*p && *p != ':') ++p;
	    while (*p == ':') ++p;
	    strcpy(tbuf, p);
	    t_termcap_entry = tbuf;
	    t_tinit();
	    return 1;
	  } else
	    while (*p && *p != ':' && *p != '|') ++p;
	}
      }
    if (!(fp = fopen(termcap_file, "r"))) { *tbuf = 0; return -1; }
    close_fp = 1;
  }

  c = getc(fp);
  for (;;) {
    int found;
    /* read until the beginning of an entry is found */
    for (;;) {
      if (!c) { *tbuf = 0; return 0; }
      while (c == ' ' || c == '\t') c = getc(fp);
      if (c == '#') { /* skip comment */
	while (c && c != '\n') c = getc(fp);
	c = getc(fp);
      } else if (c == '\n') /* skip empty lines */
        c = getc(fp);
      else
	break;
    }
    if (!c) { *tbuf = 0; return 0; }
    /* read the names of the terminal */
    tn = tnames;
    while (c != ':') *tn++ = c, c = getc(fp);
    *tn = 0;
    /* check if this is the termcap entry we're looking for */
    tn = q = tnames;
    for (found = 0;;) {
      int last = 0;
      while (*q && *q != '|') ++q;
      if (!*q) last = 1; else *q = 0;
      if (!strcmp(tn, terminal)) { found = 1; break; }
      if (last) break;
      tn = ++q;
    }
    if (!found) {
      /* skip the whole entry */
      for (;;) {
	while (c && c != '\\' && c != '\n') c = getc(fp);
	if (!c) { /* end of file */
	  *tbuf = 0;
	  return -1;
	} else if (c == '\\')
	  getc(fp);
        else
	  break;
	c = getc(fp);
      }
      c = getc(fp);
      continue;
    }
    for (p = tbuf;;) {
      char id[3];
      id[0] = id[1] = id[2] = 0;
      /* eat up junk until the beginning of the next capability-field is
       * found */
      while (c == ':')
	if ((c = getc(fp)) == '\\')
	  if ((c = getc(fp)) == '\n') {
	    do { c = getc(fp); } while (c == ' ' || c == '\t');
	    while (c == ':') c = getc(fp);
	  } else {
	    abort(); /* FIXME */
	    *p++ = '\\', ++i;
	  }
	else if (c == '\n' || !c)
	  break;
      /* read the id of the cap. into `id' */
      id[0] = c; /* id[0] != ':' */
      id[1] = c = getc(fp);
      if (!strcmp(id, "tc")) { /* reference to a similar terminal */
	char terminal[1024], *t = terminal;
	if (getc(fp) == '=') {
	  c = getc(fp);
	  while (c && c != ':' && c != '\n') *t++ = c, c = getc(fp);
	  *t = 0;
	  rewind(fp);
	  t_tgetent(p, terminal, fp, i);
	} else  /* corrupt entry */
	  *p = 0;
	return 1;
      }
      /* read all up to the next separator ':' */
      *p++ = id[0], ++i;
      while (c && c != ':' && c != '\n' && i < TL_ENTRY_MAX) 
	*p++ = c, ++i, c = getc(fp);
      if (i == TL_ENTRY_MAX || c == '\n' || !c) break;
      *p++ = ':', ++i;
    }
    break;
  }
  *p = 0;
  if (close_fp) fclose(fp);
  return 1;
}
/* t_tgetent */

  int
tgetent(char *tbuf, char *terminal)
{
  int rval;

  if ((rval = t_tgetent(tbuf, terminal, 0, 0)) == 1)
    t_termcap_entry = tbuf;
  return rval;
}
/* tgetent */

  static char *
t_tgetwhatever(char *id)
{
  char *p;
  int id_len = strlen(id);

  for (p = t_termcap_entry;;) {
    while (*p == ':') ++p;
    if (!*p) break;
    if (!strncmp(p, id, id_len)) {
      p += id_len;
      if (*p == '@') break; /* entry deleted */
      return p;
    }
    while (*p && *p != ':') ++p;
  }
  return 0;
}
/* t_tgetwhatever */

  int
tgetflag(char *id)
{
  return !!t_tgetwhatever(id);
}
/* tgetflag */

  int
tgetnum(char *id)
{
  char *p = t_tgetwhatever(id);

  if (!p ? 1 : *p++ != '#') return -1;
  return atoi(p);
}
/* tgetnum */

  char *
tgetstr(char *id, char **abuf)
{
  char *p = t_tgetwhatever(id), *q = *abuf;

  if (!p) return 0;
  while (*++p ? *p != ':' : 0) {
    switch (*p) {
      case '\\':
	switch (*++p) {
	  case 'E':
	  case 'e': *(*abuf)++ = '\e'; break;
	  case 'n': *(*abuf)++ = '\n'; break;
	  case 'r': *(*abuf)++ = '\r'; break;
	  case 't': *(*abuf)++ = '\t'; break;
	  case 'b': *(*abuf)++ = '\b'; break;
	  case 'f': *(*abuf)++ = '\f'; break;
	  case '0': case '1': case '2': case '3': case '4':
	  case '5': case '6': case '7':  /* octal escapes */
	    for (**abuf = 0; *p >= '0' && *p <= '7'; ++p)
	      **abuf <<= 3, **abuf = *p - '0';
	    ++*abuf;
	    --p;
	    break;
	  default: *(*abuf)++ = *p;
	}
	break;
      case '^': *(*abuf)++ = *++p & 0x1f; break;
      default: *(*abuf)++ = *p;
    }
  }
  *(*abuf)++ = 0;
  return q;
}
/* tgetstr */

  static char *
vtencode(char *cmd, va_list ap)
  /* Create a terminal command string suitable for `tputs()'.  the
   * `%'-escapes are substituted as listed below:
   *   %%      produce the character %
   *   %d      output an integer as in `printf("%d", ...)'
   *   %2      output an integer as in `printf("%2d", ...)'
   *   %3      output an integer as in `printf("%3d", ...)'
   *   %.      output a character as in `printf("%c", ...)'
   *   %+x     (where x is a non-zero byte) output (value + x) as
   *           in `printf("%c", ...)'
   *   %>xy    (where x and y are bytes) if value > x then add y; no output
   *   %r      reverse the order of the first two parameters; no output
   *   %i      increment by one; no output
   *   %n      exclusive-or all parameters with 0140 - this is needed for
   *           the Datamedia 2500 terminal
   *   %B      convert value to BCD (Binary Coded Decimal) - that is:
   *           do a ((value - 2 * (value / 10)) + (value % 10)); no output
   *   %D      Reverse coding: do a (value - 2 * (value % 16)); no output
   * NOTE:  the escapes %>xy, %i, %n, %B and %D affect all parameters.
   */
{
  int prm[16], prmn;  /* array of paramters; `prmn' elements. */
  static char buf[256];
  char *cp = cmd; /* command pointer */
  char *bp = buf; /* buffer pointer */
  int escape;
  int i;

  /* read in all parameters */
  for (escape = prmn = 0; *cp;) {
    i = 0; /* increment */
    switch (*cp++) {
      case '%':
        if (!escape) { ++escape; continue; }
        break;
      case '+':
        if (escape) i += *cp++; else break;
      case 'd': case '2': case '3': case '.':
        if (escape) prm[prmn++] = va_arg(ap, int) + i;
        break;
    }
    escape = 0;
  }

  /* modify the parameters */
  for (cp = cmd, escape = 0; *cp;) {
    switch (*cp++) {
      case '%':
        if (!escape) { ++escape; continue; }
        break;
      case '>':
        if (escape) {
          char x, y;
          x = *cp++, y = *cp++;
          for (i = 0; i < prmn; ++i) if (prm[i] > x) prm[i] += y;
        }
        break;
      case 'r':
        if (escape) {
          if (prmn < 2) break;
          i = prm[0], prm[0] = prm[1], prm[1] = i;
        }
        break;
      case 'i':
        if (escape) for (i = 0; i < prmn; ++i) ++prm[i];
        break;
      case 'n':
        if (escape) for (i = 0; i < prmn; ++i) prm[i] ^= 0140;
        break;
      case 'B':
        if (escape) 
          for (i = 0; i < prmn; ++i)
            prm[i] = (prm[i] - 2 * (prm[1] / 10)) + (prm[i] % 16);
        break;
      case 'D':
        if (escape)
          for (i = 0; i < prmn; ++i) prm[i] = prm[i] - 2 * (prm[i] % 16);
        break;
    }
    escape = 0;
  }

  /* create the command string */
  for (cp = cmd, escape = i = 0; *cp;) {
    switch (*cp++) {
      case '%':
        if (!escape) { ++escape; continue; } else *bp++ = '%';
        break;
      case 'd':
        if (escape) {
          sprintf(bp, "%d", prm[i++]);
          bp += strlen(bp);
        } else
          *bp++ = 'd';
        break;
      case '2':
        if (escape) {
          sprintf(bp, "%2d", prm[i++]);
          bp += strlen(bp);
        } else
          *bp++ = '2';
        break;
      case '3':
        if (escape) {
          sprintf(bp, "%3d", prm[i++]);
          bp += strlen(bp);
        } else
          *bp++ = '3';
        break;
      case '+':
        if (escape)
          ++cp;
        else {
          *bp++ = '+';
          break;
        }
      case '.':
        if (escape) *bp++ = (char)prm[i++]; else *bp++ = '.';
        break;
      case '>':
        if (escape) cp += 2; else *bp++ = '>';
        break;
      case 'i': case 'r': case 'B': case 'D':
        if (escape) break;
      default:
        *bp++ = cp[-1];
    }
    escape = 0;
  }
  *bp = 0;
  return buf;
}
/* vtencode */

  static char *
tencode(char *cmd, ...)
{
  va_list ap;
  char *buf;

  va_start(ap, cmd);
  buf = vtencode(cmd, ap);
  va_end(ap);
  return buf;
}
/* tencode */

  char *
tgoto(char *cm, int column, int line)
{
  if (cm ? !*cm : 1) return "OOPS";
  return tencode(cm, line, column);
}
/* tgoto */

  int
tputs(char *s, int affcnt, int (*outc)(int))
{
  long padding = 0;  /* padding counter multiplied by 10 */

  if (isdigit(*s)) {  /* need padding */
    while (isdigit(*s)) padding *= 10, padding += *s++ - '0';
    padding *= 10;
    if (*s == '.') padding += s[1] - '0', s += 2;
    if (*s == '*') {
      affcnt += !affcnt;
      padding *= affcnt;
      ++s;
    }
  }
  while (*s) outc(*s++);
  if (padding && t_speed) {
    padding *= (long)t_speed * (long)affcnt / 10;
    while (padding--) outc(t_PC);
  }
  return 0;
}
/* tputs */

/* end of termlib.c */


/* VIM configuration: (do not delete this line)
 *
 * vim:bk:nodg:efm=%f\:%l\:%m:hid:icon:
 * vim:sw=2:sm:textwidth=79:ul=1024:wrap:
 */
