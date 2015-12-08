/* map.c	8/19/1995
 * Map some key-sequences to some key-sequences, version 0.3
 * Requieres: tio.o
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

#include <alloca.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "buffer.h"
#include "hexer.h"

#undef TIO_MAP
#define TIO_MAP 1
#include "tio.h"

#define MAP_MAXLEN 256
#define MAP_MAX 16

#define MAP_ESC ('\033')

int tio_m_remap = 0;
  /* if `tio_m_remap == 1', the sequence of keys another sequence is mapped
   * to may be mapped again, i.e. if `ab' maps to `bcd' and `bc' is mapped
   * to `cd', then `ab' maps to `cdd'.  the default is *not* to allow
   * remapping.
   */

struct map_s {
  int from[MAP_MAXLEN];
  int to[MAP_MAXLEN];
  struct map_s *next;
};

static struct map_s *map_first[MAP_MAX] = { 0 };
static int map_max = 0;
static int keys[MAP_MAXLEN];
static int keys_n = 0;

static int munget[MAP_MAXLEN];
static int munget_n = 0;


/* operations on strings of keys
 */

  static int
key_strcmp(const int *ks1, const int *ks2)
{
  while (*ks1 && *ks2 && *ks1 == *ks2) ++ks1, ++ks2;
  return *ks1 != *ks2;
}
/* key_strcmp */

  static int
key_strncmp(const int *ks1, const int *ks2, int n)
{
  while (*ks1 && *ks2 && *ks1 == *ks2 && n--) ++ks1, ++ks2;
  return n && *ks1 != *ks2;
}
/* key_strncmp */

  static char *
key_strrep(const int *ks)
{
  int n;
  const int *i;
  char *s, *t;
  int key;

  for (n = 0, i = ks; *i; ++i) {
    if (*i == MAP_ESC)
      key = *++i + HXKEY_BIAS;
    else
      key = *i;
    n += strlen(tio_keyrep(key));
  }
  s = t = (char *)malloc(n + 1);
  for (i = ks; *i; ++i) {
    if (*i == MAP_ESC)
      key = *++i + HXKEY_BIAS;
    else
      key = *i;
    strcpy(t, tio_keyrep(key)); t += strlen(t);
  }
  return s;
}
/* key_strrep */

#if 0
  static char *
key_strrep_simple(ks)
  const int *ks;
  /* like `key_strrep()', but special keys are translated to 0xff.
   */
{
  char *s, *t;

  s = t = (char *)malloc(key_strlen(ks) + 1);
  for (; *ks; ++ks) if (*ks < 0x100) *t++ = *ks; else *t++ = 0xff;
  *t = 0;
  return s;
}
/* key_strrep_simple */
#endif

#if 0
  static int
key_strncpy(ks1, ks2, n)
  int *ks1;
  const int *ks2;
  int n;
{
  while (*ks2 && n--) *ks1++ = *ks2++;
  return *ks1 = 0;
}
/* key_strncpy */
#endif

  static int
key_strcpy(int *ks1, const int *ks2)
{
  while (*ks2) *ks1++ = *ks2++;
  return *ks1 = 0;
}
/* key_strcpy */

  static int
string_to_keys(int *kkeys, char *string)
{
  while (*string) {
    if (*string == MAP_ESC) {
      ++string;
      if (*string == 1)
        *kkeys++ = (int)HXKEY_ESCAPE;
      else
        *kkeys++ = *string + (int)HXKEY_BIAS;
    } else
      *kkeys++ = *string;
    ++string;
  }
  *kkeys = 0;
  return 0;
}
/* string_to_keys */

  static int
scan_keys(int *kkeys, char *string, int mode)
{
  do {
    if (*string == MAP_ESC) {
      ++string;
      if (*string == 1)
        *kkeys++ = HXKEY_ESCAPE;
      else
        *kkeys++ = *string + HXKEY_BIAS;
      ++string;
    } else
      string = tio_keyscan(kkeys++, string, mode);
  } while (*string);
  *kkeys = 0;
  return 0;
}
/* scan_keys */

#if 0
  static void
mungetch(key)
  int key;
{
  if (tio_m_remap) tio_ungetch(key); else munget[munget_n++] = key;
}
/* mungetch */
#endif

  static void
mungets(int *kkeys)
{
  int i;

  for (i = 0; kkeys[i]; ++i);
  if (tio_m_remap)
    while (i) tio_ungetch(kkeys[--i]);
  else
    while (i) munget[munget_n++] = kkeys[--i];
}
/* mungets */

  int
tio_mgetch(int map, char *map_string)
{
  int key;
  struct map_s *i;
  char *s = map_string;
  int ungets[MAP_MAXLEN], ungets_n = 0;
  int k;
  int prefix;

  if (*tio_interrupt) {
    *tio_interrupt = 0;
    return (int)HXKEY_BREAK;
  }
  while (map_max < map) map_first[++map_max] = 0;
  if (map_string) {
    /* create the `map_string' (a string representation of the unmapped keys
     * typed so far)  */
    if (keys_n)
      for (k = 0; k < keys_n; ++k) {
        strcpy(s, tio_keyrep(keys[k]));
        s += strlen(s);
      }
    else
      *map_string = 0;
  }
  if (tio_getmore()) return tio_getch();
  if (munget_n) return munget[--munget_n];
  key = tio_getch();
  if (key == (int)HXKEY_ERROR) return (int)HXKEY_ERROR;
  if (key == (int)HXKEY_ESCAPE && !keys_n)
    if (!tio_readmore()) return (int)HXKEY_ESCAPE;
  if (key <= 0) return key;
  keys[keys_n++] = key;
  keys[keys_n] = 0;
  for (;;) {
    /* check if `keys' matches */
    for (i = map_first[map]; i; i = i->next)
      if (!key_strcmp(keys, i->from)) { /* match */
        if (ungets_n) {
          ungets[ungets_n] = 0;
          mungets(ungets);
          ungets[ungets_n = 0] = 0;
        }
        keys[keys_n = 0] = 0;
        mungets(i->to);
        if (map_string) *map_string = 0;
        return tio_mgetch(map, map_string);
      }
    /* check if `keys' is a prefix */
    for (i = map_first[map], prefix = 0; i; i = i->next)
      if (!key_strncmp(keys, i->from, keys_n)) { prefix = 1; break; }
    if (!prefix) {
      ungets[ungets_n++] = keys[0];
      if (!--keys_n) break;
      for (k = 0; k < keys_n; ++k) keys[k] = keys[k + 1];
      continue;
    } else
      break;
  }
  if (ungets_n) {
    ungets[ungets_n] = 0;
    mungets(ungets);
    ungets[ungets_n = 0] = 0;
  }
  if (s) {
    strcpy(s, tio_keyrep(key));
    s += strlen(s);
  }
  if (munget_n) return munget[--munget_n];
  return (int)HXKEY_NONE;
}
/* tio_mgetch */

  int
