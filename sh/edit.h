/* NAME:
 *      edit.h - globals for edit modes
 *
 * DESCRIPTION:
 *      This header defines various global edit objects.
 *
 * SEE ALSO:
 *      
 *
 * RCSid:
 *      $Id: edit.h,v 1.2 1992/04/25 08:33:28 sjg Exp $
 *
 */

/* some useful #defines */
#ifdef EXTERN
# define _I_(i) = i
#else
# define _I_(i)
# define EXTERN extern
# define EXTERN_DEFINED
#endif

#define	BEL		0x07

/*
 * The following are used for my horizontal scrolling stuff
 */
EXTERN	char   *xbuf;		/* beg input buffer */
EXTERN	char   *xend;		/* end input buffer */
EXTERN char    *xcp;		/* current position */
EXTERN char    *xep;		/* current end */
EXTERN char    *xbp;		/* start of visible portion of input buffer */
EXTERN char    *xlp;		/* last char visible on screen */
EXTERN int	x_adj_ok;
/*
 * we use x_adj_done so that functions can tell 
 * whether x_adjust() has been called while they are active.
 */
EXTERN int	x_adj_done;

EXTERN int	x_cols;
EXTERN int	x_col;
EXTERN int	x_displen;
EXTERN int	x_arg;		/* general purpose arg */

EXTERN int	x_do_init;		/* set up tty modes */
EXTERN int	ed_erase, ed_kill, ed_werase, ed_intr, ed_quit;

#ifdef DEBUG
# define _D_(x) x
#else
# define _D_(x)
#endif

/****  edit.c  ****/
int             x_read      ARGS((int fd, char *buf, size_t len));
int             x_getc      ARGS((void));
void            x_flush     ARGS((void));
void            x_adjust    ARGS((void));
void            x_putc      ARGS((int c));
int             x_debug_info ARGS((void));
void            x_puts      ARGS((char *s));
void            x_init      ARGS((void));
bool_t          x_mode      ARGS((bool_t onoff));
bool_t          x_mode      ARGS((bool_t onoff));
int             promptlen   ARGS((char *cp));

/****  emacs.c  ****/
void            x_redraw    ARGS((int limit));
char*		x_lastcp    ARGS((void));
EXTERN int xlp_valid _I_(0);
  
/* This lot goes at the END */
/* be sure not to interfere with anyone else's idea about EXTERN */
#ifdef EXTERN_DEFINED
# undef EXTERN_DEFINED
# undef EXTERN
#endif
/*
 * Local Variables:
 * version-control:t
 * comment-column:40
 * End:
 */
