/* exh.c	8/19/1995
 * The EXH-command interpreter, version 0.8
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
#include <assert.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "buffer.h"
#include "hexer.h"
#include "commands.h"
#include "exh.h"
#include "regex.h"
#include "set.h"
#include "tio.h"

#define EXH_DEFAULT_SHELL "/bin/sh"
#define EXH_DEFAULT_PAGER "more"
#define EXIT_EXEC_FAILED 27

#ifndef ERESTARTSYS
#define ERESTARTSYS EINTR
#endif

char *exh_initialize[] = {
  ":map b :?\\<[~HAT ^V^I]^M",
  ":map w :/\\<[~HAT ^V^I]^M",
  ":map e :/[~HAT ^V^I]\\>^M",
  ":map n /^M",
  ":map N ?^M",
  ":map zz :zz^M",
  ":map zt :zt^M",
  ":map zb :zb^M",
  ":map ZZ :x^M",
  0
};

  static int
exh_shell_command(char *command, int pager_f)
                    /* command to be executed by the shell specified by
                     * the "SHELL" environment variable.  the default shell
                     * is `EXH_DEFAULT_SHELL'. */
                    /* if `pager != 0' the output is piped into the
                     * pager specified by the "PAGER" environment.  the
                     * default pager is `EXH_DEFAULT_PAGER'. */
  /* we won't use the "-c"-switch (available for most shells), instead we'll
   * gonna pipe the command into the shell.
   */
{
  int shell_pid, pager_pid;
  int shell_status, pager_status;
  int shell_x, pager_x;
  int pipe1[2], pipe2[2];
  char *shell[64], *pager[64], *p;
  int i;

  tio_suspend();
  tio_clear();
  tio_flush();
  if (!(*shell = getenv("SHELL"))) *shell = EXH_DEFAULT_SHELL;
  *shell = strdup(*shell);
  if (!(*pager = getenv("PAGER"))) *pager = EXH_DEFAULT_PAGER;
  *pager = strdup(*pager);
  /* break `*shell' and `*pager' down into whitespace separated
   * substrings.  it is *not* possible to mask whitespace characters in any
   * way. */
  for (i = 0; *shell[i]; shell[++i] = p) {
    for (p = shell[i]; *p && *p != ' ' && *p != '\t'; ++p);
    if (*p) for (*p++ = 0; *p == ' ' || *p == '\t'; ++p);
  }
  shell[i] = 0;
  for (i = 0; *pager[i]; pager[++i] = p) {
    for (p = pager[i]; *p && *p != ' ' && *p != '\t'; ++p);
    if (*p) for (*p++ = 0; *p == ' ' || *p == '\t'; ++p);
  }
  pager[i] = 0;
  pipe1[0] = pipe1[1] = -1;
  if (pipe(pipe1) ? 1 : pager_f ? pipe(pipe2) : 0) {
    he_message(1, "INTERNAL ERROR: `pipe()' failed.");
    if (pipe1[0] > 0) { close(pipe1[0]); close(pipe1[1]); }
    goto exit_exh_shell_command;
  }
  /* execute the shell (and the pager)
   */
  if ((shell_pid = fork()) > 0) { /* parent */
    close(pipe1[0]);
    if (pager_f) { /* start up the pager */
      if ((pager_pid = fork()) > 0) { /* parent */
        close(pipe2[0]);
        close(pipe2[1]);
        /* send the command to the shell */
        if (write(pipe1[1], command, strlen(command)) != strlen(command) ||
	    write(pipe1[1], "\n", 1) != 1) {
	  close(pipe1[1]);
	  he_message(1, "couldn't send the `%s' command to the pager", command);
	  close(pipe2[0]);
	  close(pipe2[1]);
	  pager_f = 0;
	} else {
        close(pipe1[1]);
        do if (waitpid(shell_pid, &shell_status, 0) >= 0) break;
          while (errno == ERESTARTSYS);
        do if (waitpid(pager_pid, &pager_status, 0) >= 0) break;
          while (errno == ERESTARTSYS);
        if ((pager_x = WEXITSTATUS(pager_status))) {
          if (pager_x == EXIT_EXEC_FAILED) {
            he_message(1, "couldn't start pager program `%s'", *pager);
            close(pipe2[0]);
            close(pipe2[1]);
            pager_f = 0;  /* we clear the `pager_f'-flag, so the
                           * "press any key"-requester gets displayed. */
          } else
            he_message(1, "%s exited with code %i", *pager, pager_x);
        }
        if ((shell_x = WEXITSTATUS(shell_status))) {
          if (shell_x == EXIT_EXEC_FAILED)
            he_message(1, "couldn't start shell `%s'", *shell);
          else
            he_message(1, "%s exited with code %i", *shell, shell_x);
        }
        if (!shell_x && !pager_x)
          he_message(0, "shell command done");
	}
      } else if (pager_pid < 0) { /* `fork()' failed */
        close(pipe2[0]);
        close(pipe2[1]);
        he_message(1, "INTERNAL ERROR: `fork()' failed.");
        /* hmm... i think it's better not to send the command in case the
         * pager failed. */
        close(pipe1[1]);
        do if (waitpid(shell_pid, &shell_status, 0) >= 0) break;
          while (errno == ERESTARTSYS);
        /* we don't care about the exit status */
        he_message(1, "shell command failed");
      } else { /* child: pager program */
        close(pipe1[0]);
        close(pipe1[1]);
        close(pipe2[1]);
        close(0);
        if (dup(pipe2[0]) != 0)
	  exit(EXIT_EXEC_FAILED);
        execvp(*pager, pager);
        exit(EXIT_EXEC_FAILED);
      }
    } else { /* !pager_f */
      if (write(pipe1[1], command, strlen(command)) != strlen(command) ||
	  write(pipe1[1], "\n", 1) != 1) {
	close(pipe1[1]);
	he_message(1, "couldn't send command `%s' to pager", command);
      } else {
      close(pipe1[1]);
      do if (waitpid(shell_pid, &shell_status, 0) >= 0) break;
        while (errno == ERESTARTSYS);
      if ((shell_x = WEXITSTATUS(shell_status))) {
        if (shell_x == EXIT_EXEC_FAILED)
          he_message(1, "couldn't start shell `%s'", *shell);
        else
          he_message(1, "%s exited with code %i", *shell, shell_x);
      }
      if (!shell_x) he_message(0, "shell command done");
      }
    }
  } else { /* child: shell */
    close(pipe1[1]);
    close(pipe2[0]);
    close(0);
    if (dup(pipe1[0]) != 0)
      exit(EXIT_EXEC_FAILED);
    if (pager_f) {
      close(1);
      if (dup(pipe2[1]) != 1)
	exit(EXIT_EXEC_FAILED);
      close(2);
      if (dup(pipe2[1]) != 2)
	exit(EXIT_EXEC_FAILED);
    }
    /* execute the shell */
    execvp(*shell, shell);
    exit(EXIT_EXEC_FAILED);
  }
#if 0
  if (!pager_f) {
    tio_last_line();
    tio_printf("\n@Ar press any key @~@r");
    tio_getch();
    tio_clear_to_eol();
  }
#else
  tio_last_line();
  tio_printf("\n@Ar press any key @~@r");
  tio_getch();
  tio_clear_to_eol();
#endif
exit_exh_shell_command:
  tio_restart();
  free(*shell);
  free(*pager);
  return 0;
}
/* exh_shell_command */

  static int
