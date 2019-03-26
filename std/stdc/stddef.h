/* ANSI common definitions */

/* $Id: stddef.h,v 1.2 1992/04/25 08:19:26 sjg Exp $ */

#ifndef NULL
#if __STDC__
#define	NULL	(void*)0
#else
#define	NULL	0
#endif
#endif

#ifndef _STDDEF_H
#define	_STDDEF_H

/* doesn't really belong here, but the library function need it */
#ifndef ARGS
# ifdef  __STDC__
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

#ifdef HAVE_SYS_STDTYPES
# include <sys/stdtypes.h>
#else
typedef unsigned size_t;		/* may need long */
typedef int ptrdiff_t;
#endif /* HAVE_SYS_STDTYPES */
#define	offsetof(type,id) ((size_t)&((type*)NULL)->id)

extern	int errno;		/* really belongs in <errno.h> */

#endif

