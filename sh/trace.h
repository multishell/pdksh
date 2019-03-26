/* NAME:
 *      trace.h - definitions for a simple trace facility
 *
 * SYNOPSIS:
 *      #include "trace.h"
 *
 * DESCRIPTION:
 *      Defines the macro _TRACE().
 *      Also declares Trace_log and Trace_level.
 *
 * SEE ALSO:
 *      
 *
 * AMENDED:
 *      91/11/22  22:53:58  (sjg)
 *
 * RELEASED:
 *      91/11/22  22:54:18  v1.2
 *
 * SCCSID:
 *      @(#)trace.h  1.2  91/11/22  22:53:58  (sjg)
 *
 *      @(#)Copyright (c) 1990 Simon J. Gerraty.
 */

/* some useful #defines */
#ifndef EXTERN
# define EXTERN extern
# define EXTERN_DEFINED
#endif

#ifndef TRUE
# define TRUE  1
# define FALSE 0
#endif
#ifndef ARGS
# if defined(__STDC__) || defined(PROTO)
#   define ARGS(p) p
# else
#   define ARGS(p) ()
# endif
#endif

/*
 * this garbage is sometimes needed when mixing
 * langauges or calling conventions under DOS.
 */
#ifndef _CDECL
# if defined(MSDOS) || defined(MSC)
#   ifndef NO_EXT_KEYS
#     define _CDECL  cdecl
#     define _NEAR   near
#   else
#     define _CDECL
#     define _NEAR
#   endif
# else /* not DrOS */
#   define _CDECL
#   define _NEAR
# endif /* DOS */
#endif /* _CDECL */

/* manifest constants */

/* struct / union */

/* macros */


#ifdef USE_TRACE
EXTERN char * _CDECL	Trace_log;
EXTERN int _CDECL 	Trace_level;

void _CDECL checkpoint	ARGS((char *fmt, ...));

/*
 * This macro takes a variabel number of args. 
 * examples:
 * 	_TRACE(5, ("The current Debug level is %d\n", Debug));
 * Note that if more than two args are provided, all but the 
 * first (which should be an integer indicating the Trace-level 
 * required for this message to be printed) must be enclosed in 
 * parenthesis. 
 */
# define _TRACE(lev, args) if (Trace_level >= lev) checkpoint args
#else 	/* don't USE_TRACE */
  /*
   * this macro evaluates to a harmless no entry
   * loop that most optimizers will remove all together.
   */
# define _TRACE(l, args) while (0)
#endif 	/* USE_TRACE */


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
