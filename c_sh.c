/*
 * built-in Bourne commands
 */

#if !defined(lint) && !defined(no_RCSids)
static char *RCSid = "$Id: c_sh.c,v 1.5 1994/05/31 13:34:34 michael Exp $";
#endif

#include "sh.h"
#include "expand.h"
#include "ksh_stat.h" 	/* umask() */
#include "ksh_time.h"
#include "ksh_times.h"

static	char *clocktos ARGS((clock_t t));

/* :, false and true */
int
c_label(wp)
	char **wp;
{
	return wp[0] && wp[0][0] == 'f' ? 1 : 0;
}

int
c_shift(wp)
	register char **wp;
{
	register struct block *l = e->loc;
	register int n;

	n = wp[1] ? evaluate(wp[1]) : 1;
	if (n < 0) {
		errorf("shift: %s: bad number\n", wp[1]);
		return (1);
	}
	if (l->argc < n) {
		errorf("shift: nothing to shift\n");
		return (1);
	}
	l->argv[n] = l->argv[0];
	l->argv += n;
	l->argc -= n;
	return 0;
}

int
c_umask(wp)
	register char **wp;
{
	register int i;
	register char *cp;
	int symbolic = 0;
	int old_umask;
	int optc;

	while ((optc = ksh_getopt(wp, &builtin_opt, "S")) != EOF)
		switch (optc) {
		case 'S':
			symbolic = 1;
			break;
		}
	cp = wp[builtin_opt.optind];
	if (cp == NULL) {
		old_umask = umask(0);
		umask(old_umask);
		if (symbolic) {
			char buf[18];
			int j;

			old_umask = ~old_umask;
			cp = buf;
			for (i = 0; i < 3; i++) {
				*cp++ = "ugo"[i];
				*cp++ = '=';
				for (j = 0; j < 3; j++)
					if (old_umask & (1 << (8 - (3*i + j))))
						*cp++ = "rwx"[j];
				*cp++ = ',';
			}
			cp[-1] = '\0';
			shprintf("%s\n", buf);
		} else
			shprintf("%#3.3o\n", old_umask);
	} else {
		int new_umask;

		if (digit(*cp)) {
			for (new_umask = 0; *cp>='0' && *cp<='7'; cp++)
				new_umask = new_umask*8 + (*cp-'0');
			if (*cp)
				errorf("umask: bad number\n");
		} else {
			/* symbolic format */
			int positions, new_val;
			char op;

			old_umask = umask(0);
			umask(old_umask); /* in case of error */
			old_umask = ~old_umask;
			new_umask = old_umask;
			positions = 0;
			while (*cp) {
				while (*cp && strchr("augo", *cp))
					switch (*cp++) {
					case 'a': positions |= 0111; break;
					case 'u': positions |= 0100; break;
					case 'g': positions |= 0010; break;
					case 'o': positions |= 0001; break;
					}
				if (!positions)
					positions = 0111; /* default is a */
				if (!strchr("=+-", op = *cp))
					break;
				cp++;
				new_val = 0;
				while (*cp && strchr("rwxugoXs", *cp))
					switch (*cp++) {
					case 'r': new_val |= 04; break;
					case 'w': new_val |= 02; break;
					case 'x': new_val |= 01; break;
					case 'u': new_val |= old_umask >> 6;
						  break;
					case 'g': new_val |= old_umask >> 3;
						  break;
					case 'o': new_val |= old_umask >> 0;
						  break;
					case 'X': if (old_umask & 0111)
							new_val |= 01;
						  break;
					case 's': /* ignored */
						  break;
					}
				new_val = (new_val & 07) * positions;
				switch (op) {
				case '-':
					new_umask &= ~new_val;
					break;
				case '=':
					new_umask = new_val
					    | (new_umask & ~(positions * 07));
					break;
				case '+':
					new_umask |= new_val;
				}
				if (*cp == ',') {
					positions = 0;
					cp++;
				} else if (!strchr("=+-", *cp))
					break;
			}
			if (*cp)
				errorf("umask: bad format\n");
			new_umask = ~new_umask;
		}
		umask(new_umask);
	}
	return 0;
}

