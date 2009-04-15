/* tio.h	8/19/1995
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

#ifndef _TIO_H_
#define _TIO_H_

#include "config.h"
#include "defs.h"

#define TIO_MAX_UNGET 4096

enum t_keys_e {
  KEY_ERROR      = -1,
  KEY_NONE       = 0,
  KEY_BACKSPACE  = 0x08,
  KEY_TAB        = 0x09,
  KEY_RETURN     = 0x0d,
  KEY_ESCAPE     = 0x1b,
  KEY_DELETE     = 0x7f,
  KEY_UP         = 0x100,
  KEY_DOWN,
  KEY_LEFT,
  KEY_RIGHT,
  KEY_F0,
  KEY_F1,
  KEY_F2,
  KEY_F3,
  KEY_F4,
  KEY_F5,
  KEY_F6,
  KEY_F7,
  KEY_F8,
  KEY_F9,
  KEY_F10,
  KEY_BACKTAB,
  KEY_BEGIN,
  KEY_CANCEL,
  KEY_CLOSE,
  KEY_COPY,
  KEY_CREATE,
  KEY_END,
  KEY_ENTER,
  KEY_EXIT,
  KEY_UPPER_LEFT,
  KEY_UPPER_RIGHT,
  KEY_CENTER,
  KEY_BOTTOM_LEFT,
  KEY_BOTTOM_RIGHT,
  KEY_HOME,
  KEY_PAGE_UP,
  KEY_PAGE_DOWN,
  KEY_BREAK,
  KEY_NULL
};

#define KEY_BIAS 254            /* bias used for storing non-character
                                 * keys in character strings */

extern struct t_keynames_s {
  enum t_keys_e key;
  char *name;
} t_keynames[];

#if USE_STDARG
extern void (*error_msg)( const char *, ... );
#else
extern void (*error_msg)( /* const char *, ... */ );
#endif
  /* Pointer to the error message function.
   */

  extern
tio_isprint( /* int x */ );

  extern
#if USE_STDARG
nprintf( const char *fmt, ... );
#else
nprintf( /* const char *fmt, ... */ );
#endif

  extern
vnprintf( /* const char *fmt, va_list ap */ );
  /* Returns the number of output characters a call to a printf-like function
   * would pruduce.
   */

  extern
tio_init( /* char *program_name */ );
  /* Initialize.  This function should be called before any other
   * `tio_*'-function.  `program_name' should be the name of the application.
   */

#define tio_flush() (void)fflush(stdout)
  /* Flush `stdout'.
   */

  extern void
tio_start_application( /* void */ );
  /* Send the start-application string to the terminal.
   */

  extern void
tio_end_application( /* void */ );
  /* Send the end-application string to the terminal.
   */

  extern void
tio_keypad( /* int on */ );
  /* Set the keypad mode.
   * on=0: numeric keypad.
   * on=1: application keypad.  Select this mode if you want to use the
   *       keypad (i.e. arrow-keys).
   */

  extern
tio_getch( /* void */ );

  extern
tio_get( /* void */ );
  /* Read a character from the keyboard.  `tio_getch()' waits for input,
   * `tio_get()' returns `KEY_NONE' (0) if no input is available.
   * The functions return the following special keys:
   *   KEY_ERROR                an error occured while waiting for input.
   *                            NOTE: Changing the size of the window causes
   *                              `tio_getch()' to return `KEY_ERROR'.
   *   KEY_NONE                 no key.
   *   KEY_BACKSPACE            backspace key.
   *   KEY_TAB                  TAB key.
   *   KEY_RETURN               return key.
   *   KEY_ESCAPE               escape key.
   *   KEY_DELETE               delete key.
   *   KEY_UP                   up arrow.
   *   KEY_DOWN                 down arrow.
   *   KEY_LEFT                 left arrow.
   *   KEY_RIGHT                right arrow.
   *   KEY_BACKTAB              BACKTAB key.
   *   KEY_F0, ... KEY_F10      function keys F0-F10.
   *   KEY_BEGIN                begin key.
   *   KEY_CANCEL               cancel key.
   *   KEY_CLOSE                close key.
   *   KEY_COPY                 copy key.
   *   KEY_CREATE               create key.
   *   KEY_END                  end key.
   *   KEY_ENTER                enter key.
   *   KEY_EXIT                 exit key.
   *   KEY_UPPER_LEFT           upper left key on keypad.
   *   KEY_UPPER_RIGHT          upper right key on keypad.
   *   KEY_CENTER               center key on keypad.
   *   KEY_BOTTOM_LEFT          bottom left key on keypad.
   *   KEY_BOTTOM_RIGHT         bottom right key on keypad.
   *   KEY_HOME                 home key.
   *   KEY_PAGE_UP              page up key.
   *   KEY_PAGE_DOWN            page down key.
   */

  extern
tio_tget( /* int timeout */ );
  /* Like `tio_get()', but waits `timeout' tenths of a second for input.
   * `tio_tget()' returns `KEY_NONE' (0) if nothing has been read.
   */

  extern
