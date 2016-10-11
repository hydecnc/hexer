/* port.c	4/10/1995
 * Functions missing on some OSs.
 */

/* This file has been placed int the PUBLIC DOMAIN by the author.
 */

#include "config.h"

#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#ifndef FD_ZERO
#include <poll.h>
#endif

#if !HAVE_STRCMP
  int
strcmp(const char *a, const char *b)
{
  while (*a && *b && *a == *b) ++a, ++b;
  return *a > *b ? 1 : *a < *b ? -1 : 0;
}
/* strcmp */
#endif

#if !HAVE_STRCASECMP
  int
strcasecmp(const char *s1, const char *s2)
{
  while (*s1 && *s2 && tolower(*s1) == tolower(*s2)) ++s1, ++s2;
  return *s1 > *s2 ? 1 : *s1 < *s2 ? -1 : 0;
}
/* strcasecmp */
#endif

/* end of port.c */


/* VIM configuration: (do not delete this line)
 *
 * vim:bk:nodg:efm=%f\:%l\:%m:hid:icon:
 * vim:sw=2:sm:textwidth=79:ul=1024:wrap:
 */
