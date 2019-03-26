/*
 * Miscellaneous functions
 */

#ifndef lint
static char *RCSid = "$Id: misc.c,v 1.2 1992/04/25 08:33:28 sjg Exp $";
#endif

#include "stdh.h"
#include <signal.h>
#include <errno.h>
#include <setjmp.h>
#include "sh.h"
#include "expand.h"
#ifndef NOSTDHDRS
# include <limits.h>
#else
# define UCHAR_MAX	0xFF
#endif

char ctypes [UCHAR_MAX+1];	/* type bits for unsigned char */

static char *   cclass      ARGS((char *p, int sub));

/*
 * Fast character classes
 */
void
setctypes(s, t)
	register /* const */ char *s;
	register int t;
{
	register int i;

	if ((t&C_IFS)) {
		for (i = 0; i < UCHAR_MAX+1; i++)
			ctypes[i] &=~ C_IFS;
		ctypes[0] |= C_IFS; /* include \0 in C_IFS */
	}
	ctypes[(unsigned char) *s++] |= t;	/* allow leading \0 in string */
	while (*s != 0)
		ctypes[(unsigned char) *s++] |= t;
}

void
initctypes()
{
	register int c;

	for (c = 'a'; c <= 'z'; c++)
		ctypes[c] |= C_ALPHA;
	for (c = 'A'; c <= 'Z'; c++)
		ctypes[c] |= C_ALPHA;
	ctypes['_'] |= C_ALPHA;
	setctypes("0123456789", C_DIGIT);
	setctypes("\0 \t\n|&;<>()", C_LEX1);
	setctypes("*@#!$-?", C_VAR1);
	setctypes("=-+?#%", C_SUBOP);
}

/* convert unsigned long to base N string */

char *
ulton(n, base)
	register unsigned long n;
	int base;
{
	register char *p;
	static char buf [20];

	p = &buf[sizeof(buf)];
	*--p = '\0';
	do {
		*--p = "0123456789ABCDEF"[n%base];
		n /= base;
	} while (n != 0);
	return p;
}

char *
strsave(s, ap)
	register char *s;
	Area *ap;
{
	return strcpy((char*) alloc((size_t)strlen(s)+1, ap), s);
}

static struct option {
	char *name;
	int flag;
} options[] = {
	{"allexport",	FEXPORT},
	{"bgnice",	FBGNICE},
#if defined(EDIT) && defined(EMACS)
	{"emacs",	FEMACS},
#endif
	{"errexit",	FERREXIT},
	{"hashall",	FHASHALL},
	{"ignoreeof",	FIGNEOF},
	{"interactive",	FTALKING},
	{"keyword",	FKEYWORD},
	{"markdirs",	FMARKDIRS},
	{"monitor",	FMONITOR},
	{"noexec",	FNOEXEC},
	{"noglob",	FNOGLOB},
	{"nounset",	FNOUNSET},
	{"privileged",	FPRIVILEGED},
	{"stdin",	FSTDIN},
	{"trackall",	FHASHALL},
	{"verbose",	FVERBOSE},
#if defined(EDIT) && defined(VI)
	{"vi",		FVI},
#endif
	{"xtrace",	FXTRACE},
	{NULL,		0}
};	

/*
 * translate -o option into F* constant
 */
int
option(n)
	const char *n;
{
	register struct option *op;

	for (op = options; op->name != NULL; op++)
		if (strcmp(op->name, n) == 0)
			return op->flag;
	return 0;
}

char *
getoptions()
{
	register int c;
	char m [26+1];
	register char *cp = m;

	for (c = 'a'; c <= 'z'; c++)
		if (flag[FLAG(c)])
			*cp++ = (char) c;
	*cp = 0;
	return strsave(m, ATEMP);
}

void
printoptions()
{
	register struct option *op;

	for (op = options; op->name != NULL; op++)
		if (flag[op->flag])
			shellf("%s ", op->name);
	shellf("\n");
}
	
/* atoi with error detection */

getn(as)
	char *as;
{
	register char *s;
	register int n;

	s = as;
	if (*s == '-')
		s++;
	for (n = 0; digit(*s); s++)
		n = (n*10) + (*s-'0');
	if (*s)
		errorf("%s: bad number\n", as);
	return (*as == '-') ? -n : n;
}

/*
 * stripped down strerror for kill and exec
 */
char *
strerror(i)
	int i;
{
	switch (i) {
	  case EINVAL:
		return "Invalid argument";
	  case EACCES:
		return "Permission denied";
	  case ESRCH:
		return "No such process";
	  case EPERM:
		return "Not owner";
	  case ENOENT:
		return "No such file or directory";
	  case ENOTDIR:
		return "Not a directory";
	  case ENOEXEC:
		return "Exec format error";
	  case ENOMEM:
		return "Not enough memory";
	  case E2BIG:
		return "Argument list too long";
	  default:
		return "Unknown system error";
	}
}