int
c_dot(wp)
	char **wp;
{
	char *file, *cp;
	char **argv;
	int argc;
	int i;

	if ((cp = wp[1]) == NULL)
		return 0;
	file = search(cp, path, R_OK);
	if (file == NULL)
		errorf("%s: not found\n", cp);

	/* Set positional parameters? */
	if (wp[2]) {
		argv = ++wp;
		argv[0] = e->loc->argv[0]; /* preserve $0 */
		for (argc = -1; *wp++; argc++)
			;
	} else {
		argc = 0;
		argv = (char **) 0;
	}
	i = include(file, argc, argv);
	if (i)
		return exstat;
	return 1;
}

int
c_wait(wp)
	char **wp;
{
	int UNINITIALIZED(rv);

	while (ksh_getopt(wp, &builtin_opt, "") != EOF)
		;
	wp += builtin_opt.optind;
	if (*wp == (char *) 0) {
		while (waitfor((char *) 0) >= 0 && !sigtraps[SIGINT].set)
			;
		rv = 0;
	} else {
		for (; *wp; wp++)
			rv = waitfor(*wp);
		if (rv < 0)
			rv = 127; /* magic exit code: bad job-id */
	}
	return rv;
}

int
c_read(wp)
	register char **wp;
{
	register int c = 0;
	int expand = 1, history = 0;
	register char *cp;
	int fd = 0;
	struct shf *shf;
	int fl;
	int optc;
	XString xs;
	char UNINITIALIZED(*xp);

	while ((optc = ksh_getopt(wp, &builtin_opt, "rsu,")) != EOF)
		switch (optc) {
		case 'r':
			expand = 0;
			break;
		case 's':
			history = 1;
			break;
		case 'u':
			cp = builtin_opt.optarg;
			if (*cp
			    && (!digit(*cp)
			      || (fl = fcntl(fd = *cp++ - '0', F_GETFL, 0) < 0)
			      || (fl != O_RDONLY && fl != O_RDWR)))
				errorf("read: bad -u argument\n");
			break;
		}
	wp += builtin_opt.optind;

	if (*wp == NULL)
		*--wp = "REPLY";

	/* Since we can't necessarily seek backwards on non-regular files,
	 * don't buffer them so we can't read too much.
	 */
	shf = shf_reopen(fd, SHF_RD | SHF_INTERRUPT | can_seek(fd), shl_spare);

	if ((cp = strchr(*wp, '?')) != NULL) {
		*cp = 0;
		if (Flag(FTALKING)) {
			/* at&t says it prints prompt on fd if its open
			 * for writing and is a tty, but it doesn't do it
			 */
			shellf("%s ", cp+1);
			shf_flush(shl_out);
		}
	}

	if (history)
		Xinit(xs, xp, 128);
	for (; *wp != NULL; wp++) {
		for (cp = line; cp <= line+LINE; ) {
			if (c == '\n' || c == EOF)
				break;
			while ((c = shf_getc(shf)) == '\0')
				;
			if (history) {
				Xcheck(xs, xp);
				Xput(xs, xp, c);
			}
			if (expand && c == '\\') {
				while ((c = shf_getc(shf)) == '\0')
					;
				if (c == '\n') {
					c = 0;
					if (Flag(FTALKING) && isatty(fd))
						/* yylex() set prompt to PS2 */
						pprompt(prompt);
				} else if (c != EOF)
					*cp++ = c;
				continue;
			}
			if (c == '\n' || c == EOF)
				break;
			if (ctype(c, C_IFS)) {
				if (cp == line && ctype(c, C_IFSWS))
					continue;
				if (wp[1])
					break;
			}
			*cp++ = c;
		}
		/* strip trailing IFS white space from last variable */
		if (!wp[1])
			while (cp > line && ctype(cp[-1], C_IFS)
					 && ctype(cp[-1], C_IFSWS))
				cp--;
		*cp = 0;
		setstr(global(*wp), line);
	}

	shf_flush(shf);
	if (history) {
		Xput(xs, xp, '\0');
#ifdef EASY_HISTORY
		histsave(Xstring(xs, xp));
#else /* EASY_HISTORY */
		/* todo: is incremented line here correct? */
		histsave(++(source->line), Xstring(xs, xp), 1);
#endif /* EASY_HISTORY */
		Xfree(xs, xp);
	}

	return c == EOF;
}

