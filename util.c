/* util.c	9/11/1995
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"
#include "hexer.h"
#include "util.h"
#include "tio.h"

#undef isoct
#undef ishex
#define isoct(x) ((x) >= '0' && (x) < '8')
#define ishex(x) (isdigit(x) || (tolower(x) >= 'a' && tolower(x) <= 'f'))

  int
util_trunc(char *s)
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
util_pstrcmp(char **a, char **b)
{
  return strcmp(*a, *b);
}
/* util_pstrcmp */

  int
util_strsort(char **list)
  /* a simple bubblesort.
   */
{
  int n;

  for (n = 0; list[n]; ++n);
  qsort((void *)list, n, sizeof(char *), (int (*)(const void *, const void *))util_pstrcmp);
  return 0;
}
/* util_strsort */

#if !HAVE_STRERROR
#ifndef BSD
extern const char * const sys_errlist[];
#endif
extern const int sys_nerr;

/*
 * We are using an error buffer instead of just returning the result from
 * strerror(3) to avoid constness warnings.  Yes, this makes this function
 * not-thread-safe.  Well, strerror(3) isn't.
 */
static char errbuf[512];

  char *
strerror(int errnum)
{
  if (errnum >= sys_nerr)
    strcpy(errbuf, "Unknown error");
  else
    snprintf(errbuf, sizeof(errbuf), "%s", sys_errlist[errnum]);
  return (errbuf);
}
#endif /* HAVE_STRERROR */

/* end of util.c */


/* VIM configuration: (do not delete this line)
 *
 * vim:bk:nodg:efm=%f\:%l\:%m:hid:icon:
 * vim:sw=2:sm:textwidth=79:ul=1024:wrap:
 */