tio_ungetch( /* int x */ );
  /* Put the character `x' back into the input stream.  At most
   * `IO_MAX_UNGET' characters can be ungetch.  The return value is `x'
   * or -1 on error.
   */

  extern
tio_ungets( /* int *x */ );
  /* Put the character string `x' back into the input stream using
   * `tio_ungetch()'.  The return value is 0 and -1 on error.
   */

  extern
tio_readmore( /* void */ );
  /* Returns a non-zero value iff there are pending input characters.
   * NOTE:  Keys put into the unput-queue (via `tio_ungetch()' or
   *   `tio_ungets()') are not counted.
   */

  extern
tio_getmore( /* void */ );
  /* Returns a non-zero value iff there are any keys in the unget-queue.
   */

  extern
tio_testkey( /* int key */ );
  /* Returns 1, if a termcap entry for the requested key exists, else 0.
   * The function return always 1 for the keys `KEY_BACKSPACE', `KEY_TAB',
   * `KEY_RETURN', `KEY_ESCAPE', `KEY_DELETE', `KEY_NONE' and `KEY_ERROR'.
   */

  extern char *
tio_keyname( /* int key */ );
  /* Returns the name of the key `key'.  If `key' is a printable character,
   * it is returned as a string.  If `key' is a special key, the name of
   * that key is returned.  If `key' is unknown and greater than 0xff "??"
   * is returned, else a `\x??' hexadecimal code.
   */

  extern char *
tio_keyrep( /* int key */ );
  /* Returs a string representation of `key'.  If `key' is not a printable
   * character an escape sequence is generated:
   *   key > 0xff      if key is a known special key, a unique sequence
   *                   ~<keyname> (i.e. ~UP for up-arrow)
   *   key < 0x1f      standard ^<char> escape sequence
   *   key == 0x7f     ^? (delete)
   *   else            a \x?? escape (hex representation of the key)
   * if `key' is a printable character, a string containing that character
   * is returned.
   */

  extern char *
tio_vkeyrep( /* int key */ );
  /* Similar to `tio_keyrep', but returns a long string representation
   * whenever available.
   */

  extern char *
tio_keyscan( /* int *key, char *s, int mode */ );
  /* Check if `s' is a sting representation of a key.  on success,
   * the keycode is written to `*key' and a pointer to the first
   * character after the srep is returned, else `*key' is set to 0
   * and `s' is returned.
   * mode & 1:  scan for escapes starting with a `^'.
   * mode & 2:  scan for escapes starting with a `~'.
   * mode == 0:  scan for all known escapes.
   */

  extern
tio_echo( /* int on */ );
  /* on=1:  characters are echoed as they are typed.
   * on=0:  characters are not echoed.
   * RETURN VALUE:  Previous echo-state.
   */

  extern void
tio_return( /* void */ );
  /* Move cursor to column 0.
   */

  extern void
tio_home( /* void */ );
  /* Cursor home.
   */

  extern void
tio_goto_line( /* int line */ );
  /* Move cursor to line `line'.
   */

  extern void
tio_goto_column( /* int line */ );
  /* Move cursor to column `column'.
   */

  extern void
tio_last_line( /* void */ );
  /* Move cursor to the last line, column 0.
   */

  extern
tio_scroll_up( /* int count, int first, int last */ );
  /* Scroll up `count' lines; insert `count' lines at the bottom of the
   * screen.
   * The function returns -1 if the terminal can't scroll backwards.
   */

  extern
tio_scroll_down( /* int count, int first, int last */ );
  /* Scroll down `count' lines; insert `count' lines at the top of the screen.
   * The function returns -1 if the terminal can't scroll backwards.
   */

  extern void
tio_display( /* char *text, int indent */ );
  /* Send the string `text' to the terminal.  The string may contain the
   * following `@*'-commands:
   *
   *  @@                   @
   *  @c                   clear screen.
   *  @M[+-]rrr[+-]ccc     absolute move.
   *                       Move the cursor to position row rrr, column ccc.
   *                       If a value is prefixed by a `-', it is interpreted
   *                       relative to the bottom-right corner of the screen.
   *  @m[+-]rrr[+-]ccc     relative move.
   *  @Iiii                set indent.
   *  @b                   bell.
   *  @v                   flash, visible bell.
   *  @U                   underline on.
   *  @u                   underline off.
   *  @Am                  set attribute m, where m is one of the
   *                       following:
   *                        b  bold.
   *                        l  blink.
   *                        h  half bright.
   *                        r  reverse.
   *                        u  underline (equivalent to `@U').
   *                        s  standout.
   *                        ~  normal.
   *  @r                   return.
   *  @n                   newline.
   *
   * `indent' sets the indent.  If `indent < 0', the indent stays unchanged.
   */

  extern void
tio_message( /* char **message, int indent */ );
  /* displays the array of strings `message' via `tio_display()' providing
   * a simple pager.  this function assumes, that every string in
   * `message' displays as a single line.
   */

  extern