int
c_eval(wp)
	register char **wp;
{
	register struct source *s;

	s = pushs(SWORDS);
	s->u.strv = wp+1;
	return shell(s, FALSE);
}

int
c_trap(wp)
	register char **wp;
{
	int i;
	char *s;
	register Trap *p;

	ksh_getopt(wp, &builtin_opt, "");
	wp += builtin_opt.optind;

	if (*wp == NULL) {
		int anydfl = 0;

		for (p = sigtraps, i = SIGNALS+1; --i >= 0; p++) {
			if (p->trap == NULL)
				anydfl = 1;
			else {
				shprintf("trap -- ");
				print_value_quoted(p->trap);
				shprintf(" %s\n", p->name);
			}
		}
#if 0 /* this is ugly and not clear POSIX needs it */
		/* POSIX may need this so output of trap can be saved and
		 * used to restore trap conditions
		 */
		if (anydfl) {
			shprintf("trap -- -");
			for (p = sigtraps, i = SIGNALS+1; --i >= 0; p++)
				if (p->trap == NULL && p->name)
					shprintf(" %s", p->name);
			shprintf("\n");
		}
#endif
		return 0;
	}

	s = (gettrap(*wp) == NULL) ? *wp++ : NULL; /* get command */
	if (s != NULL && s[0] == '-' && s[1] == '\0')
		s = NULL;

	/* set/clear traps */
	while (*wp != NULL) {
		p = gettrap(*wp++);
		if (p == NULL) {
			shellf("trap: bad signal %s\n", wp[-1]);
			shf_flush(shl_out);
			return 1;
		}
		settrap(p, s);
	}
	return 0;
}

int
c_exitreturn(wp)
	char **wp;
{
	int how = LEXIT;

	if (wp[1] != NULL)
		exstat = getn_(wp[1], wp[0]);
	if (wp[0][0] == 'r') { /* return */
		struct env *ep;

		/* need to tell if this is exit or return so trap exit will
		 * work right (POSIX)
		 */
		for (ep = e; ep; ep = ep->oenv)
			if (STOP_RETURN(ep->type)) {
				how = LRETURN;
				break;
			}
	}

#ifdef JOBS
	if (how == LEXIT && !really_exit && j_stopped()) {
		really_exit = 1;
		how = LSHELL;
	}
#endif /* JOBS */

	quitenv();	/* get rid of any i/o redirections */
	unwind(how);
	/*NOTREACHED*/
	return 0;
}

int
c_brkcont(wp)
	register char **wp;
{
	int n, quit;
	struct env *ep;

	quit = n = wp[1] == NULL ? 1 : getn(wp[1]);
	if (quit <= 0) {
		/* at&t ksh does this for non-interactive shells only - weird */
		errorf("%s: bad option `%s'\n", wp[0], wp[1]);
		return 1;
	}

	/* Stop at E_NONE, E_PARSE, E_FUNC, or E_INCL */
	for (ep = e; ep && !STOP_BRKCONT(ep->type); ep = ep->oenv)
		if (ep->type == E_LOOP) {
			if (--quit == 0)
				break;
			ep->flags |= EF_BRKCONT_PASS;
		}

	if (quit) {
		/* at&t ksh doesn't print a message - just does what it
		 * can.  We print a message 'cause it helps in debugging
		 * scripts, but don't generate an error (ie, keep going).
		 */
		if (source->type == SFILE)
			shellf("%s:%d: ", source->file, source->line);
		if (n == quit) {
			shellf("cannot %s\n", wp[0]);
			return 0; 
		}
		shellf("%s: can only go %d level(s)\n", wp[0], n - quit);
	}

	unwind(*wp[0] == 'b' ? LBREAK : LCONTIN);
	/*NOTREACHED*/
}

int
c_set(wp)
	register char **wp;
{
	int argi, setargs;
	struct block *l = e->loc;
	register char **owp = wp;

	if (wp[1] == NULL) {
		static char *args [] = {"set", "-", NULL};
		extern int c_typeset ARGS((char **args));
		return c_typeset(args);
	}

	argi = parse_args(wp, OF_SET, (char **) 0, &setargs);
	/* set $# and $* */
	if (setargs) {
		owp = wp += argi - 1;
		wp[0] = l->argv[0]; /* save $0 */
		while (*++wp != NULL)
			*wp = strsave(*wp, &l->area);
		l->argc = wp - owp - 1;
		l->argv = (char **) alloc(sizeofN(char *, l->argc+2), &l->area);
		for (wp = l->argv; (*wp++ = *owp++) != NULL; )
			;
	}
	return 0;
}

