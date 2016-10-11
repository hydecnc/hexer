/* defs.h:
 * Copyright (c) 1995,1996 Sascha Demetrio
 * Copyright (c) 2009, 2015 Peter Pentchev
 */

/*
 * General settings:
 */
#define TMP_DIR "/tmp"

#ifndef __unused
#ifdef __GNUC__
#define __unused __attribute__((unused))
#define __printflike(x, y) __attribute__((format(printf, x, y)))
#else  /* __GNUC__ */
#define __unused
#define __printflike(x, y)
#endif /* __GNUC__ */
#endif /* __unused */

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
#ifndef HE_DEFAULT_PAGER
#define HE_DEFAULT_PAGER "more"
#endif
#define HEXERINIT_FILE ".hexerrc"
#define TIO_READWAIT_TIMEOUT -1

/* end of defs.h */


/* VIM configuration: (do not delete this line)
 *
 * vim:bk:nodg:efm=%f\:%l\:%m:hid:icon:
 * vim:sw=2:sm:textwidth=79:ul=1024:wrap:
 */
