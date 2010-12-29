/* commands.c	10/05/1995
 */

/* Copyright (c) 1995,1996 Sascha Demetrio
 * Copyright (c) 2009, 2010 Peter Pentchev
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

#include <assert.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <stdlib.h>

#include "buffer.h"
#include "hexer.h"
#include "commands.h"
#include "edit.h"
#include "exh.h"
#include "helptext.h"
#include "regex.h"
#include "set.h"
#include "tio.h"
#include "util.h"

#define EXIT_EXEC_FAILED 27

#ifndef ERESTARTSYS
#define ERESTARTSYS EINTR
#endif

int he_map_special;

  static void
press_any_key(struct he_s *hedit)
{
  tio_printf("@Ar press any key @~@r");
  tio_getch();
  tio_clear_to_eol();
  he_refresh_all(hedit);
}
/* press_any_key */


/* commands for the exh-interpreter.  the commands may be separated by
 * semicolons.  if a command finds a semicolon at the end of its input,
 * a pointer to the character immediately following the semicolon is
 * returned.  if a separating semicolon is not found in the input, a
 * null-pointer is returned.
 */

  static char *
exh_skipcmd(char *cmd, char *dest)
  /* read `cmd' up to the next semicolon.  `\;' escapes the semicolon.
   * The return value is the character following the first occurence of an
   * unmasked semicolon or 0 if no semicolon was found or the first unmasked
   * semicolon was the last character in the string `cmd'.  the string read is
   * written to `dest'.
   */
{
  char *p, *q;

  if (!dest) dest = (char *)alloca(strlen(cmd) + 1);
  for (p = cmd, q = dest; *p && *p != ';';)
    if (*p == '\\' && p[1] == ';')
      *q++ = *++p, ++p;
    else
      *q++ = *p++;
  if (!*p) p = 0;
  *q = 0;
  return p;
}
/* exh_skipcmd */

  static char *
exh_map(struct he_s *hedit, char *args, int map)
{
  char *from, *to;
  char *p, *skip;

  skip = exh_skipcmd(args, p = (char *)alloca(strlen(args) + 1));
  args = p;
  if (!*args) {
    char **list, **i;
    i = list = tio_maplist(map);
    if (*list) {
      tio_message(list, 1);
      press_any_key(hedit);
    } else
      he_message(0, "no mappings");
    while (*i) free((char *)*i++);
    free((char *)list);
    return skip;
  }
  for (p = from = args; *p && *p != ' '; ++p);
  if (*p) {
    *p++ = 0;
    while (*p == ' ') ++p;
    for (to = p; *p; ++p);
    *p = 0;
    tio_map(map, from, to, he_map_special);
  }
  return skip;
}
/* exh_map */

  static char *
exh_unmap(struct he_s *hedit, char *args, int map)
  /* ARGSUSED */
{
  char *from;
  char *p, *skip;

  skip = exh_skipcmd(args, p = (char *)alloca(strlen(args) + 1));
  args = p;
  if (!*args) {
    he_message(0, "unmap what?");
    return 0;
  }
  for (p = from = args; *p && *p != ' '; ++p);
  *p = 0;
  if (tio_unmap(map, from, he_map_special)) he_message(0, "no such mapping");
  return skip;
}
/* exh_unmap */

  static char *
exh_save_buffer(struct he_s *hedit, char *name, long begin, long end, int *error)
{
  struct buffer_s *i;
  char *p;
  char *skip;
  int force_f = 0;
  FILE *fp;
  long l;
  int whole_f = 0;
  char *file = 0;

  if (end >= hedit->buffer->size || end < 0) end = hedit->buffer->size - 1;
  if (begin == 0 && end == hedit->buffer->size - 1) whole_f = 1;
  while (*name == ' ' || *name == '\t') ++name;
  if (*name == '!') {
    force_f = 1;
    while (*name == ' ' || *name == '\t') ++name;
  }
  skip = exh_skipcmd(name, p = (char *)alloca(strlen(name) + 1));
  name = p;
  util_trunc(name);
  if (!*name) {
    i = current_buffer;
    file = i->path;
  } else {
    for (i = buffer_list; i; i = i->next)
      if (!strcmp(name, i->hedit->buffer_name)) {
        file = i->path;
        break;
      }
    if (!i) {
      he_message(0, "@Abno buffer named@~ `%s'", name);
      if (error) *error = 1;
      return skip;
    }
  }
  if (i->hedit->read_only && !force_f) {
    he_message(0, "@Abread only buffer@~ - use @U:w!@~ to override");
    if (error) *error = 1;
    return skip;
  }
  if (!(fp = fopen(file, "w"))) {
    he_message(0, "@Abcan't open@~ `%s'", file);
    if (error) *error = 1;
    return skip;
  }
  fclose(fp);
  l = b_copy_to_file(i->hedit->buffer, file, begin, end - begin + 1);
  he_message(0, "wrote `%s' - 0x%lx (%li) bytes", file, l, l);
  if (whole_f) i->hedit->buffer->modified = 0;
  if (error) *error = 0;
  return skip;
}
/* exh_save_buffer */

  static char *
