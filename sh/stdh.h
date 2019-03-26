/* NAME:
 *      stdh.h - standard headers
 *
 * SYNOPSIS:
 *      #include "stdh.h"
 *
 * DESCRIPTION:
 *      We use this header to encapsulate all the stddef et al 
 *      inclusion so that most of the source can ignore the 
 *      problems that their lack might cause.
 *
 * SEE ALSO:
 *      
 *
 * AMENDED:
 *      91/11/25  13:33:12  (sjg)
 *
 * RELEASED:
 *      91/11/25  13:33:17  v1.3
 *
 * SCCSID:
 *      @(#)stdh.h  1.3  91/11/25  13:33:12  (sjg)
 *
 */

#ifndef ARGS
# ifdef __STDC__
#   define ARGS(args) args
# else
#   define ARGS(args) ()
#   ifdef VOID
#     define void VOID
#   endif
#   define const
#   define volatile
# endif
#endif

#include <stdio.h>
/* if we have std headers then include them here
 * otherwise make allowances
 */
#ifndef NOSTDHDRS
# include <stddef.h>
# include <stdlib.h>
# include <string.h>
# include <sys/types.h>
#else
# ifdef HAVE_SYS_STDTYPES
#   include <sys/stdtypes.h>
# else
#   include <sys/types.h>
/* just in case they have sys/stdtypes and don't know it
 */
#   ifndef	__sys_stdtypes_h
#define _PID_T
#define _CLOCK_T
typedef int pid_t;
typedef long clock_t;
#   endif
# endif
# ifdef _SYSV
#   include <string.h>
# else
#   include <strings.h>
#   define strchr index
#   define strrchr rindex
# endif
/* just a useful subset of what stdlib.h would have
 */
extern char * getenv  ARGS((const char *));
extern void * malloc  ARGS((size_t));
extern int    free    ARGS((void *));
extern int    exit    ARGS((int));

/* these _should_ match ANSI */
extern char * strstr  ARGS((const char *, const char *));
extern void * memmove ARGS((void *, const void *, size_t));
extern void * memcpy  ARGS((void *, const void *, size_t));
#endif /* NOSTDHDRS */
  

#ifndef offsetof
#define	offsetof(type,id) ((size_t)&((type*)NULL)->id)
#endif

