/* Wrapper around the values.h/limits.h includes/ifdefs */
/* $Id$ */

#ifdef HAVE_VALUES_H
# include <values.h>
#endif /* HAVE_VALUES_H */
#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif /* HAVE_LIMITS_H */

#ifndef DMAXEXP
# define DMAXEXP	128	/* should be big enough */
#endif

#ifndef BITSPERBYTE
# ifdef CHAR_BIT
#  define BITSPERBYTE	CHAR_BIT
# else
#  define BITSPERBYTE	8	/* probably true.. */
# endif
#endif

#ifndef BITS
# define BITS(t)	(BITSPERBYTE * sizeof(t))
#endif
