/* set.c	8/19/1995
 * Database of runtime-cunfigurable options
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
#include <assert.h>
#include <ctype.h>

#if USE_STDARG
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "defs.h"
#include "set.h"

struct option_s {
  char option[256];
  union {
    long ival; /* integer value */
    char sval[1024]; /* string value */
    int bval; /* boolean value */
  } u;
  enum s_option_e type;
  struct option_s *next;
  set_fn action;
};

static const char *true_strings[] = { "true", "t", "yes", "y", "on", 0 };
static const char *false_string = "false";

static struct option_s *option_first = 0;

  long
s_get_option_integer(option)
  const char *option;
{
  struct option_s *i;

  for (i = option_first; i; i = i->next)
    if (!strcmp(option, i->option)) {
      assert(i->type == S_INTEGER);
      return i->u.ival;
    }
  return -1;
}
/* s_get_option_integer */

  char *
s_get_option_string(option)
  const char *option;
{
  struct option_s *i;

  for (i = option_first; i; i = i->next)
    if (!strcmp(option, i->option)) {
      assert(i->type == S_STRING);
      return i->u.sval;
    }
  return "";
}
/* s_get_option_string */

  int
s_get_option_bool(option)
  const char *option;
{
  struct option_s *i;

  for (i = option_first; i; i = i->next)
    if (!strcmp(option, i->option)) {
      assert(i->type == S_BOOL);
      return i->u.bval;
    }
  return 0;
}
/* s_get_option_bool */

  int
s_delete_option(option)
  const char *option;
{
  struct option_s *i, *j;

  for (i = option_first, j = 0; i; j = i, i = i->next)
    if (!strcmp(option, i->option)) {
      if (j) j->next = i->next; else option_first = i->next;
      free((char *)i);
      return 0;
    }
  return -1;
}
/* s_delete_option */

#if USE_STDARG
  static void
s_append(struct option_s *last,
         const char *option,
         const enum s_option_e type, ...)
#else
  static void
s_append(last, option, type, va_alist)
  struct option_s *last;
  const char *option;
  const enum s_option_e type; 
  va_dcl
#endif
{
  va_list ap;
  struct option_s *i;

#if USE_STDARG
  va_start(ap, type);
#else
  va_start(ap);
#endif
  i = (struct option_s *)malloc(sizeof(struct option_s));
  i->type = type;
  i->next = 0;
  strcpy(i->option, option);
  if (last) last->next = i; else option_first = i;
  i->action = 0;
  switch (type) {
    case S_STRING:
      strcpy(i->u.sval, va_arg(ap, char *));
      break;
    case S_INTEGER:
      i->u.ival = va_arg(ap, long);
      break;
    case S_BOOL:
      i->u.bval = !!va_arg(ap, int);
      break;
    default:
      abort();
  }
  va_end(ap);
}
/* s_append */

  void
s_set_option_string(option, value)
  const char *option;
  const char *value;
{
  struct option_s *i, *j;

  for (i = option_first, j = 0; i; j = i, i = i->next)
    if (!strcmp(option, i->option)) {
      strcpy(i->u.sval, value);
      i->action = 0;
      i->type = S_STRING;
      return;
    }
  s_append(j, option, S_STRING, value);
}
/* s_set_option_string */

  void
s_set_option_integer(option, value)
  const char *option;
  const long value;
{
  struct option_s *i, *j;

  for (i = option_first, j = 0; i; j = i, i = i->next)
    if (!strcmp(option, i->option)) {
      i->u.ival = value;
      i->type = S_INTEGER;
      i->action = 0;
      return;
    }
  s_append(j, option, S_INTEGER, value);
}
/* s_set_option_integer */

  void
s_set_option_bool(option, value)
  const char *option;
  const int value;
{
  struct option_s *i, *j;

  for (i = option_first, j = 0; i; j = i, i = i->next)
    if (!strcmp(option, i->option)) {
      i->u.bval = value;
      i->type = S_BOOL;
      i->action = 0;
      return;
    }
  s_append(j, option, S_BOOL, value);
}
/* s_set_option_bool */

  enum s_option_e
s_get_type(option)
  const char *option;
{
  struct option_s *i;

  for (i = option_first; i; i = i->next)
    if (!strcmp(option, i->option)) return i->type;
  return (enum s_option_e)0;
}
/* s_get_type */

  char *