exh_write(struct he_s *hedit, char *file, long begin, long end, int *error)
{
  char *p;
  char *skip;
  int force_f = 0;
  FILE *fp;
  long l;
  int whole_f = 0;

  if (end >= hedit->buffer->size || end < 0) end = hedit->buffer->size - 1;
  if (begin == 0 && end == hedit->buffer->size - 1) whole_f = 1;
  while (*file == ' ' || *file == '\t') ++file;
  if (*file == '!') {
    force_f = 1, ++file;
    while (*file == ' ' || *file == '\t') ++file;
  }
  skip = exh_skipcmd(file, p = (char *)alloca(strlen(file) + 1));
  util_trunc(file = p);
  if (!*file) {
    if (!(file = current_buffer->path)) {
      he_message(1, "@Abno filename@~");
      return skip;
    }
    if (current_buffer->hedit->read_only && !force_f) {
      he_message(0, "@Abread only buffer@~ - use @U:w!@~ to override");
      if (error) *error = 1;
      return skip;
    }
  }
  if (!(fp = fopen(file, "w"))) {
    he_message(0, "@Abcan't open@~ `%s'", file);
    if (error) *error = 1;
    return skip;
  }
  fclose(fp);
  l = b_copy_to_file(hedit->buffer, file, begin, end - begin + 1);
  he_message(0, "wrote `%s' - 0x%lx (%li) bytes", file, l, l);
  if (whole_f) hedit->buffer->modified = 0;
  if (error) *error = 0;
  return skip;
}
/* exh_write */


  static char *