exh_subshell(void)
  /* start a subshell session.
   */
{
  int pid, status, x;
  char *shell[64], *p;
  int i;

  tio_suspend();
  tio_clear();
  tio_flush();
  if (!(*shell = getenv("SHELL"))) *shell = EXH_DEFAULT_SHELL;
  *shell = strdup(*shell);
  /* break `*shell' down into whitespace separated substrings.
   * it is *not* possible to mask whitespace characters in any way. */
  for (i = 0; *shell[i]; shell[++i] = p) {
    for (p = shell[i]; *p && *p != ' ' && *p != '\t'; ++p);
    if (*p) for (*p++ = 0; *p == ' ' || *p == '\t'; ++p);
  }
  shell[i] = 0;
  /* execute the shell */
  if ((pid = fork()) > 0) {
    do if (waitpid(pid, &status, 0) >= 0) break; while (errno == ERESTARTSYS);
    if ((x = WEXITSTATUS(status))) {
      if (x == EXIT_EXEC_FAILED)
        he_message(1, "couldn't start shell `%s'", *shell);
      else
        he_message(1, "%s exited with code %i", *shell, x);
    }
  } else if (!pid) { /* child: shell */
    tio_printf("@Absubshell@~.  type `@Uexit@u' to return to @Abhexer@~.\n");
    tio_flush();
    execvp(*shell, shell);
    exit(EXIT_EXEC_FAILED);
  } else { /* `fork()' failed */
    he_message(1, "INTERNAL ERROR: `fork()' failed.");
  }
  tio_restart();
  free(*shell);
  return 0;
}
/* exh_subshell */

  char *
