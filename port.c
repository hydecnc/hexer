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

#if !HAVE_USLEEP
  int
usleep(unsigned long usecs)
  /* NOTE: this implementation of `usleep()' is not completely compatible
   *   with the BSD 4.3 `usleep()'-function, since it can be interupted by
   *   an incoming signal.
   */
{
#ifdef FD_ZERO
  struct timeval tv;

  tv.tv_sec = usecs / 1000000;
  tv.tv_usec = usecs & 1000000;
  select(0, 0, 0, 0, &tv);
#else
  struct pollfd fd;

  poll(&fd, 0, usecs > 1999 ? usecs / 1000 : 1);
#endif
  return 0;
}
/* usleep */
#endif

/* end of port.c */


/* VIM configuration: (do not delete this line)
 *
 * vim:bk:nodg:efm=%f\:%l\:%m:hid:icon:
 * vim:sw=2:sm:textwidth=79:ul=1024:wrap:
 */
