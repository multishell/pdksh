/*
 * Miscellaneous functions
 */

#if !defined(lint) && !defined(no_RCSids)
static char *RCSid = "$Id: misc.c,v 1.2 1994/05/19 18:32:40 michael Exp michael $";
#endif

#include "sh.h"
#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif
#include "expand.h"

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

struct option options[] = {
	/* Special cases (see parse_args()): -A, -c, -o, -s */
	/* options are sorted by their longnames */
	{ "allexport",	'a',		OF_ANY },
#ifdef BRACEEXPAND
	{ "braceexpand",  0,		OF_ANY }, /* non-standard */
#endif
	{ "bgnice",	  0,		OF_ANY }, /* not done */
	{ "",	 	'c',		     0 }, /* -c on command line */
#ifdef VI
	{ "vi-tabcomplete",  0, 	OF_ANY },
#endif
#ifdef EMACS
	{ "emacs",	  0,		OF_ANY },
#endif
	{ "errexit",	'e',		OF_ANY },
#ifdef EMACS
	{ "gmacs",	  0,		OF_ANY }, /* not done */
#endif
	{ "ignoreeof",	  0,		OF_ANY },
	{ "interactive",'i',	    OF_CMDLINE },
	{ "keyword",	'k',		OF_ANY },
	{ "markdirs",	'X',		OF_ANY }, /* not done */
#ifdef JOBS
	{ "monitor",	'm',		OF_ANY },
#else /* JOBS */
	{ "",		'm',		     0 }, /* so FMONITOR not ifdef'd */
#endif /* JOBS */
	{ "noclobber",	'C',		OF_ANY },
	{ "noexec",	'n',		OF_ANY },
	{ "noglob",	'f',		OF_ANY },
	{ "nolog",	  0,		OF_ANY }, /* not done */
#ifdef	JOBS
	{ "notify",	'b',		OF_ANY },
#endif	/* JOBS */
	{ "nounset",	'u',		OF_ANY },
	{ "posix",	  0,		OF_ANY },
	{ "privileged",	'p',		OF_ANY }, /* not done */
	{ "restricted",	'r',	    OF_CMDLINE }, /* not done */
	{ "stdin",	's',	    OF_CMDLINE }, /* pseudo non-standard */
	{ "trackall",	'h',		OF_ANY },
	{ "verbose",	'v',		OF_ANY },
#ifdef VI
	{ "vi",		  0,		OF_ANY },
	{ "viraw",	  0,		OF_ANY }, /* ignored */
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

static void
printoptions(verbose)
	int verbose;
{
	int i;

	if (verbose) {
		/* verbose (at&t) version */
		shprintf("Current option settings\n");
		for (i = 0; options[i].name; i++)
			if (options[i].name[0])
				shprintf("%-16s %s\n", options[i].name,
					Flag(i) ? "on" : "off");
	} else {
		/* short version */
		for (i = 0; options[i].name; i++)
			if (Flag(i) && options[i].name[0])
				shprintf("%s ", options[i].name);
		shprintf("\n");
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
	}
#endif /* EDIT */
	/* Turning off -p? */
	if (f == FPRIVILEGED && oldval && !newval) {
		setuid(getuid());
		setgid(getgid());
	}
}

/* parse command line & set command arguments.  returns the index of
 * non-option arguments
 */
int
parse_args(argv, what, cargp, setargsp)
	char **argv;
	int	what;		/* OF_CMDLINE or OF_SET */
	char	**cargp;
	int	*setargsp;
{
	static char cmd_opts[NELEM(options) + 5]; /* c:o:\0 */
	static char set_opts[NELEM(options) + 6]; /* A:o;s\0 */
	char *opts;
	Getopt go;
	int i, optc, set, sortargs = 0;

	/* First call?  Build option strings... */
	if (cmd_opts[0] == '\0') {
		char *p;

		/* c is also in options[], but it needs a trailing : */
		strcpy(cmd_opts, "c:o:"); /* see cmd_opts[] declaration */
		p = cmd_opts + strlen(cmd_opts);
		for (i = 0; options[i].name; i++)
			if (options[i].c && (options[i].flags & OF_CMDLINE))
				*p++ = options[i].c;
		*p = '\0';

		strcpy(set_opts, "A:o;s"); /* see set_opts[] declaration */
		p = set_opts + strlen(set_opts);
		for (i = 0; options[i].name; i++)
			if (options[i].c && (options[i].flags & OF_CMDLINE))
				*p++ = options[i].c;
		*p = '\0';
	}

	opts = (what == OF_CMDLINE) ? cmd_opts : set_opts;
	ksh_getopt_reset(&go, GF_ERROR|GF_PLUSOPT);
	while ((optc = ksh_getopt(argv, &go, opts)) != EOF) {
		set = (go.info & GI_PLUS) ? 0 : 1;
		switch (optc) {
		case 'A':
			if (!*go.optarg || *skip_varname(go.optarg, FALSE))
				errorf("%s: is not an identifier\n", go.optarg);
			set_array(go.optarg, set, argv + go.optind);
			for (; argv[go.optind]; go.optind++)
				;
			go.info |= GI_DONE;	/* kludge to get out of loop */
			break;

		case 'c':
			Flag(FCOMMAND) = 1;
			*cargp = go.optarg;
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
			/* temporary hack to ease transition to braceexpand */
			if (strcmp(go.optarg, "alternations") == 0) {
				i = (int) FBRACEEXPAND;
				/*
				shellf("set: warning: use `%s' instead of `%s'\n",
					options[i].name, go.optarg);
				*/
			}
			if (i >= 0 && (options[i].flags & what))
				change_flag((enum sh_flag) i, what, set);
			else
				errorf("%s: %s: bad option\n",
					argv[0], go.optarg);
			break;

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
			if (!options[i].name)
				errorf("%s: parse_args: internal error (%c)\n",
					argv[0], optc);
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
		*setargsp = (go.info & GI_MINUSMINUS) || argv[go.optind];

	if (sortargs) {
		for (i = go.optind; argv[i]; i++)
			;
		qsortp((void **) &argv[go.optind], (size_t) (i - go.optind),
			xstrcmp);
	}

	return go.optind;
}

/* atoi with error detection */
int
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

/* like getn_ but uses shellf instead of errorf */
int
getn_(as, who)
	char *as;
	char *who;
{
	register char *s;
	register int n;

	s = as;
	if (*s == '-')
		s++;
	for (n = 0; digit(*s); s++)
		n = (n*10) + (*s-'0');
	if (*s) {
		shellf("%s: %s: bad number\n", who, as);
		shf_flush(shl_out);
		n = 1; /* ensure non-zero */
	}
	return (*as == '-') ? -n : n;
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
 * a call to errorf().
 *
 * Non-standard features:
 *	- ';' is like ':' in options, except the argument is optional
 *	  (if it isn't present, optarg is set to 0).
 *	  Used for 'set -o'.
 *	- ',' is like ':' in options, except the argument always immediately
 *	  follows the option character (optarg is set to the null string if
 *	  the option is missing).
 *	  Used for 'read -u2' and 'print -u2'.
 *	- '#' is like ':' in options, expect that the argument is optional
 *	  and must start with a digit.  If the argument doesn't start with a
 *	  digit, it is assumed to be missing and normal option processing
 *	  continues (optarg is set to 0 if the option is missing).
 *	  Used for 'typeset -LZ4'.
 *	- accepts +c as well as -c IF the GF_PLUSOPT flag is present.  If an
 *	  option starting with + is accepted, the GI_PLUS flag will be set
 *	  in go->info.
 */
int
ksh_getopt(argv, go, options)
	char **argv;
	Getopt *go;
	char *options;
{
	char c;
	char *o;

	/* protect against multiple calls which could cause optind or p
	 * to go out of range.  Also prevents skipping --.
	 */
	if (go->info & GI_DONE)
		return EOF;
	if (go->p == 0 || (c = argv[go->optind - 1][go->p]) == '\0') {
		go->p = 1;
		go->info &= ~GI_PLUS;
		if (argv[go->optind] == (char *) 0
		    || (argv[go->optind][0] != '-'
			&& (!(go->flags & GF_PLUSOPT)
			   || argv[go->optind][0] != '+'))
		    || (c = argv[go->optind][1]) == '\0')
		{
			go->info |= GI_DONE;
			return EOF;
		}
		go->optind++;
		if (c == '-' && !argv[go->optind - 1][2]) {
			go->info |= GI_DONE | GI_MINUSMINUS;
			return EOF;
		}
		if (argv[go->optind - 1][0] == '+')
			go->info |= GI_PLUS;
	}
	go->p++;
	if (c == '?' || c == ':' || c == ';' || c == ',' || c == '#'
	    || !(o = strchr(options, c)))
	{
		go->info &= ~GI_PLUS;
		go->info |= GI_DONE;
		if (options[0] == ':') {
			go->buf[0] = c;
			go->optarg = go->buf;
		} else {
			shellf("%s: -%c: bad option\n", argv[0], c);
			shf_flush(shl_out);
			if (go->flags & GF_ERROR)
				errorf((char *) 0);
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
			go->info &= ~GI_PLUS;
			go->info |= GI_DONE;
			if (options[0] == ':') {
				go->buf[0] = c;
				go->optarg = go->buf;
				return ':';
			}
			shellf("%s: -`%c' requires argument\n", argv[0], c);
			shf_flush(shl_out);
			if (go->flags & GF_ERROR)
				errorf((char *) 0);
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
	int inquote = 0;

	for (; *s; s++) {
	    if (*s == '\'') {
		shprintf("'\\'" + 1 - inquote);
		inquote = 0;
	    } else {
		if (!inquote) {
		    shprintf("'", shl_stdout);
		    inquote = 1;
		}
		shf_putc(*s, shl_stdout);
	    }
	}
	if (inquote)
	    shprintf("'", shl_stdout);
}