exh_skip_expression(exp, separator)
  char *exp;
  char separator;
  /* returns a pointer to the first unmasked occurrence of the separator
   * `separator'; separators within a range or prefixed by `\' are ignored.
   * if no separator is found, 0 is returned.
   */
{
  char *p = exp;
  char *s = 0;

  for (;; ++p) {
    while (*p && *p != separator && *p != '\\' && *p != '[') ++p;
    if (!*p) break;
    if (*p == '\\') {
      ++p; continue;
    } else if (*p == '[') {
      ++p;
      if (*p == '^') ++p;
      if (*p == ']') ++p;
      for (;; ++p) {
	while (*p && *p != ']' && *p != '\\') ++p;
        if (*p == '\\') { ++p; continue; }
	if (!*p || *p == ']') break;
      }
    } else {
      assert(*p == separator);
      s = p;
      break;
    }
  }
  return s;
}
/* exh_skip_expression */

  char *
exh_skip_replace(exp, separator)
  char *exp;
  char separator;
  /* similar to `exh_skip_expression()', but ignores the special meaning of
   * brackets.
   */
{
  char *p = exp;
  char *s = 0;

  for (;; ++p) {
    while (*p && *p != separator && *p != '\\') ++p;
    if (!*p) break;
    if (*p == '\\') {
      if (p[1] == separator) ++p;
    } else { 
      assert(*p == separator);
      s = p;
      break;
    }
  }
  return s;
}
/* exh_skip_replace */

  static char *
exh_get_number(char *exp, long *number)
{
  char *p;

  switch (*exp) {
    case '0':  /* octal or hex number */
      p = exp + 1;
      if (*p == 'x' || *p == 'X') {  /* hex number */
	*number = 0;
	++p;
	while (isdigit(*p) || (tolower(*p) >= 'a' && tolower(*p) <= 'f')) {
	  if (*number & (0xf << 27)) { /* too large */
	    ++p;
	    continue;
	  }
	  *number <<= 4;
	  *number += isdigit(*p) ? *p - '0' : tolower(*p) - 'a' + 10;
	  ++p;
	}
      } else { /* octal number */
        *number = 0;
	while (*p >= '0' && *p <= '7') {
	  if (*number & (0x7 << 28)) { /* too large */
	    ++p;
	    continue;
	  }
	  *number <<= 3;
	  *number += *p - '0';
	  ++p;
	}
      }
      break;
    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9':  /* decimal number */
      p = exp;
      *number = 0;
      while (isdigit(*p)) {
	if (*number & (0xf << 27)) { /* too large */
	  ++p;
	  continue;
	}
	*number *= 10;
	*number += *p - '0';
	++p;
      }
      break;
    default:
      return 0;
  }
  return p;
}
/* exh_get_number */

  static char *