exhcmd_substitute(struct he_s *hedit, char *pattern, long begin, long end)
{
  char *p = pattern;
  char separator = *pattern;
  char *exp, *replace = 0, *options = 0;
  char *skip = 0;
  int confirm_f = 0;
  long position, last_match = hedit->position;
  char *replace_str;
  long replace_len, match_len;
  int subst = 0;
  char *data;
  static char prev_exp[4096] = "";
  static char prev_replace[4096] = "";
  static char prev_options[256] = "";

  if (!*pattern) {
    if (!*prev_exp) {
      he_message(0, "@Abno previous substitute@~");
      goto exit_exh_substitute;
    } else {
      exp = prev_exp;
      replace = prev_replace;
      options = prev_options;
    }
  } else {
    exp = p + 1;
    if (*exp) {
      p = exh_skip_expression(exp, separator);
      if (p) {
        *p = 0;
        replace = p + 1;
        p = exh_skip_replace(replace, separator);
        if (p) {
          *p = 0;
          options = p + 1;
          while (*p && *p != ';') ++p;
          if (*p) skip = p + 1; else skip = 0;
        } else
          options = "";
      } else {
        replace = "";
        options = "";
      }
    } else {
      if (!*prev_exp) {
        he_message(0, "@Abno previous substitute@~");
        goto exit_exh_substitute;
      } else {
        exp = prev_exp;
        replace = prev_replace;
        options = prev_options;
      }
    }
  }
  if (exp != prev_exp) strcpy(prev_exp, exp);
  if (replace != prev_replace) strcpy(prev_replace, replace);
  if (options != prev_options) strcpy(prev_options, options);
  p = options;
  while (*p) {
    switch (*p++) {
      case 'c':  /* confirm all changes */
        confirm_f = 1;
	break;
      case 'g':  /* ignored */
        break;
      default:
	he_message(0, "@Abinvalid option@~ `%c'", p[-1]);
	goto exit_exh_substitute;
    }
  }
  for (position = begin; position <= end;) {
    hedit->position = position;
    switch (position = he_search(hedit, exp, replace, 1, 0, 0, end,
                                 &replace_str, &replace_len, &match_len)) {
      case -2:  /* invalid expression */
        assert(rx_error);
	he_message(0, "@Abinvalid expression:@~ %s", rx_error_msg[rx_error]);
	goto exit_exh_substitute;
      case -1:  /* match not found */
        if (rx_error)
          he_message(0, "@Abregex: %s@~", rx_error_msg[rx_error]);
        if (subst > 0)
	  he_message(0, "%i substitutions", subst);
	else
	  he_message(0, "no match");
	goto exit_exh_substitute;
      default:  /* found */
        /* first we're gonna check if the match is in the selected area */
        if (position + match_len > end + 1) {
          position += 1;
          break;
        }
        last_match = position;
	if (!confirm_f)
	  goto substitute;
	else {
	  /* We want to show the user the match found so we can query him/her
	   * if the match should be replaced.  We'll do this by selecting the
           * match.
           * NOTE: `match_len' and/or `replace_len' might be zero.  If both
           *   are zero, we won't query the user.
	   */
	  int choice;
          if (!match_len && !replace_len) goto substitute;
          he_select(hedit, position, position + match_len - !!match_len);
          /* we want to make sure that (if possible) the whole match is
           * visible. */
          hedit->position = position + match_len;
          he_update_screen(hedit);
          hedit->position = position;
          he_update_screen(hedit);
          choice =
            he_query_yn(1,
                  "replace 0x%lx (%li) bytes at position 0x%lx (%li)  (ynaq) ",
                        match_len, match_len, position, position);
	  tio_flush();
	  switch (choice) {
	    case -1:  /* escape pressed */
	      if (subst > 0)
		he_message(0, "%i substitutions / canceled", subst);
	      else
		he_message(0, "canceled");
	      free((char *)replace_str);
	      he_cancel_selection(hedit);
	      he_update_screen(hedit);
	      goto exit_exh_substitute;
	    case 0:
	      free((char *)replace_str);
	      position += 1;
	      he_cancel_selection(hedit);
	      he_update_screen(hedit);
	      break;
	    case 1:  /* substitute */
	      goto substitute;
	    case 2:  /* always */
	      confirm_f = 0;
	      he_cancel_selection(hedit);
	      he_update_screen(hedit);
	      goto substitute;
	  }
	}
	break;
substitute:
        end -= match_len - replace_len;
	/* create an entry for the undo-list */
        if (match_len) {
          data = (char *)malloc(match_len);
          b_read(hedit->buffer, data, position, match_len);
          he_subcommand(hedit, 0, position, match_len, data);
        }
        if (replace_len)
          he_subcommand(hedit, 1, position, replace_len, replace_str);
	/* in any case the memory allocated for `replace_str' won't be
	 * `free((char *))'d before the final call to `he_subcommand()', so we
	 * can still use that pointer.  */
	if (match_len || replace_len) ++subst;
          /* if nothing gets replaced by nothing, we won't count that
           * as a substitution, so there won't be an entry in the undo-
           * list if nothing got changed. */
	if (match_len) b_delete(hedit->buffer, position, match_len);
	if (replace_len) {
          b_insert(hedit->buffer, position, replace_len);
          b_write(hedit->buffer, replace_str, position, replace_len);
        }
	if (confirm_f && match_len) {
          he_refresh_part(hedit, hedit->begin_selection, -1);
	  he_cancel_selection(hedit);
	  he_update_screen(hedit);
	}
	position += replace_len + !replace_len;
	break;
    }
    exp = 0;
  }

exit_exh_substitute:
  if (subst) he_subcommand(hedit, -1, 0, 0, 0);
  hedit->position = last_match;
  if (!confirm_f) he_refresh_part(hedit, 0, -1);
  return skip;
}
/* exhcmd_substitute */

  static char *
exhcmd_write(struct he_s *hedit, char *file, long begin, long end)
{
  return exh_write(hedit, file, begin, end, 0);
}
/* exhcmd_write */

  static char *
exhcmd_skip(struct he_s *hedit, char *args)
  /* ARGSUSED */
{
  struct buffer_s *i;
  char *ab = alternate_buffer;
  char *skip;

  skip = exh_skipcmd(args, 0);
  for (i = buffer_list; i; i = i->next)
    if (!i->visited_f) {
      alternate_buffer = current_buffer->hedit->buffer_name;
      if (he_select_buffer_(i)) { /* Oops. */
        he_message(1, "@AbINTERNAL ERROR@~");
        he_select_buffer_(current_buffer);
        alternate_buffer = ab;
      } else {
        he_status_message(1);
        he_refresh_part(current_buffer->hedit, 0, -1);
      }
      break;
    }
  if (!i) he_message(0, "no more unvisited buffers");
  return skip;
}
/* exhcmd_skip */

  static char *
exhcmd_next(struct he_s *hedit, char *args)
  /* ARGSUSED */
{
  struct buffer_s *i;
  char *ab = alternate_buffer;
  char *skip;

  skip = exh_skipcmd(args, 0);
  if ((i = current_buffer->next)) {
    alternate_buffer = current_buffer->hedit->buffer_name;
    if (he_select_buffer_(i)) { /* Oops. */
      he_message(1, "@AbINTERNAL ERROR@~");
      he_select_buffer_(current_buffer);
      alternate_buffer = ab;
    } else {
      he_status_message(1);
      he_refresh_part(current_buffer->hedit, 0, -1);
    }
  } else
    he_message(0, "no next buffer");
  return skip;
}
/* exhcmd_next */

  static char *