tio_map(int map, char *from, char *to, int special_f)
  /* define a key mapping from `from' to `to' in keymap `map'.  if `special_f'
   * is set, special characters may also be written in ascii (i.e. "~UP" for
   * cursor up).  `special_f' should be set when reading commands from a file.
   */
{
  struct map_s *i, *j;
  int *kfrom, *kto;

  kfrom = (int *)alloca((strlen(from) + 1) * sizeof(int));
  if (special_f)
    scan_keys(kfrom, from, 0);
  else
    string_to_keys(kfrom, from);
  kto = (int *)alloca((strlen(to) + 1) * sizeof(int));
  if (special_f)
    scan_keys(kto, to, 0);
  else
    string_to_keys(kto, to);
  while (map_max < map) map_first[++map_max] = 0;
  for (i = map_first[map], j = 0; i; j = i, i = i->next)
    if (!key_strcmp(kfrom, i->from)) {
      key_strcpy(i->to, kto);
      return 0;
    }
  i = (struct map_s *)malloc(sizeof(struct map_s));
  i->next = 0;
  key_strcpy(i->from, kfrom);
  key_strcpy(i->to, kto);
  if (j) j->next = i; else map_first[map] = i;
  return 0;
}
/* tio_map */

  int
tio_unmap(int map, char *from, int special_f)
  /* remove the mapping `from' from the keymap `map'.  the flag `special_f'
   * works the same as in `tio_map()'.
   */
{
  struct map_s *i, *j;
  int *kfrom;

  kfrom = (int *)alloca((strlen(from) + 1) * sizeof(int));
  if (special_f)
    scan_keys(kfrom, from, 0);
  else
    string_to_keys(kfrom, from);
  if (map > map_max) return -1;
  for (i = map_first[map], j = 0; i; j = i, i = i->next)
    if (!key_strcmp(i->from, kfrom)) {
      if (j) j->next = i->next; else map_first[map] = i->next;
      free((char *)i);
      return 0;
    }
  return -1;
}
/* tio_unmap */

  char **
tio_maplist(int map)
{
  unsigned n, k;
  struct map_s *i;
  char **from, **to, **list;
  size_t from_maxwidth;

  for (i = map_first[map], n = 0; i; i = i->next, ++n);
  if (!n) {
    *(list = (char **)malloc(sizeof(char *))) = 0;
    return list;
  }
  from = (char **)alloca(n * sizeof(char *));
  to = (char **)alloca(n * sizeof(char *));
  list = (char **)malloc((n + 1) * sizeof(char *));
  for (i = map_first[map], k = 0; i; i = i->next, ++k) {
    from[k] = key_strrep(i->from);
    to[k] = key_strrep(i->to);
  }
  from_maxwidth = strlen(from[0]);
  for (k = 1; k < n; ++k)
    if (strlen(from[k]) > from_maxwidth) from_maxwidth = strlen(from[k]);
  from_maxwidth += 2;
  for (k = 0; k < n; ++k) {
    list[k] = (char *)malloc(from_maxwidth + strlen(to[k]) + 2);
    strcpy(list[k], from[k]);
    memset(list[k] + strlen(list[k]), ' ', from_maxwidth - strlen(list[k]));
    strcpy(list[k] + from_maxwidth, to[k]);
    list[k][strlen(list[k]) + 1] = 0;
    list[k][strlen(list[k])] = '\n';
    free((char *)from[k]);
    free((char *)to[k]);
  }
  list[n] = 0;
  return list;
}
/* tio_maplist */

#if 0
  char *
tio_mapentry(map, from)
  int map;
  char *from;
{
  struct map_s *i, *j;
  int *kfrom;
  int length;
  char *rep, *sfrom, *sto;

  kfrom = (int *)alloca(strlen(from) * sizeof(int));
  string_to_keys(kfrom, from);
  if (map > map_max) return -1;
  for (i = map_first[map], j = 0; i; j = i, i = i->next)
    if (!key_strcmp(i->from, kfrom)) {
      length = strlen(sfrom = key_strrep(i->from))
               + strlen(sto = key_strrep(i->to)) + 3;
      rep = (char *)malloc(length);
      strcpy(rep, sfrom);
      strcat(rep, "  ");
      strcat(rep, sto);
      free(sfrom);
      free(sto);
      return rep;
    }
  return 0;
}
/* tio_mapentry */
#endif

/* end of map.c */


/* VIM configuration: (do not delete this line)
 *
 * vim:bk:nodg:efm=%f\:%l\:%m:hid:icon:
 * vim:sw=2:sm:textwidth=79:ul=1024:wrap:
 */
