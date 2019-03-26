/* time, time/date conversion */
/* $Id: time.h,v 1.2 1992/04/25 08:19:26 sjg Exp $ */

#if ! _TIME_H
#define	_TIME_H 1

#include <stddef.h>		/* need size_t */

#ifndef HAVE_SYS_STDTYPES
#ifndef _TIME_T
typedef long time_t;
#endif
typedef long clock_t;		/* seconds/CLK_TCK */
#endif

#if _V7 || _SYSV
#define	CLK_TCK	60		/* todo: get from <sys/param.h> */
#endif

#if _BSD
#define	CLK_TCK	100
#endif

#if _ST
#define	CLK_TCK	200		/* ST system clock */
#endif

struct tm {
	int	tm_sec, tm_min, tm_hour;
	int	tm_mday, tm_mon, tm_year, tm_wday, tm_yday;
	int	tm_isdst;
	long	tm_gmtoff;	/* BSD */
	char   *tm_zone;	/* BSD */
};

clock_t	clock ARGS((void));
time_t	time ARGS((time_t *tp));
#define	difftime(t1, t2)	(double)((t2)-(t1))
time_t	mktime ARGS((struct tm *tmp));
char   *asctime ARGS((const struct tm *tmp));
char   *ctime ARGS((const time_t *tp));
struct tm *gmtime ARGS((const time_t *tp));
struct tm *localtime ARGS((const time_t *tp));
size_t	strftime ARGS((char *buf, size_t len, const char *fmt, const struct tm *tmp));

#endif

