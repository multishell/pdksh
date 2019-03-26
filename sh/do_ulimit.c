/*
	ulimit -- handle "ulimit" builtin

	Eric Gisin, September 1988
	Adapted to PD KornShell. Removed AT&T code.

	last edit:	06-Jun-1987	D A Gwyn

	This started out as the BRL UNIX System V system call emulation
	for 4.nBSD, and was later extended by Doug Kingston to handle
	the extended 4.nBSD resource limits.  It now includes the code
	that was originally under case SYSULIMIT in source file "xec.c".
*/

#ifndef lint
static char *RCSid = "$Id: do_ulimit.c,v 1.2 1992/04/25 08:33:28 sjg Exp $";
#endif

#include "stdh.h"
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#if defined(_BSD) || defined(_BSD_SYSV)
#include <sys/time.h>
#include <sys/resource.h>
#else
#define	RLIMIT_FSIZE	2
#endif
#include "sh.h"

extern	long ulimit();

int
do_ulimit(a1, a2)
	char	*a1, *a2;
{
	register int	c;
	long		i;
#if defined(_BSD) || defined(_BSD_SYSV)
	struct rlimit	limit;		/* data being gotten/set */
	int		softonly = 0;	/* set => soft limit, clear => hard limit */
	int		factor = 1024;	/* unit scaling (1K or 1) */
#endif
	int	command = RLIMIT_FSIZE;

	if (a1 && (*a1 == '-'))		/* DAG -- Gould added first test */
	{	c = *++a1;		/* DAG */
#if defined(_BSD) || defined(_BSD_SYSV)
		if (c >= 'A' && c <= 'Z')
		{
			++softonly;
			c += 'a' - 'A';	/* DAG -- map to lower-case */
		}
#endif
		switch(c)
		{
#if defined(_BSD) || defined(_BSD_SYSV)
			case 'c':
				command = RLIMIT_CORE;
				break;
			case 'd':
				command = RLIMIT_DATA;
				break;
			case 'm':
				command = RLIMIT_RSS;
				break;
			case 's':
				command = RLIMIT_STACK;
				break;
			case 't':
				factor = 1;
				command = RLIMIT_CPU;
				break;
#endif	/* _BSD || _BSD_SYSV */
			case 'f':
				command = RLIMIT_FSIZE;
#if _BSD_SYSV
				factor = 512;
#endif
				break;
			default:
#if _BSD
				errorf("Usage: %s [-cdmstf] [limit]\n", "ulimit");
#else
				errorf("Usage: %s [-f] [limit]\n", "ulimit");
#endif
		}
		a1 = a2;
	}
	if (a1)
	{
		i = 0;
		while ((c = *a1++) >= '0' && c <= '9')
		{
			i = (i * 10) + (long)(c - '0');
			if (i < 0)
				goto Error;
		}
		if (c || i < 0)
			goto Error;
	}
#if !(defined(_BSD) || defined(_BSD_SYSV))
	else
	{
		i = -1;
		command--;
	}

	if ((i = ulimit(command, i)) < 0L)
		goto Error;

	if (command != RLIMIT_FSIZE)
		shellf("%ld\n", i);
#else					/* DPK -- generalized for 4.nBSD: */
	if (getrlimit(command, &limit))
		goto Error;	/* errno is already set */

	if (a1)
	{
		limit.rlim_cur = i * factor;

		if (!softonly)
			limit.rlim_max = limit.rlim_cur;

		if (setrlimit(command, &limit))
			goto Error;
	}
	else
	{
		i = softonly ? limit.rlim_cur : limit.rlim_max;
#if _BSD			/* DAG -- System V always prints an integer */
		if (i == RLIM_INFINITY)
			shellf("unlimited\n");
		else
#endif
			shellf("%ld\n", i/factor);
	}
#endif	/* _BSD || _BSD_SYSV */
	return 0;

  Error:
	errorf("bad ulimit\n");
}