#if USE_STDARG
tio_printf( const char *fmt, ... );
#else
tio_printf( );
#endif

  extern
tio_vprintf( /* const char *fmt, va_list */ );
  /* Similar to `printf()'.  `tio_printf()' understands the same @-commands
   * as `tio_display()'.  Note that @-commands in strings inserted via %s
   * are executed.  Use `tio_raw_printf()' if you don't wan't @-commands to
   * be executed.
   */

  extern
#if USE_STDARG
tio_raw_printf( const char *fmt, ... );
#else
tio_raw_printf( /* const char *fmt, ... */ );
#endif

  extern
tio_raw_vprintf( /* const char *fmt, va_list */ );
  /* Like `printf()'.  No @-commands.
   */

  extern
tio_puts( /* char *s */ );
  /* Like `fputs(s, stdout)'.
   */

  extern
tio_putchar( /* int x */ );
  /* Like `putchar(x)'.
   */

  extern void
tio_up( /* int count */ );
  /* Move cursor up `count' lines.
   */

  extern void
tio_down( /* int count */ );
  /* Move cursor down `count' lines.
   */

  extern void
tio_left( /* int count */ );
  /* Move cursor left `count' lines.
   */
  extern void
tio_right( /* int count */ );
  /* Move cursor right `count' lines.
   */

  extern void
tio_move( /* int line, int column */ );
  /* Move the cursor to position `line'/`column'.
   */

  extern void
tio_rel_move( /* int line, int column */ );
  /* Move the cursor relative to the cursor position.
   */

  extern void
tio_last_line( /* void */ );
  /* Move the cursor to the last line, first column.
   */

  extern void
tio_clear( /* void */ );
  /* Clear screen.
   */

  extern void
tio_clear_to_eol( /* void */ );
  /* Clear to end of line.
   */

  extern void
tio_bell( /* void */ );

  extern void
tio_visible_bell( /* void */ );

  extern void
tio_underscore( /* void */ );

  extern void
tio_underscore_off( /* void */ );

  extern void
tio_bold( /* void */ );

  extern void
tio_blink( /* void */ );

  extern void
tio_half_bright( /* void */ );

  extern void
tio_reverse( /* void */ );

  extern void
tio_normal( /* void */ );
  /* End all standout modes.
   */

  extern void
tio_set_cursor( /* int mode */ );
  /* Set the visibility of the cursor.
   * `mode == 0':  invisible.
   * `mode == 1':  normal.
   * `mode == 2':  standout.
   */

  extern
tio_have_color( /* void */ );
  /* returns a non-zero value if color is available, 0 else.
   */

  extern void
tio_set_colors( /* int fg_color, int bg_color */ );

  extern void
tio_set_fg( /* int color */ );
  /* set the foreground color to `color'.
   */

  extern void
tio_set_bg( /* int color */ );
  /* set the background color to `color'.
   */

  extern
tio_get_fg( /* void */ );
  /* return the current foreground color.
   */

  extern
tio_get_bg( /* void */ );
  /* return the current background color.
   */

  extern
tio_insert_character( /* char x */ );
  /* Insert the character `x' at the current position.
   */

  extern
tio_delete_character( /* void */ );
  /* Delete the character under the cursor.
   */

  extern
tio_readwait( /* int timeout */ );
  /* Wait until input is availabe on `stdin' using `select()'.
   * The `timeout' is measured in microseconds.  If `timeout' is a negative
   * value, `tio_readwait()' blocks until input is availabe.  If
   * `timeout == 0', `tio_readwait()' will return immeiately.
   * Return values:
   *  1  Success.  Data available on `stdin'.
   *  0  Timeout.  No input availabe.
   *  -1 Error.  See `select(2)'.
   * If `tio_readwait()' is interrupted by a signal, -1 is returned and
   * `errno' is set to `EINTR'.
   * NOTE:  if the unread-buffer is not empty, 1 is returned.
   */

  extern
tio_raw_readwait( /* int timeout */ );
  /* Similar to `tio_readwait()', but ignores the unread buffer.
   */

  extern
tio_getwait( /* int timeout */ );
  /* Similar to `tio_readwait()', but returns also if the unget buffer is
   * not empty.
   */

  extern void
tio_reset( /* void */ );

  extern void
tio_suspend( /* void */ );

  extern void
tio_restart( /* void */ );

#ifndef TIO_MAP
#define TIO_MAP 0
#endif

#if TIO_MAP

  extern
tio_mgetch( /* int map, char *map_string */ );

  extern
tio_map( /* int map, int *from, int *to */ );

  extern
tio_unmap( /* int map, int *from */ );

  extern char **
tio_maplist( /* int map */ );

#endif
#endif

/* end of tio.h */


/* VIM configuration: (do not delete this line)
 *
 * vim:aw:bk:ch=2:nodg:efm=%f\:%l\:%m:et:hid:icon:
 * vim:sw=2:sc:sm:si:textwidth=79:to:ul=1024:wh=12:wrap:wb:
 */