exhcmd_previous(struct he_s *hedit, char *args)
  /* ARGSUSED */
{
  struct buffer_s *i;
  char *ab = alternate_buffer;
  char *skip;

  skip = exh_skipcmd(args, 0);
  for (i = buffer_list; i; i = i->next) if (i->next == current_buffer) break;
  if (i) {
    alternate_buffer = current_buffer->hedit->buffer_name;
    if (he_select_buffer_(i)) { /* Oops. */
      he_message(1, "@AbINTERNAL ERROR@~");
      he_select_buffer_(current_buffer);
      alternate_buffer = ab;
    } else {
      he_status_message(1);
      he_refresh_part(current_buffer->hedit, 0, -1);
    }
  } else
    he_message(0, "no previous buffer");
  return skip;
}
/* exhcmd_previous */

  static char *
exhcmd_rewind(struct he_s *hedit, char *args)
  /* ARGSUSED */
{
  char *ab = alternate_buffer;
  char *skip;

  skip = exh_skipcmd(args, 0);
  alternate_buffer = current_buffer->hedit->buffer_name;
  if (he_select_buffer_(buffer_list)) { /* Oops. */
    he_message(1, "@AbINTERNAL ERROR@~");
    he_select_buffer_(current_buffer);
    alternate_buffer = ab;
  } else {
    he_status_message(1);
    he_refresh_part(current_buffer->hedit, 0, -1);
  }
  return skip;
}
/* exhcmd_rewind */

  static char *
exhcmd_wn(struct he_s *hedit, char *file, long begin, long end)
{
  char *skip;
  int error = 0;

  skip = exh_write(hedit, file, begin, end, &error);
  if (!error) exhcmd_next(hedit, "");
  return skip;
}
/* exhcmd_wn */

  static char *
exhcmd_read(struct he_s *hedit, char *file, long position)
{
  FILE *fp;
  long l;
  char *data;
  char *skip, *p;

  skip = exh_skipcmd(file, p = (char *)alloca(strlen(file) + 1));
  util_trunc(file = p);
  while (*file == ' ' || *file == '\t') ++file;
  if (!(fp = fopen(file, "r"))) {
    he_message(0, "@Abcan't open@~ `%s'", file);
    return skip;
  }
  fclose(fp);
  l = b_paste_from_file(hedit->buffer, file, position);
  if (l == -1)
    return skip;
  data = (char *)malloc(l);
  fp = fopen(file, "r");
  if (fread(data, 1, l, fp) != l) {
    free(data);
    fclose(fp);
    return skip;
  }
  he_subcommand(hedit, 1, position, l, data);
  he_subcommand(hedit, -1, 0, 0, 0);
  he_message(0, "read 0x%lx (%li) bytes from `%s'", l, l, file);
  if (l) he_refresh_part(hedit, position, -1);
  fclose(fp);
  return skip;
}
/* exhcmd_read */

  static char *
exhcmd_edit(struct he_s *hedit, char *name)
  /* ARGSUSED */
{
  char *ab = alternate_buffer;
  struct passwd *pe;
  int new_buffer_f = 0;
  FILE *fp;
  char *skip, *p;
  char *path = 0;
  char *user = 0;
  int uid = getuid();

  skip = exh_skipcmd(name, p = (char *)alloca(strlen(name) + 1));
  util_trunc(name = p);
  while (*name == ' ' || *name == '\t') ++name;
  if (!strcmp(name, "#")) { /* switch to alternate buffer. */
    if (he_alternate_buffer() < 0) 
      he_message(0, "no alternate buffer");
    else {
      he_refresh_part(current_buffer->hedit, 0, -1);
      tio_ungetch('g' & 0x1f);  /* FIXME */
    }
    return skip;
  } else
    alternate_buffer = current_buffer->hedit->buffer_name;
  if (he_select_buffer(name) < 0) {
    new_buffer_f = 1;
    if (*name == '~') {
      user = strdup(name + 1);
      for (p = user; *p && *p != '/'; ++p);
      *p = 0;
      if (*user) {
        for (setpwent(); (pe = getpwent());)
          if (!strcmp(user, pe->pw_name)) break;
      } else
        for (setpwent(); (pe = getpwent());)
          if (uid == pe->pw_uid) break;
      if (pe) {
        path = (char *)malloc(strlen(name) + strlen(pe->pw_dir) + 4);
        strcpy(path, pe->pw_dir);
        strcat(path, name + strlen(user) + 1);
      } else
        path = name;
      free(user);
    } else
      path = name;
    if (!(fp = fopen(path, "r"))) {
      he_message(0, "@Abcan't open@~ `%s'", name);
      alternate_buffer = ab;
      if (path && path != name) free(path);
      return skip;
    } else {
      fclose(fp);
      he_open_buffer(name, path);
      he_select_buffer(name);
    }
  }
  he_status_message(!new_buffer_f);
  he_refresh_part(current_buffer->hedit, 0, -1);
  if (path && path != name) free(path);
  return skip;
}
/* exhcmd_edit */

  static char *