exh_get_address(struct he_s *hedit, char *exp, long *address)
  /* evaluate the specified address and store it in `*address'.  on success
   * a skip-pointer for the expression is returned, else (if `exp' is not
   * a valid address or expression) 0 is returned.
   */
{
  char *p = exp, *q;
  char *rs;
  long rl, ml;
  int direction;
  char separator;
  long n;
  long positive_f;
  long offset;
  int eox_f = 0;
  long old_position = hedit->position;

  for (; p && !eox_f;) {
    positive_f = 0;
    direction = 1;
    separator = '/';
    switch (*p) {
      case '?':  /* reverse search */
        direction = -1;
        separator = '?';
        /* fall through */
      case '/':  /* forward search */
        p = exh_skip_expression(q = p + 1, separator);
        if (p) *p++ = 0; else p = exp + strlen(exp);
        *address = he_search(hedit, q, "", direction, 0, 1, -1, &rs, &rl, &ml);
        switch (*address) {
          case -2:  /* invalid expression */
            assert(rx_error);
            he_message(0, "@Abinvalid expression:@~ %s",
                       rx_error_msg[rx_error]);
            p = 0;
            break;
          case -1:  /* no match */
            if (!rx_error)
              he_message(0, "no match");
            else
              he_message(0, "@Abregexp error:@~ %s", rx_error_msg[rx_error]);
            p = 0;
            break;
          default:  /* match */
            hedit->position = *address;
            free((char *)rs);
            break;
        }
        break;
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        p = exh_get_number(p, address);
        if (*address > (n = hedit->buffer->size)) {
          he_message(0, "only 0x%lx (%li) bytes", n, n);
          p = 0;
        } else
          hedit->position = *address;
        break;
      case '#':  /* line number */
        p = exh_get_number(p + 1, &n);
        if ((*address = b_goto_line(hedit->buffer, n)) < 0) {
          n = b_no_lines(hedit->buffer);
          he_message(0, "only 0x%lx (%lu) lines", n, n);
          p = 0;
        } else
          hedit->position = *address;
        break;
      case '.':  /* current position */
        p = exp + 1;
        *address = hedit->position;
        break;
      case '$':  /* end of buffer */
        p = exp + 1;
        hedit->position = *address = hedit->buffer->size;
        break;
      case '+':  /* positive offset */
        positive_f = 1;
        /* fall through */
      case '-':  /* negative offset */
        if (!(p = exh_get_number(p + 1, &offset))) {
          he_message(0, "@Abinvalid expression:@~ offset expected after `%c'",
                     positive_f ? '+' : '-');
          p = 0;
          break;
        }
        if (positive_f)
          *address += offset;
        else
          *address -= offset;
        if (*address < 0) *address = 0;
        if (*address > hedit->buffer->size) *address = hedit->buffer->size;
        hedit->position = *address;
        break;
      default:
        eox_f = 1;
        break;
    }
  }
  if (!p) hedit->position = old_position;
  return p;
}
/* exh_get_address */

  int