s_get_option(option)
  const char *option;
{
  struct option_s *i;
  static char rval[1024];

  for (i = option_first; i; i = i->next)
    if (!strcmp(option, i->option)) break;
  if (!i) return "";
  switch (i->type) {
    case S_STRING:
      strcpy(rval, i->u.sval);
      break;
    case S_INTEGER:
      sprintf(rval, "%li", i->u.ival);
      break;
    case S_BOOL:
      if (i->u.bval)
	strcpy(rval, true_strings[0]);
      else
	strcpy(rval, false_string);
      break;
    default:
      abort();
  }
  return rval;
}
/* s_get_option */

  void
s_set_type(option, type)
  const char *option;
  const enum s_option_e type;
{
  switch (type) {
    case S_STRING:
      s_set_option_string(option, "");
      break;
    case S_INTEGER:
      s_set_option_integer(option, 0);
      break;
    case S_BOOL:
      s_set_option_bool(option, 0);
      break;
    default:
      abort();
  }
}
/* s_set_type */

  void
s_set_option(option, value_string, no_action)
  const char *option;
  const char *value_string;
  int no_action;
{
  struct option_s *i;
  int k;

  for (i = option_first; i; i = i->next)
    if (!strcmp(option, i->option)) break;
  assert(i);
  switch (i->type) {
    case S_STRING:
      strcpy(i->u.sval, value_string);
      if (i->action && !no_action) i->action(value_string);
      break;
    case S_INTEGER:
      i->u.ival = atol(value_string);
      if (i->action && !no_action) i->action(i->u.ival);
      break;
    case S_BOOL:
      i->u.bval = 0;
      for (k = 0; true_strings[k]; ++k)
	if (!strcasecmp(true_strings[k], value_string)) {
	  i->u.bval = 1;
	  break;
	}
      if (i->action && !no_action) i->action(i->u.bval);
      break;
    default:
      abort();
  }
}
/* s_set_option */

  void
s_set_action(option, action)
  const char *option;
  set_fn action;
{
  struct option_s *i;

  for (i = option_first; i; i = i->next)
    if (!strcmp(i->option, option)) {
      i->action = action;
      return;
    }
  abort();
}
/* s_set_action */

  char **
s_option_list(prefix, bool_only)
  const char *prefix;
  int bool_only;
{
  struct option_s *i;
  char **list;
  int n;

  for (i = option_first, n = 0; i; i = i->next)
    if (!strncmp(i->option, prefix, strlen(prefix))) ++n;
  list = (char **)malloc((n + 1) * sizeof(char *));
  for (i = option_first, n = 0; i; i = i->next)
    if (!strncmp(i->option, prefix, strlen(prefix)))
      if (!bool_only || i->type == S_BOOL) {
        list[n] = (char *)malloc(strlen(i->option) + 1);
        strcpy(list[n], i->option);
        ++n;
      }
  list[n] = 0;
  return list;
}
/* s_option_list */

  char **
s_option_value_list()
{
  struct option_s *i;
  char **list;
  int n, option_maxlen = 0;

  for (i = option_first, n = 0; i; i = i->next, ++n)
    if (strlen(i->option) > option_maxlen) option_maxlen = strlen(i->option);
  list = (char **)malloc((n + 1) * sizeof(char *));
  for (i = option_first, n = 0; i; i = i->next, ++n) {
    list[n] = (char *)malloc(option_maxlen + 4
                             + strlen(s_get_option(i->option)));
    memset(list[n], ' ', option_maxlen + 2);
    strcpy(list[n], i->option);
    list[n][strlen(list[n])] = ' ';
    strcpy(list[n] + option_maxlen + 2, s_get_option(i->option));
    strcpy(list[n] + strlen(list[n]), "\n");
  }
  list[n] = 0;
  return list;
}
/* s_option_value_list */
    
  void
s_clear_all()
{
  struct option_s *i, *j;

  if (option_first) {
    for (j = option_first, i = j->next; i; j = i, i = i->next) free((char *)j);
    free((char *)j);
    option_first = 0;
  }
}
/* s_clear_all */

/* end of set.c */


/* VIM configuration: (do not delete this line)
 *
 * vim:bk:nodg:efm=%f\:%l\:%m:hid:icon:
 * vim:sw=2:sm:textwidth=79:ul=1024:wrap:
 */
