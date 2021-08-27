/* work around multiple typedefs in stddef.h and sys/types.h */
/* $Id: types.h,v 1.2 1992/04/25 08:19:26 sjg Exp $ */

#include <stddef.h>		/* defines size_t and ptrdiff_t */
#include <time.h>		/* defines time_t and clock_t */

/* "inhibit" the typedefs in sys/types.h */
#define size_t _size_t
#define	time_t _time_t
#define	clock_t _clock_t
//#include "/./usr/include/sys/types.h"
#undef	size_t
#undef	time_t
#undef	clock_t