exh_command(hedit, cmd)
  struct he_s *hedit;
  char *cmd;
  /* execute the exh-command `cmd' on `hedit'.  multiple commands may be
   * specified (separated by semicolons).
   * note:  if any of the commands changes the current buffer, all successive
   *   commands will affect the current buffer.  i.e. the command
   *     :ed TESTFILE; 0x400
   *   warps you to position 0x400 in "TESTFILE", no matter what value of
   *   of `hedit' was specified.
   */
{
  char command[256];
  char *p;
  long begin = 0, end = 0;
  int i, j, k;
  int terminal_match_f = -1;
  char *skip;
  struct buffer_s *current = current_buffer;

  p = (char *)alloca(strlen(cmd) + 1);
  strcpy(p, cmd);
  /* remove trailing whitespace */
  for (i = strlen(p) - !!*p; i && (p[i] == ' ' || p[i] == '\t'); --i);
  if (*p) p[i + 1] = 0;
check_terminal:
  while (*p == ':') ++p;
  /* remove leading whitespace */
  while (*p == ' ' || *p == '\t') ++p;
  /* check if a terminal condition is specified. */
  for (i = 0; isalnum(p[i]); ++i);
  if (p[i] == ':') {
    char term[256];
    for (j = 0; j < i; ++j) term[j] = p[j];
    term[j] = 0;
    if (strcmp(term, s_get_option("TERM")) && terminal_match_f < 1)
      terminal_match_f = 0;
    else
      terminal_match_f = 1;
    p += i;
    goto check_terminal;
  }
  if (!terminal_match_f) goto exit_exh_command;
  /* check if an address or address range is specified in the command */
  if (*p == '%') { /* calculator command */
    ++p;
    begin = 0;
    end = hedit->buffer->size;
    goto exit_exh_command;
  } else if (*p == '!') { /* shell command */
    for (++p; *p == ' ' || *p == '\t'; ++p);
    if (*p)
      exh_shell_command(p, 1);
    else
      exh_subshell();
    he_refresh_all(hedit);
    goto exit_exh_command;
  } else {
    p = exh_get_address(hedit, p, &begin);
    if (!p) goto exit_exh_command;
    if (*p == ',') {
      ++p;
      p = exh_get_address(hedit, p, &end);
      if (!p) goto exit_exh_command;
    } else
      end = begin;
    if (begin > end) {
      if (he_query_yn(1, "@Abfirst address succeeds the second@~ - swap") > 0)
	begin ^= end, end ^= begin, begin ^= end;
      else
	goto exit_exh_command;
    }
  }

  /* read the command name */
  while (*p == ' ' || *p == '\t') ++p;
  for (i = 0; isalpha(*p); ++p, ++i) command[i] = *p;
  command[i] = 0;

  if (!*command) {
    /* no command.  go to the given address, if two adresses were selected,
     * select the specified area.  */
    if (end > begin) { /* area */
      he_select(hedit, begin, end);
      hedit->position = end;
      he_update_screen(hedit);
      hedit->position = begin;
    } else
      hedit->position = begin;
    goto exit_exh_command;
  }

  /* check if `command' is a single character command */
  if (!command[1])
    for (i = 0; exh_commands[i].cmd_name; ++i)
      if (exh_commands[i].cmd_char == *command) goto exh_execute;

  /* check if `command' is a unique prefix of a known command */
  for (i = 0, j = strlen(command), k = -1; exh_commands[i].cmd_name; ++i)
    if (!strncmp(command, exh_commands[i].cmd_name, j)) {
      if (k < 0)
	k = i;
      else {
	he_message(0, "ambiguous command `%s'", command);
	goto exit_exh_command;
      }
    }
  if (k < 0) {
    he_message(0, "unknown command `%s'", command);
    goto exit_exh_command;
  } else
    i = k;

exh_execute:
  if (begin == 0 && end == 0) {
    begin = exh_commands[i].whole_f ? 0 : hedit->position;
    end = exh_commands[i].whole_f ? hedit->buffer->size - 1 : hedit->position;
  }
  while (*p == ' ' || *p == '\t') ++p;
  skip = exh_commands[i].cmd(hedit, p, begin, end);
  if (skip) exh_command(current == current_buffer
                        ? hedit : current_buffer->hedit, skip);
exit_exh_command:
  return 0;
}
/* exh_command */

/* completer functions
 */

  static char **