exhcmd_buffer(struct he_s *hedit, char *name)
{
  char *ab = alternate_buffer;
  long j, k;
  struct buffer_s *i;
  char **list;
  char *c;
  char *skip, *p;

  skip = exh_skipcmd(name, p = (char *)alloca(strlen(name) + 1));
  util_trunc(name = p);
  if (!*name) { /* list buffers */
    int name_maxlen = 0;
    for (i = buffer_list, k = 0; i; i = i->next, ++k)
      if (strlen(i->hedit->buffer_name) > name_maxlen)
        name_maxlen = strlen(i->hedit->buffer_name);
    list = (char **)malloc((k + 1) * sizeof(char *));
    for (i = buffer_list, k = 0; i; i = i->next, ++k) {
      list[k] = (char *)malloc(strlen(i->hedit->buffer_name) + 512);
      sprintf(list[k], " `%s' ", i->hedit->buffer_name);
      c = list[k] + strlen(list[k]);
      for (j = name_maxlen - strlen(i->hedit->buffer_name); j >= 0; --j)
        *c++ = ' ';
      *c = 0;
      if (i->path) {
        if (strcmp(i->hedit->buffer_name, i->path)) {
          sprintf(c, "(%s) ", i->path);
          c += strlen(c);
        }
      } else {
        sprintf(c, "(no file) ");
        c += strlen(c);
      }
      if (i->loaded_f) {
        if (i->hedit->buffer->modified) {
          sprintf(c, "[modified] ");
          c += strlen(c);
        }
        if (i->hedit->read_only) {
          sprintf(c, "[readonly] ");
          c += strlen(c);
        }
        j = i->hedit->buffer->size;
        sprintf(c, " 0x%lx (%li) bytes\n", j, j);
      } else {
        if (!i->visited_f)
          sprintf(c, " (not visited)\n");
        else
          sprintf(c, " (not loaded)\n");
      }
    }
    list[k] = 0;
    tio_message(list, 0);
    for (k = 0; list[k]; ++k) free((char *)list[k]);
    free((char *)list);
    press_any_key(hedit);
    return skip;
  } else { /* select buffer */
    alternate_buffer = current_buffer->hedit->buffer_name;
    if (he_select_buffer(name) < 0) {
      he_message(0,
                 "@Abno buffer named@~ `%s' - use @U:edit@u to open a buffer",
                 name);
      alternate_buffer = ab;
      return skip;
    } else {
      he_status_message(1);
      he_refresh_part(current_buffer->hedit, 0, -1);
    }
  }
  return skip;
}
/* exhcmd_buffer */

  static char *
exhcmd_wall(struct he_s *hedit, char *args)
  /* ARGSUSED */
{
  struct buffer_s *i;
  char *skip;

  skip = exh_skipcmd(args, 0);
  for (i = buffer_list; i; i = i->next) {
    if (i->loaded_f ? i->hedit->buffer->modified : 0) {
      exh_save_buffer(hedit, i->hedit->buffer_name, 0, -1, 0);
#if 0
      l = b_write_buffer_to_file(i->hedit->buffer, i->path);
      if (l < 0)
	he_message(1, "@Abcan't write `%s'", i->path);
      else
        he_message(0, "wrote `%s' - 0x%lx (%li) bytes", i->path, l, l);
#endif
    }
  }
  return skip;
}
/* exhcmd_wall */

  static char *
exhcmd_quit(struct he_s *hedit, char *args)
  /* ARGSUSED */
{
  struct buffer_s *i, *j;
  char *skip, *p;
  int n;
  static int warn_more_files = 1;

  skip = exh_skipcmd(args, p = (char *)alloca(strlen(args) + 1));
  args = p;
  while (*args == ' ' || *args == '\t') ++args;
  if (*args == '!') goto close_all_buffers;
  for (i = buffer_list, n = 0; i; i = i->next) {
    if (i->loaded_f ? i->hedit->buffer->modified : 0) {
      he_message(0,
                 "@Abno write since last change@~ - use @U:q!@u to override");
      return skip;
    }
    n += !i->visited_f;
  }
  if (n && warn_more_files) {
    /* one warning per session should be enough.
     */
    warn_more_files = 0;
    he_message(0, "@Ab%i more files to edit@~", n);
    return skip;
  }

close_all_buffers:
  for (i = buffer_list, j = 0; i; j = i, i = i->next)
    if (j) he_close_buffer(j->hedit->buffer_name);
  if (j) he_close_buffer(j->hedit->buffer_name);
  return skip;
}
/* exhcmd_quit */

  static char *
