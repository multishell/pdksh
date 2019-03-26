/*
 * Miscellaneous functions
 */

#include "sh.h"
#include <ctype.h>	/* for FILECHCONV */
#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif

#ifndef UCHAR_MAX
# define UCHAR_MAX	0xFF
#endif

char ctypes [UCHAR_MAX+1];	/* type bits for unsigned char */

static	char	*cclass ARGS((char *p, int sub));

/*
 * Fast character classes
 */
void
setctypes(s, t)
	register const char *s;
	register int t;
{
	register int i;

	if ((t&C_IFS)) {
		for (i = 0; i < UCHAR_MAX+1; i++)
			ctypes[i] &= ~C_IFS;
		ctypes[0] |= C_IFS; /* include \0 in C_IFS */
	}
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
	setctypes(" \t\n|&;<>()", C_LEX1); /* \0 added automatically */
	setctypes("*@#!$-?", C_VAR1);
	setctypes(" \t\n", C_IFSWS);
	setctypes("=-+?", C_SUBOP1);
	setctypes("#%", C_SUBOP2);
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
	register const char *s;
	Area *ap;
{
	return s ? strcpy((char*) alloc((size_t)strlen(s)+1, ap), s) : NULL;
}

/* Allocate a string of size n+1 and copy upto n characters from the possibly
 * null terminated string s into it.  Always returns a null terminated string
 * (unless n < 0).
 */
char *
strnsave(s, n, ap)
	register const char *s;
	int n;
	Area *ap;
{
	char *ns;

	if (n < 0)
		return 0;
	ns = alloc(n + 1, ap);
	ns[0] = '\0';
	return strncat(ns, s, n);
}

/* called from expand.h:XcheckN() to grow buffer */
char *
Xcheck_grow_(xsp, xp, more)
	XString *xsp;
	char *xp;
	int more;
{
	char *old_beg = xsp->beg;

	xsp->len += more > xsp->len ? more : xsp->len;
	xsp->beg = aresize(xsp->beg, xsp->len + 8, xsp->areap);
	xsp->end = xsp->beg + xsp->len;
	return xsp->beg + (xp - old_beg);
}

struct option options[] = {
	/* Special cases (see parse_args()): -A, -o, -s.
	 * Options are sorted by their longnames - the order of these
	 * entries MUST match the order of sh_flag F* enumerations in sh.h.
	 */
	{ "allexport",	'a',		OF_ANY },
#ifdef BRACEEXPAND
	{ "braceexpand",  0,		OF_ANY }, /* non-standard */
#endif
	{ "bgnice",	  0,		OF_ANY },
	{ null,	 	'c',	    OF_CMDLINE },
#ifdef EMACS
	{ "emacs",	  0,		OF_ANY },
#endif
	{ "errexit",	'e',		OF_ANY },
#ifdef EMACS
	{ "gmacs",	  0,		OF_ANY },
#endif
	{ "ignoreeof",	  0,		OF_ANY },
	{ "interactive",'i',	    OF_CMDLINE },
	{ "keyword",	'k',		OF_ANY },
	{ "login",	'l',	    OF_CMDLINE },
	{ "markdirs",	'X',		OF_ANY },
#ifdef JOBS
	{ "monitor",	'm',		OF_ANY },
#else /* JOBS */
	{ null,		'm',		     0 }, /* so FMONITOR not ifdef'd */
#endif /* JOBS */
	{ "noclobber",	'C',		OF_ANY },
	{ "noexec",	'n',		OF_ANY },
	{ "noglob",	'f',		OF_ANY },
	{ "nohup",	  0,		OF_ANY },
	{ "nolog",	  0,		OF_ANY }, /* no effect */
#ifdef	JOBS
	{ "notify",	'b',		OF_ANY },
#endif	/* JOBS */
	{ "nounset",	'u',		OF_ANY },
	{ "physical",	  0,		OF_ANY }, /* non-standard */
	{ "posix",	  0,		OF_ANY }, /* non-standard */
	{ "privileged",	'p',		OF_ANY },
	{ "restricted",	'r',	    OF_CMDLINE },
	{ "stdin",	's',	    OF_CMDLINE }, /* pseudo non-standard */
	{ "trackall",	'h',		OF_ANY },
	{ "verbose",	'v',		OF_ANY },
#ifdef VI
	{ "vi",		  0,		OF_ANY },
	{ "viraw",	  0,		OF_ANY }, /* no effect */
	{ "vi-show8",	  0,		OF_ANY }, /* non-standard */
	{ "vi-tabcomplete",  0, 	OF_ANY }, /* non-standard */
#endif
	{ "xtrace",	'x',		OF_ANY },
	{ NULL,		  0,		     0 }
};

/*
 * translate -o option into F* constant (also used for test -o option)
 */
int
option(n)
	const char *n;
{
	int i;

	for (i = 0; options[i].name; i++)
		if (strcmp(options[i].name, n) == 0)
			return i;

	return -1;
}

struct options_info {
	int opt_width;
	struct {
		char	*name;
		int	flag;
	} opts[NELEM(options)];
};

/* format a single select menu item */
static char *
options_fmt_entry(arg, i, buf, buflen)
	void *arg;
	int i;
	char *buf;
	int buflen;
{
	struct options_info *oi = (struct options_info *) arg;

	shf_snprintf(buf, buflen, "%-*s %s",
		oi->opt_width, oi->opts[i].name,
		Flag(oi->opts[i].flag) ? "on" : "off");
	return buf;
}

static void
printoptions(verbose)
	int verbose;
{
	int i;

	if (verbose) {
		struct options_info oi;
		int n, len;

		/* verbose version */
		shprintf("Current option settings\n");

		for (i = n = oi.opt_width = 0; options[i].name; i++)
			if (options[i].name[0]) {
				len = strlen(options[i].name);
				oi.opts[n].name = options[i].name;
				oi.opts[n++].flag = i;
				if (len > oi.opt_width)
					oi.opt_width = len;
			}
		print_columns(shl_stdout, n, options_fmt_entry, &oi,
			      oi.opt_width + 5);
	} else {
		/* short version */
		for (i = 0; options[i].name; i++)
			if (Flag(i) && options[i].name[0])
				shprintf("%s ", options[i].name);
		shprintf(newline);
	}
}

char *
getoptions()
{
	int i;
	char m[FNFLAGS + 1];
	register char *cp = m;

	for (i = 0; options[i].name; i++)
		if (options[i].c && Flag(i))
			*cp++ = options[i].c;
	*cp = 0;
	return strsave(m, ATEMP);
}

/* change a Flag(*) value; takes care of special actions */
void
change_flag(f, what, newval)
	enum sh_flag f;	/* flag to change */
	int what;	/* what is changing the flag (command line vs set) */
	int newval;
{
	int oldval;

	oldval = Flag(f);
	Flag(f) = newval;
#ifdef JOBS
	if (f == FMONITOR) {
		if (what != OF_CMDLINE && newval != oldval)
			j_change();
	} else
#endif /* JOBS */
#ifdef EDIT
	if (0
# ifdef VI
	    || f == FVI
# endif /* VI */
# ifdef EMACS
	    || f == FEMACS || f == FGMACS
# endif /* EMACS */
	   )
	{
		if (newval) {
# ifdef VI
			Flag(FVI) = 0;
# endif /* VI */
# ifdef EMACS
			Flag(FEMACS) = Flag(FGMACS) = 0;
# endif /* EMACS */
			Flag(f) = newval;
		}
	} else
#endif /* EDIT */
	/* Turning off -p? */
	if (f == FPRIVILEGED && oldval && !newval) {
#ifdef OS2
		;
#else /* OS2 */
		setuid(getuid());
		setgid(getgid());
#endif /* OS2 */
	} else if (f == FPOSIX && newval) {
#ifdef BRACEEXPAND
		Flag(FBRACEEXPAND) = 0
#endif /* BRACEEXPAND */
		;
	}
}

/* parse command line & set command arguments.  returns the index of
 * non-option arguments, -1 if there is an error.
 */
int
parse_args(argv, what, setargsp)
	char **argv;
	int	what;		/* OF_CMDLINE or OF_SET */
	int	*setargsp;
{
	static char cmd_opts[NELEM(options) + 3]; /* o:\0 */
	static char set_opts[NELEM(options) + 5]; /* Ao;s\0 */
	char *opts;
	char *array;
	Getopt go;
	int i, optc, set, sortargs = 0, arrayset = 0;

	/* First call?  Build option strings... */
	if (cmd_opts[0] == '\0') {
		char *p;

		/* c is also in options[], but it needs a trailing : */
		strcpy(cmd_opts, "o:"); /* see cmd_opts[] declaration */
		p = cmd_opts + strlen(cmd_opts);
		for (i = 0; options[i].name; i++)
			if (options[i].c && (options[i].flags & OF_CMDLINE))
				*p++ = options[i].c;
		*p = '\0';

		strcpy(set_opts, "Ao;s"); /* see set_opts[] declaration */
		p = set_opts + strlen(set_opts);
		for (i = 0; options[i].name; i++)
			if (options[i].c && (options[i].flags & OF_SET))
				*p++ = options[i].c;
		*p = '\0';
	}

	if (what == OF_CMDLINE) {
		char *p;
		/* Set FLOGIN before parsing options so user can clear
		 * flag using +l.
		 */
		Flag(FLOGIN) = (argv[0][0] == '-'
				|| ((p = strrchr_dirsep(argv[0]))
				     && *++p == '-'));
		opts = cmd_opts;
	} else
		opts = set_opts;
	ksh_getopt_reset(&go, GF_ERROR|GF_PLUSOPT);
	while ((optc = ksh_getopt(argv, &go, opts)) != EOF) {
		set = (go.info & GI_PLUS) ? 0 : 1;
		switch (optc) {
		  case 'A':
			arrayset = set ? 1 : -1;
			break;

		  case 'o':
			if (go.optarg == (char *) 0) {
				/* lone -o: print options
				 *
				 * Note that on the command line, -o requires
				 * an option (ie, can't get here if what is
				 * OF_CMDLINE).
				 */
				printoptions(set);
				break;
			}
			i = option(go.optarg);
			if (i >= 0 && (options[i].flags & what))
				change_flag((enum sh_flag) i, what, set);
			else {
				bi_errorf("%s: bad option", go.optarg);
				return -1;
			}
			break;

		  case '?':
			return -1;

		  default:
			/* -s: sort positional params (at&t ksh stupidity) */
			if (what == OF_SET && optc == 's') {
				sortargs = 1;
				break;
			}
			for (i = 0; options[i].name; i++)
				if (optc == options[i].c
				    && (what & options[i].flags))
				{
					change_flag((enum sh_flag) i, what,
						    set);
					break;
				}
			if (!options[i].name) {
				internal_errorf(1, "parse_args: `%c'", optc);
				return -1; /* not reached */
			}
		}
	}
	if (!(go.info & GI_MINUSMINUS) && argv[go.optind]
	    && (argv[go.optind][0] == '-' || argv[go.optind][0] == '+')
	    && argv[go.optind][1] == '\0')
	{
		/* lone - clears -v and -x flags */
		if (argv[go.optind][0] == '-' && !Flag(FPOSIX))
			Flag(FVERBOSE) = Flag(FXTRACE) = 0;
		/* set skips lone - or + option */
		go.optind++;
	}
	if (setargsp)
		/* -- means set $#/$* even if there are no arguments */
		*setargsp = !arrayset && ((go.info & GI_MINUSMINUS)
					  || argv[go.optind]);

	if (arrayset) {
		array = argv[go.optind++];
		if (!array) {
			bi_errorf("-A: missing array name\n");
			return -1;
		}
		if (!*array || *skip_varname(array, FALSE)) {
			bi_errorf("%s: is not an identifier\n", array);
			return -1;
		}
	} else
		array = (char *) 0;	/* keep gcc happy */
	if (sortargs) {
		for (i = go.optind; argv[i]; i++)
			;
		qsortp((void **) &argv[go.optind], (size_t) (i - go.optind),
			xstrcmp);
	}
	if (arrayset) {
		set_array(array, arrayset, argv + go.optind);
		for (; argv[go.optind]; go.optind++)
			;
	}

	return go.optind;
}

/* parse a decimal number: returns 0 if string isn't a number, 1 otherwise */
int
getn(as, ai)
	char *as;
	int *ai;
{
	register char *s;
	register int n;
	int sawdigit = 0;

	s = as;
	if (*s == '-' || *s == '+')
		s++;
	for (n = 0; digit(*s); s++, sawdigit = 1)
		n = n * 10 + (*s - '0');
	*ai = (*as == '-') ? -n : n;
	if (*s || !sawdigit)
		return 0;
	return 1;
}

/* getn() that prints error */
int
bi_getn(as, ai)
	char *as;
	int *ai;
{
	int rv = getn(as, ai);

	if (!rv)
		bi_errorf("%s: bad number", as);
	return rv;
}

/* -------- gmatch.c -------- */

/*
 * int gmatch(string, pattern)
 * char *string, *pattern;
 *
 * Match a pattern as in sh(1).
 * pattern character are prefixed with MAGIC by expand.
 */

int
gmatch(s, p, isfile)
	register char *s, *p;
	int isfile;
{
	register unsigned int sc, pc;

	if (s == NULL || p == NULL)
		return 0;
	while ((pc = *p++) != 0) {
		sc = *s++;
		if (isfile) {
			sc = FILECHCONV(sc);
			pc = FILECHCONV(pc);
		}
		if (pc == MAGIC)
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
					if (*p == '\0' || gmatch(s, p, isfile))
						return (1);
				} while (*s++ != '\0');
				return (0);

#if 0
			  /* [!+*?@](pattern|pattern|..) */
			  case '!': /* matches none of the patterns */
			  case '+': /* matches one or more times */
			  case '*': /* matches zero or more times */
			  case '?': /* matches zero or once */
			  case '@': /* matches once */
				/* fall through */
#endif /* 0 */

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
	char *orig_p = p;

	if ((not = (*p == MAGIC && *++p == NOT)))
		p++;
	do {
		if (*p == MAGIC)
			p++;
		if (*p == '\0')
			/* No closing ] - act as if the opening [ was quoted */
			return sub == '[' ? orig_p : NULL;
		c = *p++;
		if (p[0] == MAGIC && p[1] == '-'
		    && (p[2] != MAGIC || p[3] != ']'))
		{
			p += 2; /* MAGIC- */
			if (*p == MAGIC)
				p++;
			d = *p++;
			/* POSIX says this is an invalid expression */
			if (c > d)
				return NULL;
		} else
			d = c;
		if (c == sub || (c <= sub && sub <= d))
			found = 1;
	} while (!(p[0] == MAGIC && p[1] == ']'));

	return (found != not) ? p+2 : NULL;
}

/* -------- qsort.c -------- */

/*
 * quick sort of array of generic pointers to objects.
 */
static void qsort1 ARGS((void **base, void **lim, int (*f)(void *, void *)));

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

static void
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

/* Initialize a Getopt structure */
void
ksh_getopt_reset(go, flags)
	Getopt *go;
	int flags;
{
	go->optind = 1;
	go->optarg = (char *) 0;
	go->p = 0;
	go->flags = flags;
	go->info = 0;
	go->buf[1] = '\0';
}


/* getopt() used for shell built-in commands, the getopts command, and
 * command line options.
 * A leading ':' in options means don't print errors, instead return '?'
 * or ':' and set go->optarg to the offending option character.
 * If GF_ERROR is set (and option doesn't start with :), errors result in
 * a call to bi_errorf().
 *
 * Non-standard features:
 *	- ';' is like ':' in options, except the argument is optional
 *	  (if it isn't present, optarg is set to 0).
 *	  Used for 'set -o'.
 *	- ',' is like ':' in options, except the argument always immediately
 *	  follows the option character (optarg is set to the null string if
 *	  the option is missing).
 *	  Used for 'read -u2', 'print -u2' and fc -40.
 *	- '#' is like ':' in options, expect that the argument is optional
 *	  and must start with a digit.  If the argument doesn't start with a
 *	  digit, it is assumed to be missing and normal option processing
 *	  continues (optarg is set to 0 if the option is missing).
 *	  Used for 'typeset -LZ4'.
 *	- accepts +c as well as -c IF the GF_PLUSOPT flag is present.  If an
 *	  option starting with + is accepted, the GI_PLUS flag will be set
 *	  in go->info.  Once a - or + has been seen, all other options must
 *	  start with the same character.
 */
int
ksh_getopt(argv, go, options)
	char **argv;
	Getopt *go;
	char *options;
{
	char c;
	char *o;

	if (go->p == 0 || (c = argv[go->optind - 1][go->p]) == '\0') {
		char *arg = argv[go->optind], flag = arg ? *arg : '\0';

		go->p = 1;
		if (flag == '-' && arg[1] == '-' && arg[2] == '\0') {
			go->optind++;
			go->p = 0;
			go->info |= GI_MINUSMINUS;
			return EOF;
		}
		if (arg == (char *) 0
		    || ((flag != '-' || (go->info & GI_PLUS))
			&& (!(go->flags & GF_PLUSOPT) || (go->info & GI_MINUS)
			    || flag != '+'))
		    || (c = arg[1]) == '\0')
		{
			go->p = 0;
			return EOF;
		}
		go->optind++;
		go->info |= flag == '-' ? GI_MINUS : GI_PLUS;
	}
	go->p++;
	if (c == '?' || c == ':' || c == ';' || c == ',' || c == '#'
	    || !(o = strchr(options, c)))
	{
		if (options[0] == ':') {
			go->buf[0] = c;
			go->optarg = go->buf;
		} else {
			warningf(TRUE, "%s%s-%c: bad option",
				(go->flags & GI_NONAME) ? "" : argv[0],
				(go->flags & GI_NONAME) ? "" : ": ", c);
			if (go->flags & GF_ERROR)
				bi_errorf((char *) 0);
		}
		return '?';
	}
	/* : means argument must be present, may be part of option argument
	 *   or the next argument
	 * ; same as : but argument may be missing
	 * , means argument is part of option argument, and may be null.
	 */
	if (*++o == ':' || *o == ';') {
		if (argv[go->optind - 1][go->p])
			go->optarg = argv[go->optind - 1] + go->p;
		else if (argv[go->optind])
			go->optarg = argv[go->optind++];
		else if (*o == ';')
			go->optarg = (char *) 0;
		else {
			if (options[0] == ':') {
				go->buf[0] = c;
				go->optarg = go->buf;
				return ':';
			}
			warningf(TRUE, "%s%s-`%c' requires argument",
				(go->flags & GI_NONAME) ? "" : argv[0],
				(go->flags & GI_NONAME) ? "" : ": ", c);
			if (go->flags & GF_ERROR)
				bi_errorf((char *) 0);
			return '?';
		}
		go->p = 0;
	} else if (*o == ',') {
		/* argument is attatched to option character, even if null */
		go->optarg = argv[go->optind - 1] + go->p;
		go->p = 0;
	} else if (*o == '#') {
		/* argument is optional and may be attatched or unattatched
		 * but must start with a digit.  optarg is set to 0 if the
		 * argument is missing.
		 */
		if (argv[go->optind - 1][go->p]) {
			if (digit(argv[go->optind - 1][go->p])) {
				go->optarg = argv[go->optind - 1] + go->p;
				go->p = 0;
			} else
				go->optarg = (char *) 0;;
		} else {
			if (argv[go->optind] && digit(argv[go->optind][0])) {
				go->optarg = argv[go->optind++];
				go->p = 0;
			} else
				go->optarg = (char *) 0;;
		}
	}
	return c;
}

/* print variable/alias value using necessary quotes
 * (POSIX says they should be suitable for re-entry...)
 * No trailing newline is printed.
 */
void
print_value_quoted(s)
	char *s;
{
	char *p;
	int inquote = 0;

	/* Test if any quotes are needed */
	for (p = s; *p; p++)
		if (!letnum(*p) && *p != '/')
			break;
	if (!*p) {
		shprintf("%s", s);
		return;
	}
	for (p = s; *p; p++) {
		if (*p == '\'') {
			shprintf("'\\'" + 1 - inquote);
			inquote = 0;
		} else {
			if (!inquote) {
				shprintf("'");
				inquote = 1;
			}
			shf_putc(*p, shl_stdout);
		}
	}
	if (inquote)
		shprintf("'");
}

/* Print things in columns and rows - func() is called to format the ith
 * element
 */
void
print_columns(shf, n, func, arg, max_width)
	struct shf *shf;
	int n;
	char *(*func) ARGS((void *, int, char *, int));
	void *arg;
	int max_width;
{
	char *str = (char *) alloc(max_width + 1, ATEMP);
	int i;
	int r, c;
	int rows, cols;
	int nspace;

	/* max_width + 1 for the space.  Note that no space
	 * is printed after the last column to avoid problems
	 * with terminals that have auto-wrap.
	 */
	cols = x_cols / (max_width + 1);
	if (!cols)
		cols = 1;
	rows = (n + cols - 1) / cols;
	if (n && cols > rows) {
		int tmp = rows;

		rows = cols;
		cols = tmp;
		if (rows > n)
			rows = n;
	}

	nspace = (x_cols - max_width * cols) / cols;
	if (nspace <= 0)
		nspace = 1;
	for (r = 0; r < rows; r++) {
		for (c = 0; c < cols; c++) {
			i = c * rows + r;
			if (i < n) {
				shf_fprintf(shf, "%-*s",
					max_width,
					(*func)(arg, i, str, max_width + 1));
				if (c + 1 < cols)
					shf_fprintf(shf, "%*s", nspace, null);
			}
		}
		shf_putchar('\n', shf);
	}
	afree(str, ATEMP);
}

/* Strip any nul bytes from buf - returns new length (nbytes - # of nuls) */
int
strip_nuls(buf, nbytes)
	char *buf;
	int nbytes;
{
	char *dst;

	/* nbytes check because some systems (older freebsd's) have a buggy
	 * memchr()
	 */
	if (nbytes && (dst = memchr(buf, '\0', nbytes))) {
		char *end = buf + nbytes;
		char *p, *q;

		for (p = dst; p < end; p = q) {
			/* skip a block of nulls */
			while (++p < end && *p == '\0')
				;
			/* find end of non-null block */
			if (!(q = memchr(p, '\0', end - p)))
				q = end;
			memmove(dst, p, q - p);
			dst += q - p;
		}
		*dst = '\0';
		return dst - buf;
	}
	return nbytes;
}

/* Copy at most dsize-1 bytes from src to dst, ensuring dst is null terminated.
 * Returns dst.
 */
char *
str_zcpy(dst, src, dsize)
	char *dst;
	char *src;
	int dsize;
{
	if (dsize > 0) {
		int len = strlen(src);

		if (len >= dsize)
			len = dsize - 1;
		memcpy(dst, src, len);
		dst[len] = '\0';
	}
	return dst;
}

/* Like read(2), but if read fails due to non-blocking flag, resets flag
 * and restarts read.
 */
int
blocking_read(fd, buf, nbytes)
	int fd;
	char *buf;
	int nbytes;
{
	int ret;
	int tried_reset = 0;

	while ((ret = read(fd, buf, nbytes)) < 0) {
		if (!tried_reset && (errno == EAGAIN
#ifdef EWOULDBLOCK
				     || errno == EWOULDBLOCK
#endif /* EWOULDBLOCK */
				    ))
		{
			int oerrno = errno;
			if (reset_nonblock(fd) > 0) {
				tried_reset = 1;
				continue;
			}
			errno = oerrno;
		}
		break;
	}
	return ret;
}

/* Reset the non-blocking flag on the specified file descriptor.
 * Returns -1 if there was an error, 0 if non-blocking wasn't set,
 * 1 if it was.
 */
int
reset_nonblock(fd)
	int fd;
{
	int flags;
	int blocking_flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) < 0)
		return -1;
	/* With luck, the C compiler will reduce this to a constant */
	blocking_flags = 0;