/* -------- gmatch.c -------- */

/*
 * int gmatch(string, pattern)
 * char *string, *pattern;
 *
 * Match a pattern as in sh(1).
 * pattern character are prefixed with MAGIC by expand.
 */

static	char	*cclass ARGS((char *, int c));

int
gmatch(s, p)
	register char *s, *p;
{
	register int sc, pc;

	if (s == NULL || p == NULL)
		return 0;
	while ((pc = *p++) != 0) {
		sc = *s++;
		if (pc ==  MAGIC)
			switch (*p++) {
			  case '[':
				if (sc == 0 || (p = cclass(p, sc)) == NULL)
					return (0);
				break;

			  case '?':
				if (sc == 0)
					return (0);
				break;

			  case '*':
				s--;
				do {
					if (*p == '\0' || gmatch(s, p))
						return (1);
				} while (*s++ != '\0');
				return (0);

			  default:
				if (sc != p[-1])
					return 0;
				break;
			}
		else
			if (sc != pc)
				return 0;
	}
	return (*s == 0);
}

static char *
cclass(p, sub)
	register char *p;
	register int sub;
{
	register int c, d, not, found = 0;

	if ((not = (*p == MAGIC && *++p == NOT)))
		p++;
	do {
		if (*p == MAGIC)
			p++;
		if (*p == '\0')
			return NULL;
		c = *p;
		if (p[1] == '-' && p[2] != ']') {
			d = p[2];
			p++;
		} else
			d = c;
		if (c == sub || (c <= sub && sub <= d))
			found = 1;
	} while (*++p != ']');

	return (found != not) ? p+1 : NULL;
}

/* -------- qsort.c -------- */

/*
 * quick sort of array of generic pointers to objects.
 */

void
qsortp(base, n, f)
	void **base;		/* base address */
	size_t n;		/* elements */
	int (*f)();		/* compare function */
{
	qsort1(base, base + n, f);
}

#define	swap2(a, b)	{\
	register void *t; t = *(a); *(a) = *(b); *(b) = t;\
}
#define	swap3(a, b, c)	{\
	register void *t; t = *(a); *(a) = *(c); *(c) = *(b); *(b) = t;\
}

qsort1(base, lim, f)
	void **base, **lim;
	int (*f)();
{
	register void **i, **j;
	register void **lptr, **hptr;
	size_t n;
	int c;

  top:
	n = (lim - base) / 2;
	if (n == 0)
		return;
	hptr = lptr = base+n;
	i = base;
	j = lim - 1;

	for (;;) {
		if (i < lptr) {
			if ((c = (*f)(*i, *lptr)) == 0) {
				lptr --;
				swap2(i, lptr);
				continue;
			}
			if (c < 0) {
				i += 1;
				continue;
			}
		}

	  begin:
		if (j > hptr) {
			if ((c = (*f)(*hptr, *j)) == 0) {
				hptr ++;
				swap2(hptr, j);
				goto begin;
			}
			if (c > 0) {
				if (i == lptr) {
					hptr ++;
					swap3(i, hptr, j);
					i = lptr += 1;
					goto begin;
				}
				swap2(i, j);
				j -= 1;
				i += 1;
				continue;
			}
			j -= 1;
			goto begin;
		}

		if (i == lptr) {
			if (lptr-base >= lim-hptr) {
				qsort1(hptr+1, lim, f);
				lim = lptr;
			} else {
				qsort1(base, lptr, f);
				base = hptr+1;
			}
			goto top;
		}

		lptr -= 1;
		swap3(j, lptr, i);
		j = hptr -= 1;
	}
}

int
xstrcmp(p1, p2)
	void *p1, *p2;
{
	return (strcmp((char *)p1, (char *)p2));
}

void
cleanpath(pwd, dir, clean)
	char *pwd, *dir, *clean;
{
	register char  *s, *d, *p;
	char *slash = "/";
	register int inslash = 0;

	d = clean;
	if (*dir != '/') {
		s = pwd;
		while (*d++ = *s++)
			;
		if (d >= clean + 2 && *(d - 2) == '/')
			d--;
		else
			*(d - 1) = '/';
	}

	s = dir;
	while (*s) {
		if ((*d++ = *s++) == '/' && d > clean + 1) {
			if (*(p = d - 2) == '/') {
				--d;
			} else if (*p == '.') {
				if (*--p == '/') {
					d -= 2;
				} else if (*p == '.' && *--p == '/') {
					while (p > clean && *--p != '/')
						;
					d = p + 1;
				}
			}
		}
		if (!*s && !inslash && *(s - 1) != '/') {
			inslash = 1;
			s = slash;
		}
	}

	if (*(d - 1) == '/' && (d - 1) > clean)
		d--;
	*d = '\0';
}