exhcmd_close(struct he_s *hedit, char *args)
  /* ARGSUSED */
{
  int force_f = 0;
  int nclosed = 0;
  int i;
  char *skip, *p;

  skip = exh_skipcmd(args, p = (char *)alloca(strlen(args) + 1));
  args = p;
  while (*args == ' ' || *args == '\t') ++args;
  if (*args == '!') ++args, ++force_f;
  util_trunc(args);
  do {
    while (*args == ' ' || *args == '\t') ++args;
    if (!*args && !nclosed) {
      i = he_close_buffer(current_buffer->hedit->buffer_name);
      assert(i <= 0);
      ++nclosed;
      if (i < 0) break;
      p = args;
    } else {
      for (p = args; *p && *p != ' ' && *p != '\t'; ++p);
      if (*p) *p++ = 0;
      if ((i = he_buffer_modified(args)) < 0)
        he_message(0, "@Abno buffer named@~ `%s'", args);
      else if (i && !force_f)
        he_message(0,
                 "@Abno write since last change@~ - use @U:c!@u to override");
      else {
        i = he_close_buffer(args);
        assert(i <= 0);
        ++nclosed;
        if (i < 0) break;
      }
    }
    args = p;
  } while (*args);
  return skip;
}
/* exhcmd_close */

  static char *
exhcmd_map(struct he_s *hedit, char *args)
{
  return exh_map(hedit, args, MAP_COMMAND);
}
/* exhcmd_map */

  static char *
exhcmd_imap(struct he_s *hedit, char *args)
{
  return exh_map(hedit, args, MAP_INSERT);
}
/* exhcmd_imap */

  static char *
exhcmd_vmap(struct he_s *hedit, char *args)
{
  return exh_map(hedit, args, MAP_VISUAL);
}
/* exhcmd_vmap */

  static char *
exhcmd_unmap(struct he_s *hedit, char *args)
{
  return exh_unmap(hedit, args, MAP_COMMAND);
}
/* exhcmd_unmap */

  static char *
exhcmd_iunmap(struct he_s *hedit, char *args)
{
  return exh_unmap(hedit, args, MAP_INSERT);
}
/* exhcmd_iunmap */

  static char *
exhcmd_vunmap(struct he_s *hedit, char *args)
{
  return exh_unmap(hedit, args, MAP_VISUAL);
}
/* exhcmd_vunmap */

  static char *
exhcmd_set(struct he_s *hedit, char *args)
{
  char *p, *q, *option;
  enum s_option_e type;
  int no_f;
  char *skip;

  skip = exh_skipcmd(args, p = (char *)alloca(strlen(args) + 1));
  util_trunc(args = p);
  if (!*args) {
    char **list = s_option_value_list(), **i = list;
    if (*list) {
      tio_message(list, 1);
      press_any_key(hedit);
    } else
      he_message(0, "no options set");
    while (*i) free((char *)*i++);
    free((char *)list);
    return skip;
  }
  p = args;
  for (; *p;) {
    no_f = 0;
    option = q = p;
    while (*p && *p != ' ' && *p != '\t') ++p;
    if (*p) *p++ = 0;
    while (*p == ' ' || *p == '\t') ++p;
    if (!strncmp(option, "no", 2)) {
      option += 2;
      no_f = 1;
    } else {
      while (*q && *q != '=' && *q != ' ' && *q != '\t') ++q;
      if (*q == '=') {
        if (no_f) {
          he_message(0, "@Abinvalid argument:@~ %s", option);
          return skip;
        }
        *q = 0;
      } else
        q = 0;
    }
    if (!(type = s_get_type(option))) {
      he_message(0, "@Abinvalid option:@~ %s", option);
      return skip;
    }
    if (no_f && type != S_BOOL) {
      if (q) *q = '=';
      he_message(0, "@Abinvalid argument:@~ %s", option - 2);
      return skip;
    }
    if (q)
      s_set_option(option, q + 1, 0);
    else {
      if (type == S_BOOL)
        s_set_option(option, no_f ? "false" : "true", 0);
      else {
        char *value = s_get_option(option);
        if (!*value) value = "<unset>";
        he_message(0, "%s=%s@~", option, value);
      }
    }
  }
  return skip;
}
/* exhcmd_set */

  static char *
