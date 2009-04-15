/* util.c	9/11/1995
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

#include <ctype.h>

#include "tio.h"

#undef isoct
#undef ishex
#define isoct(x) ((x) >= '0' && (x) < '8')
#define ishex(x) (isdigit(x) || (tolower(x) >= 'a' && tolower(x) <= 'f'))

  int
util_oct(s, max_digits)
  /* read an octal number from `*s'.  at most `max_digits' are read.  the
   * return values is the octal number on success and -1 on error.
   */
  char **s;
  int max_digits;
{
  int oct = 0;

  if (!isoct(**s)) return -1;
  max_digits -= !max_digits;
  while (isoct(**s) && max_digits--) oct <<= 3; oct += *(*s)++ - '0';
  return oct;
}
/* util_oct */

  int
util_dec(s, max_digits)
  /* read a decimal number from `*s'.  at most `max_digits' are read.  the
   * return values is the decimal number on success and -1 on error.
   */
  char **s;
  int max_digits;
{
  int dec = 0;

  if (!isdigit(**s)) return -1;
  max_digits -= !max_digits;
  while (isdigit(**s) && max_digits--) dec *= 10, dec += *(*s)++ - '0';
  return dec;
}
/* util_dec */

  int
util_hex(s, max_digits)
  /* read an hex number from `*s'.  at most `max_digits' are read.  the
   * return values is the hex number on success and -1 on error.
   */
  char **s;
  int max_digits;
{
  int hex = 0;

  if (!ishex(**s)) return -1;
  max_digits -= !max_digits;
  while (ishex(**s) && max_digits--) {
    hex <<= 4;
    hex += isdigit(**s) ? *(*s)++ - '0' : tolower(*(*s)++) - 'a' + 0xa;
  }
  return hex;
}
/* util_hex */

  int
util_escape(s, d)
  /* read an escape sequence from `*s' (without the escape character).
   * if `d' is non-zero, the result is written to `*d'.  the return value
   * is the code read from `*s' and -1 if the end of the string `*s' is
   * reached.  the values of `*s' and `*d' are incremented apropriately.
   */
  char **s;
  char **d;
{
  int code = -1;

  if (**s) {
    switch (*(*s)++) {
    case '\\': code = '\\'; break;
    case 'a': code = '\a'; break;
    case 'b': code = '\b'; break;
    case 'd': code = util_dec(s, 3) & 0xff; break;
    case 'e':
    case 'E': code = '\e'; break;
    case 'f': code = '\f'; break;
    case 'n': code = '\n'; break;
    case 'o': code = util_oct(s, 3) & 0xff; break;
    case 'r': code = '\r'; break;
    case 'v': code = '\v'; break;
    case 'x': code = util_hex(s, 2); break;
    default:
      if (isoct((*s)[-1])) {
	--*s;
	code = util_oct(s, 3) & 0xff;
      } else
	code = (unsigned char)(*s)[-1];
      break;
    }
    if (d)
      if (code)
        *(*d)++ = (char)code;
      else {
        *(*d)++ = '\e';
        *(*d)++ = (char)(KEY_NULL - KEY_BIAS);
      }
  }
  return code;
}
/* util_escape */

  int
util_translate(s, d)
  /* translate one character of the string `*s' and write it to `*d'.
   * C-style escape sequences in `*s' are translated to ASCII-codes in `*d'.
   * if `d' is a null-pointer, the character is not written to `*d'.
   * the return value is the ASCII-code read and -1 if the end of the
   * string `*s' is reached.  the values of `*s' and `*d' are incremented
   * apropriately.
   */
  char **s;
  char **d;
{
  if (!**s) return -1;
  if (**s == '\\') {
    ++*s;
    return util_escape(s, d);
  } else
    if (d)
      return (unsigned char)(*(*d)++ = *(*s)++);
    else
      return *(*s)++;
}
/* util_translate */

  int
util_trunc(s)
  char *s;
  /* remove trailing whitespace.
   */
{
  int i = strlen(s) - !!*s;

  while (i ? s[i] == ' ' || s[i] == '\t' : 0) --i;
  s[++i] = 0;
  return 0;
}
/* util_trunc */

  static int
util_pstrcmp(a, b)
  char **a, **b;
{
  extern strcmp();

  return strcmp(*a, *b);
}
/* util_pstrcmp */

  int
util_strsort(list)
  char **list;
  /* a simple bubblesort.
   */
{
  int n;

  for (n = 0; list[n]; ++n);
  qsort((void *)list, n, sizeof(char *), (int (*)())util_pstrcmp);
  return 0;
}
/* util_strsort */

/* end of util.c */


/* VIM configuration: (do not delete this line)
 *
 * vim:aw:bk:ch=2:nodg:efm=%f\:%l\:%m:et:hid:icon:
 * vim:sw=2:sc:sm:si:textwidth=79:to:ul=1024:wh=12:wrap:wb:
 */
