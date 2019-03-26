/* clock() */

#include <time.h>

#if _BSD

#include <sys/time.h>
#include <sys/resource.h>

clock_t
clock()
{
	struct timeval tv;
	struct rusage ru;

	getrusage(RUSAGE_SELF, &ru);
	tv.tv_sec = ru.ru_utime.tv_sec + ru.ru_stime.tv_sec;
	tv.tv_usec = ru.ru_utime.tv_usec + ru.ru_stime.tv_usec;
	return tv.tv_sec*CLK_TCK + (long)tv.tv_usec*CLK_TCK/1000000;
}

#endif

#if _V7 || _SYSV

#include <sys/times.h>

clock_t
clock()
{
	struct tms tms;

	(void) times(&tms);
	return tms.tms_utime + tms.tms_stime;
}

#endif

#if _ST

#include <osbind.h>

clock_t
clock()
{
	long save;
	clock_t c;

	/* access the ST's 200 HZ system clock in protected memory */
	save = Super(0L);
	c = *((long *)0x04BA);
	(void)Super(save);
	return c;
}

#endif