exhcmd_delete(struct he_s *hedit, char *args, long begin, long end)
  /* ARGSUSED */
{
  long count = end - begin;
  char *data;
  char *skip, *p;

  skip = exh_skipcmd(args, p = (char *)alloca(strlen(args) + 1));
  util_trunc(args = p);
  if (!count) return skip;
  data = (char *)malloc(count);
  b_read(hedit->buffer, data, begin, count);
  if (!kill_buffer)
    kill_buffer = new_buffer(0);
  else
    b_clear(kill_buffer);
  b_insert(kill_buffer, 0, count);
  b_write(kill_buffer, data, 0, count);
  he_subcommand(hedit, 0, begin, count, data);
  he_subcommand(hedit, -1, 0, 0, 0);
  b_delete(hedit->buffer, begin, count);
  if (hedit->position > begin) hedit->position -= count;
  he_refresh_part(hedit, begin, -1);

  if (count > 1)
    he_message(0, "0x%lx (%li) bytes deleted (cut).", count, count);
  return skip;
}
/* exhcmd_delete */

  static char *
exhcmd_yank(struct he_s *hedit, char *args, long begin, long end)
  /* ARGSUSED */
{
  long count = end - begin;
  char *skip, *p;

  skip = exh_skipcmd(args, p = (char *)alloca(strlen(args) + 1));
  util_trunc(args = p);
  if (kill_buffer) b_clear(kill_buffer); else kill_buffer = new_buffer(0);
  if (!count) return skip;
  b_insert(kill_buffer, 0, count);
  b_copy(kill_buffer, hedit->buffer, 0, begin, count);
  if (count > 1)
    he_message(0, "0x%lx (%li) bytes yanked.", count, count);
  return skip;
}
/* exhcmd_yank */

  static char *
exhcmd_version(struct he_s *hedit, char *args)
  /* ARGSUSED */
{
  char *skip = exh_skipcmd(args, 0);
  hexer_version();
  return skip;
}
/* exhcmd_version */

  static char *
exhcmd_zz(struct he_s *hedit, char *args)
{
  int k;
  char *skip;

  skip = exh_skipcmd(args, 0);
  if ((k = HE_LINE(hedit->position - hedit->screen_offset)) < hx_lines / 2 - 1)
    he_scroll_down(hedit, (hx_lines / 2 - 1) - k);
  if ((k = HE_LINE(hedit->position - hedit->screen_offset)) > hx_lines / 2 - 1)
    he_scroll_up(hedit, k - (hx_lines / 2 - 1));
  he_update_screen(hedit);
  return skip;
}
/* exhcmd_zz */

  static char *
exhcmd_zt(struct he_s *hedit, char *args)
{
  char *skip;

  skip = exh_skipcmd(args, 0);
  he_scroll_up(hedit, HE_LINE(hedit->position - hedit->screen_offset));
  he_update_screen(hedit);
  return skip;
}
/* exhcmd_zt */

  static char *
exhcmd_zb(struct he_s *hedit, char *args)
{
  char *skip;

  skip = exh_skipcmd(args, 0);
  he_scroll_down(hedit,
                 hx_lines - 2 - HE_LINE(hedit->position - hedit->screen_offset));
  he_update_screen(hedit);
  return skip;
}
/* exhcmd_zb */

  static char *
exhcmd_help(struct he_s *hedit, char *args)
{
  int pid1, pid2, status, x = 0;
  int pipefd[2];
  char *vp[64], *p;
  char *skip;
  int i;

  skip = exh_skipcmd(args, 0);
  tio_suspend();
  tio_clear();
  tio_flush();
  if (pipe(pipefd)) {
    he_message(1, "INTERNAL ERROR: `pipe()' failed.");
    goto exit_exhcmd_help;
  }
  if ((pid1 = fork()) < 0) { /* `fork()' failed */
    he_message(1, "INTERNAL ERROR: `fork()' failed.");
    close(pipefd[0]);
    close(pipefd[1]);
    goto exit_exhcmd_help;
  } else if (!pid1) { /* child */
    close(pipefd[1]);
    close(0);
    if (dup(pipefd[0]) == -1)
      exit(EXIT_EXEC_FAILED);
    p = (char *)alloca(strlen(he_pagerprg) + 1);
    strcpy(p, he_pagerprg);
    for (vp[i = 0] = p; *p; ++p)
      if (*p == ' ') {
        for (*p++ = 0; *p == ' ' || *p == '\t'; ++p);
        if (!*p) break;
        vp[++i] = p;
      }
    vp[++i] = 0;
    execvp(*vp, vp);
    exit(EXIT_EXEC_FAILED);
  }
  if ((pid2 = fork()) < 0) {
    he_message(1, "INTERNAL ERROR: `fork()' failed.");
    close(pipefd[0]);
    close(pipefd[1]);
    goto exit_exhcmd_help;
  } else if (!pid2) { /* child */
    size_t len, n;
    ssize_t res;

    for (n = 0, len = strlen(helptext); n < len; n += res)
      if ((res = write(pipefd[1], helptext + n, len - n)) == -1)
	exit(1);
    close(pipefd[1]);
    exit(0);
  }
  close(pipefd[1]);
  do if (waitpid(pid1, &status, 0) >= 0) break; while (errno == ERESTARTSYS);
  kill(pid2, SIGKILL);
  do if (waitpid(pid2, 0, 0) >= 0) break; while (errno == ERESTARTSYS);
  close(pipefd[0]);
  if ((x = WEXITSTATUS(status))) {
    if (x == EXIT_EXEC_FAILED)
      he_message(1, "couldn't start pager program `%s'", he_pagerprg);
    else
      he_message(1, "%s exited with code %i", he_pagerprg, x);
  }
exit_exhcmd_help:
  tio_restart();
  he_refresh_all(hedit);
  return skip;
}
/* exhcmd_help */

  static char *
