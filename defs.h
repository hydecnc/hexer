/* defs.h:
 */

/*
 * General settings:
 */
#define TMP_DIR "/tmp"

/*
 * Buffer: (defaults)
 */
#define BUFFER_BLOCKSIZE 4096
#define BUFFER_OPT 0
#define BUFFER_MAX_BLOCKS 16 /* NIY */

/*
 * Fifo: (defaults)
 */
#define FIFO_BLOCKSIZE 1547

/* I/O settings (for tio.c):
 */
#define STDOUT_BUFFER_SIZE 8192
#define TERMINAL_CANT "Terminal can't"

/* hexer:
 */
#define HE_ANYCHAR '.'
#define HE_DEFAULT_PAGER "more"
#define HEXERINIT_FILE ".hexerrc"
#define TIO_READWAIT_TIMEOUT -1

#if !HAVE_CONST
#define const
#endif

/* end of defs.h */


/* VIM configuration: (do not delete this line)
 *
 * vim:bk:nodg:efm=%f\:%l\:%m:hid:icon:
 * vim:sw=2:sm:textwidth=79:ul=1024:wrap:
 */
