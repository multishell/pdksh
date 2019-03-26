/*
	ulimit -- handle "ulimit" builtin

	Reworked to use getrusage() and ulimit() at once (as needed on
	some schizophenic systems, eg, HP-UX 9.01), made argument parsing
	conform to at&t ksh, added autoconf support.  Michael Rendell, May, '94

	Eric Gisin, September 1988
	Adapted to PD KornShell. Removed AT&T code.

	last edit:	06-Jun-1987	D A Gwyn

	This started out as the BRL UNIX System V system call emulation
	for 4.nBSD, and was later extended by Doug Kingston to handle
	the extended 4.nBSD resource limits.  It now includes the code
	that was originally under case SYSULIMIT in source file "xec.c".
*/

#if !defined(lint) && !defined(no_RCSids)
static char *RCSid = "$Id: c_ulimit.c,v 1.3 1994/05/31 13:34:34 michael Exp $";
#endif

#include "sh.h"
#ifdef HAVE_SETRLIMIT
# include <sys/time.h>
# include <sys/resource.h>
#endif /* HAVE_SETRLIMIT */
#ifdef HAVE_ULIMIT_H
# include <ulimit.h>
#else /* HAVE_ULIMIT_H */
# ifdef HAVE_ULIMIT
extern	long ulimit ARGS((int, ...));
# endif /* HAVE_ULIMIT */
#endif /* HAVE_ULIMIT_H */