int
c_unset(wp)
	register char **wp;
{
	register char *id;
	int optc, unset_var = 1;

	while ((optc = ksh_getopt(wp, &builtin_opt, "fv")) != EOF)
		switch (optc) {
		case 'f':
			unset_var = 0;
			break;
		case 'v':
			unset_var = 1;
			break;
		}
	wp += builtin_opt.optind;
	for (; (id = *wp) != NULL; wp++)
		if (unset_var) {	/* unset variable */
			struct tbl *vp = global(id);

			if ((vp->flag&RDONLY))
				errorf("unset: %s is readonly\n", vp->name);
			unset(vp);
		} else			/* unset function */
			define(id, (struct op *)NULL);
	return 0;
}

int
c_times(wp)
	char **wp;
{
	struct tms all;

	(void) ksh_times(&all);
	shprintf("Shell: %8s user ", clocktos(all.tms_utime));
	shprintf("%8s system\n", clocktos(all.tms_stime));
	shprintf("Kids:  %8s user ", clocktos(all.tms_cutime));
	shprintf("%8s system\n", clocktos(all.tms_cstime));

	return 0;
}

/*
 * time pipeline (really a statement, not a built-in command)
 */
int
timex(t, f)
	struct op *t;
	int f;
{
	int rv;
	struct tms t0, t1;
	clock_t t0t, t1t;
	extern clock_t j_utime, j_stime; /* computed by j_wait */

	j_utime = j_stime = 0;
	t0t = ksh_times(&t0);
	rv = execute(t->left, f);
	t1t = ksh_times(&t1);

	shellf("%8s real ", clocktos(t1t - t0t));
	shellf("%8s user ",
	       clocktos(t1.tms_utime - t0.tms_utime + j_utime));
	shellf("%8s system ",
	       clocktos(t1.tms_stime - t0.tms_stime + j_stime));
	shellf("\n");

	return rv;
}

static char *
clocktos(t)
	clock_t t;
{
	static char temp[20];
	register int i;
	register char *cp = temp + sizeof(temp);

	if (CLK_TCK != 100)	/* convert to 1/100'ths */
	    t = (t < 1000000000/CLK_TCK) ?
		    (t * 100) / CLK_TCK : (t / CLK_TCK) * 100;

	*--cp = '\0';
	*--cp = 's';
	for (i = -2; i <= 0 || t > 0; i++) {
		if (i == 0)
			*--cp = '.';
		*--cp = '0' + (char)(t%10);
		t /= 10;
	}
	return cp;
}

/* dummy function, special case in comexec() */
int
c_exec(wp)
	char ** wp;
{
	return 0;
}

/* dummy function, special case in comexec() */
int
c_builtin(wp)
	char ** wp;
{
	return 0;
}

extern	int c_test ARGS((char **wp));		/* in c_test.c */
extern	int c_ulimit ARGS((char **wp));		/* in c_ulimit.c */

/* A leading = means assignments before command are kept;
 * a leading * means a POSIX special builtin;
 * a leading + means a POSIX regular builtin
 * (* and + should not be combined).
 */
const struct builtin shbuiltins [] = {
	{"*=.", c_dot},
	{"*=:", c_label},
	{"[", c_test},
	{"*=break", c_brkcont},
	{"=builtin", c_builtin},
	{"*=continue", c_brkcont},
	{"*=eval", c_eval},
	{"*=exec", c_exec},
	{"*=exit", c_exitreturn},
	{"+=false", c_label},
	{"*=return", c_exitreturn},
	{"*=set", c_set},
	{"*=shift", c_shift},
	{"=times", c_times},
	{"*=trap", c_trap},
	{"+=wait", c_wait},
	{"+read", c_read},
	{"test", c_test},
	{"+=true", c_label},
	{"ulimit", c_ulimit},
	{"+umask", c_umask},
	{"*=unset", c_unset},
	{NULL, NULL}
};