exh_cpl_file_list(char *prefix)
{
  int i;
  DIR *dp;
  struct dirent *de;
  struct passwd *pe;
  char dirname[2048];
  char path[2048];  /* the real path with "~user" expanded. */
  char d_name[1024];
  char *user = 0;
  char *p;
  char **list;
  int plen = strlen(prefix);

  strcpy(dirname, prefix);
  /* remove the filename prefix from the path.
   */
  p = dirname + strlen(dirname) - !!*dirname;
  while (p > dirname) if (*p != '/') --p; else break;
  if (p != dirname || *dirname == '/') ++p;
  *p = 0;
  /* check if the `prefix' starts with a "~user/"-abbrev..
   */
  if (*prefix == '~') {
    user = strdup(prefix + 1);
    for (p = user; *p && *p != '/'; ++p);
    if (!*p) {
      /* hmm... the username `user' is possibly incomplete.  check how many
       * usernames match.
       */
      setpwent();
      for (i = 0; (pe = getpwent());)
        if (*user ? !strncmp(pe->pw_name, user, strlen(user)) : 1) ++i;
      setpwent();
      /* make the list.
       */
      *(list = (char **)malloc(sizeof(char *) * ++i)) = 0;
      for (i = 0; (pe = getpwent());)
        if (*user ? !strncmp(pe->pw_name, user, strlen(user)) : 1) {
          *d_name = '~';
          strcpy(d_name + 1, pe->pw_name);
          strcat(d_name, "/");
          list[i++] = strdup(d_name);
        }
      list[i] = 0;
      endpwent();
      goto exit;
    } else
      *p = 0;
    /* expand the user's homedir.
     */
    if (*user) {
      setpwent();
      for (; (pe = getpwent());) if (!strcmp(user, pe->pw_name)) break;
      if (!pe) {
        endpwent();
        goto empty_list;
      }
    } else {
      int uid = getuid();
      setpwent();
      for (; (pe = getpwent());) if (pe->pw_uid == uid) break;
      if (!pe) {
        endpwent();
        goto empty_list;
      }
    }
    strcpy(path, pe->pw_dir);
    if (path[strlen(path) - !!*path] != '/') strcat(path, "/");
    p = dirname + strlen(user) + 1;
    if (*p == '/') ++p;
    strcat(path, p);
    endpwent();
  } else
    strcpy(path, dirname);

  if (!(dp = opendir(*path ? path : "."))) goto empty_list;
  for (i = 0; (de = readdir(dp));) {
    strcpy(d_name, dirname);
    strcat(d_name, de->d_name);
    if (!strncmp(d_name, prefix, plen)) ++i;
  }
  list = (char **)malloc(sizeof(char *) * ++i);
  for (rewinddir(dp), i = 0; (de = readdir(dp));) {
    struct stat st;
    char *rp;
    strcpy(d_name, dirname);
    strcat(d_name, de->d_name);
    if (!strncmp(d_name, prefix, plen)) {
      rp = (char *)malloc(strlen(path) + strlen(de->d_name) + 4);
      strcpy(rp, path);
      strcat(rp, "/");
      strcat(rp, de->d_name);
      if (!stat(rp, &st)) if (S_ISDIR(st.st_mode)) strcat(d_name, "/");
      free(rp);
      list[i++] = strdup(d_name);
    }
  }
  list[i] = 0;
  closedir(dp);
exit:
  if (user) free(user);
  return list;
empty_list:
  if (user) free(user);
  *(list = (char **)malloc(sizeof(char *))) = 0;
  return list;
}
/* exh_cpl_file_list */

  static char **
exh_cpl_command_list(char *prefix)
{
  int i, j;
  int plen = strlen(prefix);
  char **list;

  for (i = 0, j = 0; exh_commands[i].cmd_name; ++i) {
    if (!strncmp(exh_commands[i].cmd_name, prefix, plen)) ++j;
    if (exh_commands[i].cmd_name[0] != exh_commands[i].cmd_char
        && exh_commands[i].cmd_char
        && (plen ? exh_commands[i].cmd_char == *prefix : 1)) ++j;
  }
  list = (char **)malloc((j + 1) * sizeof(char *));
  if (j) {
    for (i = 0, j = 0; exh_commands[i].cmd_name; ++i) {
      if (!strncmp(exh_commands[i].cmd_name, prefix, plen)) {
	list[j] =
          (char *)malloc(strlen(exh_commands[i].cmd_name) + 1);
	strcpy(list[j++], exh_commands[i].cmd_name);
      }
      if (exh_commands[i].cmd_name[0] != exh_commands[i].cmd_char
          && exh_commands[i].cmd_char
          && (plen ? exh_commands[i].cmd_char == *prefix : 1)) {
        list[j] = (char *)malloc(2);
        list[j][0] = exh_commands[i].cmd_char;
        list[j++][1] = 0;
      }
    }
    list[j] = 0;
  } else
    list[0] = 0;
  return list;
}
/* exh_cpl_command_list */

  static char **
exh_cpl_buffer_list(char *prefix)
{
  struct buffer_s *i;
  char **list;
  int j;
  int plen = strlen(prefix);

  for (j = 0, i = buffer_list; i; i = i->next)
    if (!strncmp(i->hedit->buffer_name, prefix, plen)) ++j;
  list = (char **)malloc((j + 1) * sizeof(char *));
  if (j) {
    for (j = 0, i = buffer_list; i; i = i->next)
      if (!strncmp(i->hedit->buffer_name, prefix, plen)) {
	list[j] = (char *)malloc(strlen(i->hedit->buffer_name) + 1);
	strcpy(list[j++], i->hedit->buffer_name);
      }
    list[j] = 0;
  } else
    list[0] = 0;
  return list;
}
/* exh_cpl_buffer_list */

  static char **
