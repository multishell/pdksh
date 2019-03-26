#include <sys/times.h>

#ifdef BROKEN_TIMES
extern clock_t	ksh_times ARGS((struct tms *));
#else /* BROKEN_TIMES */
# define ksh_times times
#endif /* BROKEN_TIMES */

#ifdef HAVE_TIMES
extern clock_t	times ARGS((struct tms *));
#endif /* HAVE_TIMES */
