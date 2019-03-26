/*
 * shell buffered IO and formatted output
 */

#if !defined(lint) && !defined(no_RCSids)
static char *RCSid = "$Id: io.c,v 1.2 1994/05/19 18:32:40 michael Exp michael $";
#endif

#include "sh.h"
#include "ksh_stat.h"

/*
 * formatted output functions
 */

/* shellf(...); unwind(LERROR) */
void
#ifdef HAVE_PROTOTYPES
errorf(const char *fmt, ...)
#else
errorf(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	va_list va;

	shl_stdout_ok = 0;	/* debugging: note that stdout not valid */
	exstat = 1;
	SH_VA_START(va, fmt);
	shf_vfprintf(shl_out, fmt, va);
	va_end(va);
	shf_flush(shl_out);
	unwind(LERROR);
}

/* printf to shl_out (stderr) */
void
#ifdef HAVE_PROTOTYPES
shellf(const char *fmt, ...)
#else
shellf(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	va_list va;

	SH_VA_START(va, fmt);
	shf_vfprintf(shl_out, fmt, va);
	va_end(va);
}

/* printf to shl_stdout (stdout) */
void
#ifdef HAVE_PROTOTYPES
shprintf(const char *fmt, ...)
#else
shprintf(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	va_list va;

	if (!shl_stdout_ok)
	    errorf("shprintf: shl_stdout not valid\n");
	SH_VA_START(va, fmt);
	shf_vfprintf(shl_stdout, fmt, va);
	va_end(va);
}

/* test if we can seek backwards fd (returns 0 or SHF_UNBUF) */
int
can_seek(fd)
	int fd;
{
	struct stat statb;

	return fstat(fd, &statb) == 0 && !S_ISREG(statb.st_mode) ?
		SHF_UNBUF : 0;
}

struct shf	shf_iob[3];

void
initio()
{
	shf_fdopen(1, SHF_WR, shl_stdout);	/* force buffer allocation */
	shf_fdopen(2, SHF_WR, shl_out);
	shf_fdopen(2, SHF_WR, shl_spare);	/* force buffer allocation */
}

/*
 * move fd from user space (0<=fd<10) to shell space (fd>=10),
 * set close-on-exec flag.
 */
int
savefd(fd)
	int fd;
{
	int nfd;

	if (fd < FDBASE) {
		nfd = fcntl(fd, F_DUPFD, FDBASE);
		if (nfd < 0)
			if (errno == EBADF)
				return -1;
			else
				errorf("too many files open in shell\n");
		close(fd);
	} else
		nfd = fd;
	fd_clexec(nfd);
	return nfd;
}

void
restfd(fd, ofd)
	int fd, ofd;
{
	if (fd == 2)
		shf_flush(&shf_iob[fd]);
	if (ofd < 0)		/* original fd closed */
		close(fd);
	else {
		dup2(ofd, fd);
		close(ofd);
	}
}

void
openpipe(pv)
	register int *pv;
{
	if (pipe(pv) < 0)
		errorf("can't create pipe - try again\n");
	pv[0] = savefd(pv[0]);
	pv[1] = savefd(pv[1]);
}

void
closepipe(pv)
	register int *pv;
{
	close(pv[0]);
	close(pv[1]);
}

/*
 * temporary files
 */

struct temp *
maketemp(ap)
	Area *ap;
{
	static unsigned int inc = 0;
	struct temp *tp;
	int len;
	char *path, *tmp;

	tmp = tmpdir ? tmpdir : "/tmp";
	/* The 20 + 20 is a paranoid worst case for pid/inc */
	len = strlen(tmp) + 3 + 20 + 20 + 1;
	tp = (struct temp *) alloc(sizeof(struct temp) + len, ap);
	tp->name = path = (char *) &tp[1];
	shf_snprintf(path, len, "%s/sh%05u%02u",
		tmp, (unsigned) getpid(), inc++);
	close(creat(path, 0600));	/* to get safe permissions */
	tp->next = NULL;
	tp->pid = getpid();
	return tp;
}