exh_cpl_file_and_buffer_list(char *prefix)
{
  char **list1 = exh_cpl_file_list(prefix);
  char **list2 = exh_cpl_buffer_list(prefix);
  int i, j, k;

  for (i = 0; list1[i]; ++i)
    for (j = 0; list2[j]; ++j)
      if (list2[j] != (char *)1) if (!strcmp(list1[i], list2[j])) {
	free((char *)list2[j]);
	list2[j] = (char *)1;
      }
  for (i = 0; list1[i]; ++i);
  for (j = k = 0; list2[j]; ++j) if (list2[j] != (char *)1) ++k;
  
  list1 = (char **)realloc(list1, (i + k + 1) * sizeof(char *));
  for (j = k = i; list2[j - i]; ++j)
    if (list2[j - i] != (char *)1) {
      list1[k] = list2[j - i];
      ++k;
    }
  list1[k] = 0;
  free((char *)list2);
  return list1;
}
/* exh_cpl_file_and_buffer_list */

  static char **
exh_cpl_option_list(char *prefix)
{
  char **list, **i;
  int j, k;
  int no_f = 0;

  if (!strncmp(prefix, "no", 2)) prefix += 2, no_f = 1;
  i = list = s_option_list(prefix, no_f);
  if (no_f) {
    while (*i) {
      *i = (char *)realloc(*i, k = strlen(*i) + 2);
      for (j = k; j >= 0; --j) (*i)[j] = (*i)[j - 2];
      **i = 'n', (*i)[1] = 'o';
      ++i;
    }
  }
  return list;
}
/* exh_cpl_option_list */

  char **
exh_completer(prefix, command, line, context)
  char *prefix;
  char *command;
  char *line; /* unused */
  int context;
  /* we're looking for completions of `prefix'; `command' is the
   * command-string that `prefix' is the argument of; `line' is the
   * complete line before the cursor.
   */
  /* ARGSUSED */
{
  int expect = -1;
  int i, j, k;
  char **(*completer[16])(char *);
  completer[0] = exh_cpl_command_list;
  completer[1] = exh_cpl_file_list;
  completer[2] = exh_cpl_buffer_list;
  completer[3] = exh_cpl_file_and_buffer_list;
  completer[4] = exh_cpl_option_list;

  if (context) {
    expect = -1;
    goto exit_exh_competer;
  }
  if (!*command)
    /* no command given */
    expect = 0;
  else {
    if (!command[1]) {
      /* single character command */
      for (i = 0; exh_commands[i].cmd_name; ++i)
	if (exh_commands[i].cmd_char == *command) {
	  /* `expect == -1' indicates, than no completion is done */
	  expect = exh_commands[i].expect - !exh_commands[i].expect;
	  break;
	}
      if (!exh_commands[i].cmd_name)
	/* unknown command - no completion */
	expect = -1;
    } else {
      /* check if `command' is a unique prefix of a known command */
      for (i = 0, j = strlen(command), k = 0; exh_commands[i].cmd_name; ++i)
	if (!strncmp(command, exh_commands[i].cmd_name, j)) {
	  if (!k)
	    k = i;
	  else {
	    /* ambiguous command - no completion */
	    expect = -1;
	    break;
	  }
        }
      if (!k)
	/* unknown command - no completion */
	expect = -1;
      else
        expect = exh_commands[k].expect;
    }
  }
exit_exh_competer:
  if (expect < 0) {
    char **list = (char **)malloc(sizeof(char *));
    *list = 0;
    return list;
  } else
    return completer[expect](prefix);
}
/* exh_completer */

/* end of exh.c */


/* VIM configuration: (do not delete this line)
 *
 * vim:bk:nodg:efm=%f\:%l\:%m:hid:icon:
 * vim:sw=2:sm:textwidth=79:ul=1024:wrap:
 */
