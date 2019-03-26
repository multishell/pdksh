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
 *      $Id: edit.h,v 1.2 1994/05/19 18:32:40 michael Exp michael $
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

/* tty driver characters we are interested in */
typedef struct {
	int erase;
	int kill;
	int werase;
	int intr;
	int quit;
	int eof;
} X_chars;

EXTERN X_chars edchars;

/* emacs.c */
int 	x_emacs		ARGS((char *buf, size_t len));
void 	x_init_emacs	ARGS((void));
void	x_emacs_keys	ARGS((X_chars *ec));
/* vi.c */
int 	x_vi		ARGS((char *buf, size_t len));

#ifdef DEBUG
# define _D_(x) x
#else
# define _D_(x)
#endif

/* This lot goes at the END */
/* be sure not to interfere with anyone else's idea about EXTERN */
#ifdef EXTERN_DEFINED
# undef EXTERN_DEFINED
# undef EXTERN
#endif
#undef _I_
/*
 * Local Variables:
 * version-control:t
 * comment-column:40
 * End:
 */