exhcmd_exit(struct he_s *hedit, char *args)
{
  struct buffer_s *i;
  int cant_write_f = 0;
  long k;
  char *skip, *p;

  skip = exh_skipcmd(args, p = (char *)alloca(strlen(args) + 1));
  util_trunc(args = p);
  for (i = buffer_list; i; i = i->next)
    if (i->loaded_f && i->path ? i->hedit->buffer->modified : 0) {
      if (i->hedit->read_only) {
        he_message(0, "@Abbuffer@~ `%s' @Abis readonly@~ - not written",
                   i->hedit->buffer_name);
        cant_write_f = 1;
      } else {
        k = b_write_buffer_to_file(i->hedit->buffer, i->path);
        if (k < 0) {
          he_message(0, "@Aberror writing file@~ `%s': %s", i->path,
              strerror(errno));
          cant_write_f = 1;
        } else {
          he_message(0, "wrote `%s' - 0x%lx (%li) bytes", i->path, k, k);
          hedit->buffer->modified = 0;
        }
      }
    }
  if (!cant_write_f) exhcmd_quit(hedit, "");
  return skip;
}
/* exhcmd_exit */

const struct exh_cmd_s exh_commands[] = {
  { "substitute", 's', (exh_fn)exhcmd_substitute, 1, 0 },
  { "write", 'w', (exh_fn)exhcmd_write, 1, 1 },
  { "read", 'r', (exh_fn)exhcmd_read, 0, 1 },
  { "edit", 'e', (exh_fn)exhcmd_edit, 1, 3 },
  { "buffer", 'b', (exh_fn)exhcmd_buffer, 1, 2 },
  { "next", 'n', (exh_fn)exhcmd_next, 0, 0 },
  { "previous", 'N', (exh_fn)exhcmd_previous, 0, 0 },
  { "rewind", 0, (exh_fn)exhcmd_rewind, 0, 0 },
  { "skip", 'S', (exh_fn)exhcmd_skip, 0, 0 },
  { "wn", 0, (exh_fn)exhcmd_wn, 1, 1 },
  { "wall", 0, (exh_fn)exhcmd_wall, 0, 0 },
  { "quit", 'q', (exh_fn)exhcmd_quit, 0, 0 },
  { "close", 'c', (exh_fn)exhcmd_close, 0, 2 },
  { "map", 0, (exh_fn)exhcmd_map, 0, 0 },
  { "imap", 0, (exh_fn)exhcmd_imap, 0, 0 },
  { "vmap", 0, (exh_fn)exhcmd_vmap, 0, 0 },
  { "unmap", 0, (exh_fn)exhcmd_unmap, 0, 0 },
  { "iunmap", 0, (exh_fn)exhcmd_iunmap, 0, 0 },
  { "vunmap", 0, (exh_fn)exhcmd_vunmap, 0, 0 },
  { "set", 0, (exh_fn)exhcmd_set, 0, 4 },
  { "delete", 'd', (exh_fn)exhcmd_delete, 0, 0 },
  { "yank", 'y', (exh_fn)exhcmd_yank, 0, 0 },
  { "version", 0, (exh_fn)exhcmd_version, 0, 0 },
  { "zz", 0, (exh_fn)exhcmd_zz, 0, 0 },
  { "zt", 0, (exh_fn)exhcmd_zt, 0, 0 },
  { "zb", 0, (exh_fn)exhcmd_zb, 0, 0 },
  { "wq", 0, (exh_fn)exhcmd_exit, 0, 0 },
  { "help", 'h', (exh_fn)exhcmd_help, 0, 0 },
  { "exit", 'x', (exh_fn)exhcmd_exit, 0, 0 },
  { 0, 0, (exh_fn)0, 0, 0 }
};

/* end of commands.c */


/* VIM configuration: (do not delete this line)
 *
 * vim:bk:nodg:efm=%f\:%l\:%m:hid:icon:
 * vim:sw=2:sm:textwidth=79:ul=1024:wrap:
 */