#ifdef O_NONBLOCK
	blocking_flags |= O_NONBLOCK;
#endif /* O_NONBLOCK */
#ifdef O_NDELAY
	blocking_flags |= O_NDELAY;
#else /* O_NDELAY */
# ifndef O_NONBLOCK
	blocking_flags |= FNDELAY; /* hope this exists... */
# endif /* O_NONBLOCK */
#endif /* O_NDELAY */
	if (!(flags & blocking_flags))
		return 0;
	flags &= ~blocking_flags;
	if (fcntl(fd, F_SETFL, flags) < 0)
		return -1;
	return 1;
}


#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif /* HAVE_SYS_PARAM_H */
#ifndef MAXPATHLEN
# define MAXPATHLEN PATH
#endif /* MAXPATHLEN */

/* Like getcwd(), except bsize is ignored if buf is 0 (MAXPATHLEN is used) */
char *
ksh_get_wd(buf, bsize)
	char *buf;
	int bsize;
{
#ifdef HAVE_GETWD
	extern char *getwd ARGS((char *));
	char *b;
	int len;

	if (buf && bsize > MAXPATHLEN)
		b = buf;
	else
		b = alloc(MAXPATHLEN + 1, ATEMP);
	if (!getwd(b)) {
		errno = EACCES;
		if (b != buf)
			afree(b, ATEMP);
		return (char *) 0;
	}
	len = strlen(b) + 1;
	if (!buf)
		b = aresize(b, len, ATEMP);
	else if (buf != b) {
		if (len > bsize) {
			errno = ERANGE;
			return (char *) 0;
		}
		memcpy(buf, b, len);
		afree(b, ATEMP);
		b = buf;
	}

	return b;
#else /* HAVE_GETWD */
	char *b;
	char *ret;

	/* Assume getcwd() available */
	if (!buf) {
		bsize = MAXPATHLEN;
		b = alloc(MAXPATHLEN + 1, ATEMP);
	} else
		b = buf;

	ret = getcwd(b, bsize);

	if (!buf) {
		if (ret)
			ret = aresize(b, strlen(b) + 1, ATEMP);
		else
			afree(b, ATEMP);
	}

	return ret;
#endif /* HAVE_GETWD */
}
