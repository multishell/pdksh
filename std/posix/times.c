/* P1003.1 times emulation */
/* $Id: times.c,v 1.2 1992/04/25 08:22:14 sjg Exp $ */

#include <sys/times.h>

#if _BSD

#include <sys/time.h>
#include <sys/resource.h>

static	long base_tv_sec = 0;

clock_t
times(tmsp)
	register struct tms *tmsp;
{
	struct timeval tv;
	struct rusage ru;

	getrusage(RUSAGE_SELF, &ru);
	tmsp->tms_utime = ru.ru_utime.tv_sec*CLK_TCK
		+ (long)ru.ru_utime.tv_usec*CLK_TCK/1000000;
	tmsp->tms_stime = ru.ru_stime.tv_sec*CLK_TCK
		+ (long)ru.ru_stime.tv_usec*CLK_TCK/1000000;
	getrusage(RUSAGE_CHILDREN, &ru);
	tmsp->tms_cutime = ru.ru_utime.tv_sec*CLK_TCK
		+ (long)ru.ru_utime.tv_usec*CLK_TCK/1000000;
	tmsp->tms_cstime = ru.ru_stime.tv_sec*CLK_TCK
		+ (long)ru.ru_stime.tv_usec*CLK_TCK/1000000;

	gettimeofday(&tv, (struct timezone *)NULL);
	if (base_tv_sec == 0)
		base_tv_sec = tv.tv_sec;
	tv.tv_sec -= base_tv_sec; /*  prevent clock_t overflow */
	return tv.tv_sec*CLK_TCK + (long)tv.tv_usec*CLK_TCK/1000000;
}

#endif

#if _V7

clock_t
times(tmsp)
	struct tms *tmsp;
{
	struct timeb tb;

#undef times			/* access real times() */
	times(tmsp);
#define times times_
	ftime(&tb);
	return tb.time*CLK_TCK + (long)tb.millitm*CLK_TCK/1000;
}

#endif