int
c_ulimit(wp)
	char **wp;
{
	static struct limits {
		char	*name;
		enum { RLIMIT, ULIMIT } which;
		int	gcmd;	/* get command */
		int	scmd;	/* set command (or -1, if no set command) */
		int	factor;	/* multiply by to get rlim_{cur,max} values */
		char	option;
	} limits[] = {
		/* Do not use options -H, -S or -a */
#ifdef RLIMIT_CPU
		{ "time(seconds)", RLIMIT, RLIMIT_CPU, RLIMIT_CPU, 1, 't' },
#endif
#ifdef RLIMIT_FSIZE
		{ "file(blocks)", RLIMIT, RLIMIT_FSIZE, RLIMIT_FSIZE, 512, 'f' },
#else /* RLIMIT_FSIZE */
# ifdef UL_GETFSIZE /* x/open */
		{ "file(blocks)", ULIMIT, UL_GETFSIZE, UL_SETFSIZE, 1, 'f' },
# else /* UL_GETFSIZE */
#  ifdef UL_GFILLIM /* svr4/xenix */
		{ "file(blocks)", ULIMIT, UL_GFILLIM, UL_SFILLIM, 1, 'f' },
#  else /* UL_GFILLIM */
		{ "file(blocks)", ULIMIT, 1, 2, 1, 'f' },
#  endif /* UL_GFILLIM */
# endif /* UL_GETFSIZE */
#endif /* RLIMIT_FSIZE */
#ifdef RLIMIT_DATA
		{ "data(kbytes)", RLIMIT, RLIMIT_DATA, RLIMIT_DATA, 1024, 'd' },
#endif
#ifdef RLIMIT_STACK
		{ "stack(kbytes)", RLIMIT, RLIMIT_STACK, RLIMIT_STACK, 1024, 's' },
#endif
#ifdef RLIMIT_RSS
		{ "memory(kbytes)", RLIMIT, RLIMIT_RSS, RLIMIT_RSS, 1024, 'm' },
#endif
#ifdef RLIMIT_CORE
		{ "coredump(blocks)", RLIMIT, RLIMIT_CORE, RLIMIT_CORE, 512, 'c' },
#endif
#ifdef RLIMIT_NOFILE
		{ "nofiles(descriptors)", RLIMIT, RLIMIT_NOFILE, RLIMIT_NOFILE, 1, 'n' },
#else /* RLIMIT_NOFILE */
# ifdef UL_GDESLIM /* svr4/xenix */
		{ "nofiles(descriptors)", ULIMIT, UL_GDESLIM, -1, 1, 'n' },
# endif /* UL_GDESLIM */
#endif /* RLIMIT_NOFILE */
#ifdef RLIMIT_VMEM
		{ "vmemory(kbytes)", RLIMIT, RLIMIT_VMEM, RLIMIT_VMEM, 1024, 'v' },
#else /* RLIMIT_VMEM */
  /* These are not quite right - really should subtract etext or something */
# ifdef UL_GMEMLIM /* svr4/xenix */
		{ "vmemory(maxaddr)", ULIMIT, UL_GMEMLIM, -1, 1, 'v' },
# else /* UL_GMEMLIM */
#  ifdef UL_GETBREAK /* osf/1 */
		{ "vmemory(maxaddr)", ULIMIT, UL_GETBREAK, -1, 1, 'v' },
#  else /* UL_GETBREAK */
#   ifdef UL_GETMAXBRK /* hpux */
		{ "vmemory(maxaddr)", ULIMIT, UL_GETMAXBRK, -1, 1, 'v' },
#   endif /* UL_GETMAXBRK */
#  endif /* UL_GETBREAK */
# endif /* UL_GMEMLIM */
#endif /* RLIMIT_VMEM */
#ifdef RLIMIT_SWAP
		{ "swap(kbytes)", RLIMIT_SWAP, RLIMIT_SWAP, 1024, 'w' },
#endif
		{ (char *) 0 }
	    };
	static char	options[3 + NELEM(limits)];
	register int	c;
	long		UNINITIALIZED(val);
	enum { SOFT = 0x1, HARD = 0x2 }
			how = SOFT | HARD;
	struct limits	*l;
	int		set, all = 0;
	int		optc, what;
#ifdef HAVE_SETRLIMIT
	struct rlimit	limit;
#endif /* HAVE_SETRLIMIT */

	if (!options[0]) {
		/* build options string on first call - yuck */
		char *p = options;

		*p++ = 'H'; *p++ = 'S'; *p++ = 'a';
		for (l = limits; l->name; l++)
			*p++ = l->option;
		*p = '\0';
	}
	what = 'f';
	while ((optc = ksh_getopt(wp, &builtin_opt, options)) != EOF)
		switch (optc) {
		case 'H':
			how = HARD;
			break;
		case 'S':
			how = SOFT;
			break;
		case 'a':
			all = 1;
			break;
		default:
			what = optc;
		}

	for (l = limits; l->name && l->option != what; l++)
		;
	if (!l->name)
		errorf("ulimit: internal error (%c)\n", what);

	wp += builtin_opt.optind;
	set = *wp ? 1 : 0;
	if (set) {
		char *p = *wp;

		if (all || wp[1])
			errorf("ulimit: too many arguments\n");
		val = 0;
		while ((c = *p++) >= '0' && c <= '9')
		{
			val = (val * 10) + (long)(c - '0');
			if (val < 0)
				break;
		}
		if (c)
			errorf("ulimit: bad number\n");
		val *= l->factor;
	}
	if (all) {
		for (l = limits; l->name; l++) {
#ifdef HAVE_SETRLIMIT
			if (l->which == RLIMIT) {
				getrlimit(l->gcmd, &limit);
				if (how & SOFT)
					val = limit.rlim_cur;
				else if (how & HARD)
					val = limit.rlim_max;
			} else 
#endif /* HAVE_SETRLIMIT */
#ifdef HAVE_ULIMIT
			{
				val = ulimit(l->gcmd);
			}
#else /* HAVE_ULIMIT */
				;
#endif /* HAVE_ULIMIT */
			shprintf("%-20s ", l->name);
#ifdef RLIM_INFINITY
			if (val == RLIM_INFINITY)
				shprintf("unlimited\n";);
			else
#endif /* RLIM_INFINITY */
			{
				val /= l->factor;
				shellf("%ld\n", val);
			}
		}
		return 0;
	}
#ifdef HAVE_SETRLIMIT
	if (l->which == RLIMIT) {
		getrlimit(l->gcmd, &limit);
		if (set) {
			if (how & SOFT)
				limit.rlim_cur = val;
			if (how & HARD)
				limit.rlim_max = val;
			if (setrlimit(l->scmd, &limit) < 0)
				errorf("ulimit: bad limit\n");
		} else {
			if (how & SOFT)
				val = limit.rlim_cur;
			else if (how & HARD)
				val = limit.rlim_max;
		}
	} else
#endif /* HAVE_SETRLIMIT */
#ifdef HAVE_ULIMIT
	{
		if (set) {
			if (l->scmd == -1)
				errorf("ulimit: can't change limit\n");
			else if (ulimit(l->scmd, val) < 0)
				errorf("ulimit: bad limit\n");
		} else
			val = ulimit(l->gcmd);
	}
#else /* HAVE_ULIMIT */
		;
#endif /* HAVE_ULIMIT */
	if (!set) {
#ifdef RLIM_INFINITY
		if (val == RLIM_INFINITY)
			shprintf("unlimited\n";);
		else
#endif /* RLIM_INFINITY */
		{
			val /= l->factor;
			shellf("%ld\n", val);
		}
	}
	return 0;
}
