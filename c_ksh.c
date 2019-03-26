/*
 * built-in Korn commands: c_*
 */

#if !defined(lint) && !defined(no_RCSids)
static char *RCSid = "$Id: c_ksh.c,v 1.3 1994/05/31 13:34:34 michael Exp $";
#endif

#include "sh.h"
#include "expand.h"
#include <ctype.h>

int
c_cd(wp)
	char	**wp;
{
	int		rval;
	int		cdnode;		/* was a node from cdpath added in? */
	int		printpath = 0;	/* print where we cd'd? */
	char		*cp, *cdpath, *pwd, *oldpwd;
	struct tbl	*pwd_s, *oldpwd_s;
	char		path[PATH];
	extern int	make_path();
	extern void	simplify_path();

	pwd = strval(pwd_s = global("PWD"));
	oldpwd = strval(oldpwd_s = global("OLDPWD"));

	if ((cp = wp[1]) == (char *) 0) {
		if ((cp = strval(global("HOME"))) == null)
			errorf("cd: no home directory\n");
	} else if (wp[2] == (char *) 0) {
		if (strcmp(cp, "-") == 0) {
			if ((cp = oldpwd) == null)
				errorf("cd: no OLDPWD\n");
			printpath++;
		}
	} else if (wp[3] == (char *) 0) {
		int	ilen, olen, nlen;

		/* substitue arg1 for arg2 in current path.
		 * if the first substitution fails because the cd fails
		 * we could try to find another substitution. For now
		 * we don't
		 */
		if ((cp = strstr(pwd, wp[1])) == (char *) 0)
			errorf("cd: bad substitution\n");
		ilen = cp - pwd;
		olen = strlen(wp[1]);
		nlen = strlen(wp[2]);
		if (ilen + nlen + strlen(pwd + ilen + olen) + 1 > sizeof(path))
			errorf("cd: path too long\n");
		strncpy(path, pwd, ilen);
		strcpy(path + ilen, wp[2]);
		strcpy(path + ilen + nlen, pwd + ilen + olen);
		cp = path;
		printpath++;
		/* XXX: make_path() copies cp to path, in this case cp to cp. */
	} else
		errorf("cd: too many arguments\n");

	cdpath = strval(global("CDPATH"));
	do {
		cdnode = make_path(pwd, cp, &cdpath, path, sizeof(path));
		simplify_path(path);
		rval = chdir(path);
	} while (rval < 0 && cdpath != (char *) 0);

	if (rval < 0)
		errorf("cd: %s: bad directory\n", cp);

	flushcom(0);

	if (printpath || cdnode)
		shellf("%s\n", path);

	setstr(oldpwd_s, pwd);
	setstr(pwd_s, path);

	return 0;
}

int
c_print(wp)
	register char **wp;
{
#define PO_NL		BIT(0)	/* print newline */
#define PO_EXPAND	BIT(1)	/* expand backslash sequences */
#define PO_ECHO		BIT(2)	/* act (kind of) like BSD echo */
#define PO_HIST		BIT(3)	/* print to history instead of stdout */
	int fd = 1;
	int flags = PO_EXPAND|PO_NL;
	char *s;
	XString xs;
	char *xp;

	if (wp[0][0] == 'e')	/* strict sysV echo: escapes, no options */
		wp += 1;
	else {
		int optc, fl;
		char *options = "Rnrsu,";

		while ((optc = ksh_getopt(wp, &builtin_opt, options)) != EOF)
			switch (optc) {
			case 'R':
				flags |= PO_ECHO; /* fake BSD echo command */
				flags &= ~PO_EXPAND;
				options = "ne";
				break;
			case 'e':
				flags |= PO_EXPAND;
				break;
			case 'n':
				flags &= ~PO_NL;
				break;
			case 'r':
				flags &= ~PO_EXPAND;
				break;
			case 's':
				flags |= PO_HIST;
				break;
			case 'u':
				s = builtin_opt.optarg;
				if (*s
				    && (!digit(*s)
				      || (fl = fcntl(fd = *s++ - '0', F_GETFL, 0) < 0)
				      || (fl != O_WRONLY && fl != O_RDWR)))
					errorf("print: bad -u argument\n");
			}
		if (!(builtin_opt.info & GI_MINUSMINUS)) {
			/* treat a lone - like -- */
			if (wp[builtin_opt.optind]
			    && strcmp(wp[builtin_opt.optind], "-") == 0)
				builtin_opt.optind++;
		} else if (flags & PO_ECHO)
			builtin_opt.optind--;
		wp += builtin_opt.optind;
	}

	Xinit(xs, xp, 128);

	while (*wp != NULL) {
		register int c;
		s = *wp;
		while ((c = *s++) != '\0') {
			Xcheck(xs, xp);
			if ((flags & PO_EXPAND) && c == '\\') {
				switch ((c = *s++)) {
				/* oddly enough, \007 seems more portable than
				 * \a (due to HP-UX cc, Ultrix cc, old pcc's,
				 * etc.).
				 */
				case 'a': c = '\007'; break;
				case 'b': c = '\b'; break;
				case 'c': flags &= ~PO_NL;
					  continue; /* AT&T brain damage */
				case 'f': c = '\f'; break;
				case 'n': c = '\n'; break;
				case 'r': c = '\r'; break;
				case 't': c = '\t'; break;
				case 'v': c = 0x0B; break;
				case '0': case '1': case '2': case '3':
				case '4': case '5': case '6': case '7':
					c = c - '0';
					if (*s >= '0' && *s <= '7')
						c = 8*c + *s++ - '0';
					if (*s >= '0' && *s <= '7')
						c = 8*c + *s++ - '0';
					break;
				case '\0': s--; c = '\\'; break;
				case '\\': break;
				default:
					Xput(xs, xp, '\\');
				}
			}
			Xput(xs, xp, c);
		}
		if (*++wp != NULL)
			Xput(xs, xp, ' ');
	}
	if (flags & PO_NL)
		Xput(xs, xp, '\n');

	if (flags & PO_HIST) {
		Xput(xs, xp, '\0');
#ifdef EASY_HISTORY
		histsave(Xstring(xs, xp));
#else /* EASY_HISTORY */
		/* todo: is incremented line here correct? */
		histsave(++(source->line), Xstring(xs, xp), 1);
#endif /* EASY_HISTORY */
		Xfree(xs, xp);
	} else {
		int n, len = Xlength(xs, xp);

		for (s = Xstring(xs, xp); len > 0; ) {
			n = write(fd, s, len);
			if (n < 0) {
				if (errno == EINTR) {
					intrcheck();
					continue;
				}
				return 1;
			}
			s += n;
			len -= n;
		}
	}

	return 0;
}

int
c_whence(wp)
	register char **wp;
{
	register struct tbl *tp;
	char *id;
	int vflag = 0, pflag = 0;
	int ret = 0;
	int optc;

	while ((optc = ksh_getopt(wp, &builtin_opt, "pv")) != EOF)
		switch (optc) {
		case 'p':
			pflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		}
	wp += builtin_opt.optind;

	while ((vflag || ret == 0) && (id = *wp++) != NULL) {
		if (pflag)
			tp = NULL;
		else {
			tp = tsearch(&keywords, id, hash(id));
			if (tp == NULL) {
				tp = tsearch(&aliases, id, hash(id));
				if (tp && !(tp->flag & ISSET))
					tp = NULL;
			}
		}
		if (tp == NULL)
			tp = findcom(id, Flag(FTRACKALL), FALSE, pflag);
		if (vflag || (tp->type != CALIAS && tp->type != CEXEC
			      && tp->type != CTALIAS))
			shprintf("%s", id);
		switch (tp->type) {
		  case CKEYWD:
			if (vflag)
				shprintf(" is a keyword");
			break;
		  case CALIAS:
			if (vflag)
				/* odd but at&t ksh doesn't use quoting here */
				shprintf(" is an %salias for %s",
					(tp->flag & EXPORT) ? "exported " : "",
					tp->val.s);
			else
				print_value_quoted(tp->val.s);
			break;
		  case CFUNC:
			if (vflag) {
				shprintf(" is a");
				if (tp->flag & EXPORT)
					shprintf(" exported");
				if (tp->flag & TRACE)
					shprintf(" traced");
				if (!(tp->flag & ISSET))
					shprintf(" undefined");
				shprintf(" function");
			}
			break;
		  case CSHELL:
			if (vflag)
				shprintf(" is a shell builtin");
			break;
		  case CTALIAS:
		  case CEXEC:
			if (tp->flag & ISSET) {
				if (vflag) {
					shprintf(" is ");
					if (tp->type == CTALIAS)
						shprintf(
						    "a tracked %salias for ",
							(tp->flag & EXPORT) ?
								"exported "
							      : "");
				}
				shprintf("%s", tp->val.s);
			} else {
				if (vflag)
					shprintf(" not found");
				ret = 1;
			}
			break;
		  default:
			shprintf("%s is *GOK*", id);
			break;
		}
		if (vflag || !ret)
			shprintf("\n");
	}
	return ret;
}

/* typeset, export, and readonly */
int
c_typeset(wp)
	register char **wp;
{
	struct block *l = e->loc;
	struct tbl *vp, **p;
	int fset = 0, fclr = 0;
	int thing = 0, func = 0, local = 0;
	char *options = "L#R#UZ#fi#lrtux";	/* see comment below */
	char *fieldstr, *basestr;
	int field, base;
	int optc, flag;
	int pflag = 0;

	switch (**wp) {
 	  case 'e':		/* export */
 		fset |= EXPORT;
		options = "p";
 		break;
 	  case 'r':		/* readonly */
 		fset |= RDONLY;
		options = "p";
 		break;
	  case 's':		/* set */
		/* called with 'typeset -' */
		break;
 	  case 't':		/* typeset */
 		local = 1;
 		break;
 	}
 
	fieldstr = basestr = (char *) 0;
	builtin_opt.flags |= GF_PLUSOPT;
	/* at&t ksh seems to have 0-9 as options, which are multiplied
	 * to get a number that is used with -L, -R, -Z or -i (eg, -1R2
	 * sets right justify in a field of 12).  This allows options
	 * to be grouped in an order (eg, -Lu12), but disallows -i8 -L3 and
	 * does not allow the number to be specified as a seperate argument
	 * Here, the number must follow the RLZi option, but is optional
	 * (see the # kludge in ksh_getopt()).
	 */
	while ((optc = ksh_getopt(wp, &builtin_opt, options)) != EOF) {
		flag = 0;
		switch (optc) {
		  case 'L':
			flag |= LJUST;
			fieldstr = builtin_opt.optarg;
			break;
		  case 'R':
			flag |= RJUST;
			fieldstr = builtin_opt.optarg;
			break;
		  case 'U':
			/* at&t ksh uses u, but this conflicts with
			 * upper/lower case.  If this option is changed,
			 * need to change the -U below as well
			 */
			flag |= INT_U;
			break;
		  case 'Z':
			flag |= ZEROFIL;
			fieldstr = builtin_opt.optarg;
			break;
		  case 'f':
			func = 1;
			break;
		  case 'i':
			flag |= INTEGER;
			basestr = builtin_opt.optarg;
			break;
		  case 'l':
			flag |= LCASEV;
			break;
		  case 'p': /* posix export/readonly -p flag */
			pflag = 1;
			break;
		  case 'r':
			flag |= RDONLY;
			break;
		  case 't':
			flag |= TRACE;
			break;
		  case 'u':
			flag |= UCASEV_AL;	/* upper case / autoload */
			break;
		  case 'x':
			flag |= EXPORT;
			break;
		}
		if (builtin_opt.info & GI_PLUS) {
			fclr |= flag;
			fset &= ~flag;
			thing = '+';
		} else {
			fset |= flag;
			fclr &= ~flag;
			thing = '-';
		}
	}

	field = fieldstr ? getn(fieldstr) : 0;
	base = basestr ? getn(basestr) : 0;

	if (!(builtin_opt.info & GI_MINUSMINUS) && wp[builtin_opt.optind]
	    && (wp[builtin_opt.optind][0] == '-'
		|| wp[builtin_opt.optind][0] == '+')
	    && wp[builtin_opt.optind][1] == '\0')
	{
		thing = wp[builtin_opt.optind][0];
		builtin_opt.optind++;
	}

	if (func && ((fset|fclr) & ~(TRACE|UCASEV_AL|EXPORT)))
		errorf("%s: only -t, -u and -x options may be used with -f\n",
			wp[0]);
	else if (wp[builtin_opt.optind]) {
		/* Take care of exclusions */
		/* setting these attributes clears the others, unless they
		 * are also set in this command
		 */
		if (fset & (LJUST|RJUST|ZEROFIL|UCASEV_AL|LCASEV|INTEGER
			    |INT_U|INT_L))
			fclr |= ~fset &
				(LJUST|RJUST|ZEROFIL|UCASEV_AL|LCASEV|INTEGER
				 |INT_U|INT_L);
		fclr &= ~fset;	/* set wins */
		if ((fset & (ZEROFIL|LJUST)) == ZEROFIL) {
			fset |= RJUST;
			fclr &= ~RJUST;
		}
		if (fset & LCASEV)	/* LCASEV has priority */
			fclr |= UCASEV_AL;
		else if (fset & UCASEV_AL)
			fclr |= LCASEV;
		if (fset & LJUST)	/* LJUST has priority */
			fclr |= RJUST;
		else if (fset & RJUST)
			fclr |= LJUST;
		if ((fset | fclr) & INTEGER) {
			if (!(fset | fclr) & INT_U)
				fclr |= INT_U;
			if (!(fset | fclr) & INT_L)
				fclr |= INT_L;
		}
		fset &= ~fclr; /* in case of something like -LR */
	}

	/* set variables and attributes */
	if (wp[builtin_opt.optind]) {
		int i;
		struct tbl *f;

		if (local && !func)
			fset |= LOCAL;
		for (i = builtin_opt.optind; wp[i]; i++) {
			if (func) {
				f = findfunc(wp[i], hash(wp[i]),
					     (fset&UCASEV_AL) ? TRUE : FALSE);
				if (!f)
					continue;
				if (fset | fclr) {
					f->flag |= fset;
					f->flag &= ~fclr;
				} else
					fptreef(shl_stdout, "function %s %T\n",
						wp[i], f->val.t);
			} else if (!typeset(wp[i], fset, fclr, field, base))
				errorf("%s: %s: not identifier\n",
					wp[0], wp[i]);
		}
		return 0;
	}

	/* list variables and attributes */
	flag = fset | fclr; /* no difference at this point.. */
	for (l = e->loc; l; l = l->next) {
	    for (p = tsort(func ? &l->funs : &l->vars); (vp = *p++); )
		for (; vp; vp = vp->array) {
		    if (!(vp->flag&ISSET))
			continue;
		    /* no arguments */
		    if (thing == 0 && flag == 0) {
			/* at&t ksh prints things like export, integer,
			 * leftadj, zerofill, etc., but POSIX says must
			 * be suitable for re-entry...
			 */
			shprintf("typeset ");
			if ((vp->flag&INTEGER))
			    shprintf("-i ");
			if ((vp->flag&EXPORT))
			    shprintf("-x ");
			if ((vp->flag&RDONLY))
			    shprintf("-r ");
			if ((vp->flag&TRACE)) 
			    shprintf("-t ");
			if ((vp->flag&LJUST)) 
			    shprintf("-L%d ", vp->field);
			if ((vp->flag&RJUST)) 
			    shprintf("-R%d ", vp->field);
			if ((vp->flag&ZEROFIL)) 
			    shprintf("-Z ");
			if ((vp->flag&LCASEV)) 
			    shprintf("-l ");
			if ((vp->flag&UCASEV_AL)) 
			    shprintf("-u ");
			if ((vp->flag&INT_U)) 
			    shprintf("-U ");
			if (vp->flag&ARRAY)
			    shprintf("%s[%d]\n", vp->name,vp->index);
			else
			    shprintf("%s\n", vp->name);
		    } else {
			if (flag && (vp->flag & flag) == 0)
				continue;
			if (func) {
			    if (thing == '-')
				fptreef(shl_stdout, "function %s %T\n",
					vp->name, vp->val.t);
			    else
				shprintf("%s\n", vp->name);
			} else {
			    if (pflag)
				shprintf("%s ",
				    (flag & EXPORT) ?  "export" : "readonly");
			    if (vp->flag&ARRAY)
				shprintf("%s[%d]", vp->name, vp->index);
			    else
				shprintf("%s", vp->name);
			    if (thing == '-') {
				char *s = strval(vp);

				shprintf("=");
				/* at&t ksh can't have justified integers.. */
				if ((vp->flag & (INTEGER|LJUST|RJUST))
								== INTEGER)
				    shprintf("%s", s);
				else
				    print_value_quoted(s);
			    }
			    shprintf("\n");
			}
		    }
		}
	}
	return 0;
}
	
int
c_alias(wp)
	register char **wp;
{
	struct table *t = &aliases;
	int rv = 0, tflag, Uflag = 0, xflag = 0;
	int optc;

	while ((optc = ksh_getopt(wp, &builtin_opt, "dtUx")) != EOF)
		switch (optc) {
		  case 'd':
			t = &homedirs;
			break;
		  case 't':
			t = &taliases;
			break;
		  case 'U': /* kludge for tracked alias initialization
			     * (don't do a path search, just make an entry)
			     */
			Uflag = 1;
			break;
		  case 'x':
			xflag = EXPORT;
			break;
		}
	wp += builtin_opt.optind;

	if (*wp == NULL) {
		struct tbl *ap, **p;

		for (p = tsort(t); (ap = *p++) != NULL; )
			if ((ap->flag & (ISSET|xflag)) == (ISSET|xflag)) {
				shprintf("%s=", ap->name);
				print_value_quoted(ap->val.s);
				shprintf("\n");
			}
	}

	tflag = t == &taliases;

	for (; *wp != NULL; wp++) {
		char *alias = *wp;
		char *val = strchr(alias, '=');
		char *newval;
		struct tbl *ap;
		int h;

		if (val)
			alias = strnsave(alias, val++ - alias, ATEMP);
		h = hash(alias);
		if (val == NULL && !tflag && !xflag) {
			ap = tsearch(t, alias, h);
			if (ap != NULL && (ap->flag&ISSET)) {
				shprintf("%s=", ap->name);
				print_value_quoted(ap->val.s);
				shprintf("\n");
			} else {
				shprintf("%s alias not found\n", alias);
				rv = 1;
			}
			continue;
		}
		ap = tenter(t, alias, h);
		ap->type = tflag ? CTALIAS : CALIAS;
		/* Are we setting the value or just some flags? */
		if ((val && !tflag) || (!val && tflag && !Uflag)) {
			if (ap->flag&ALLOC) {
				ap->flag &= ~(ALLOC|ISSET);
				afree((void*)ap->val.s, APERM);
			}
			/* ignore values for -t (at&t ksh does this) */
			newval = tflag ? search(alias, path, X_OK) : val;
			if (newval) {
				ap->val.s = strsave(newval, APERM);
				ap->flag |= ALLOC|ISSET;
			} else
				ap->flag &= ~ISSET;
		}
		ap->flag |= DEFINED|xflag;
		if (val)
			afree(alias, ATEMP);
	}

	return rv;
}

int
c_unalias(wp)
	register char **wp;
{
	register struct table *t = &aliases;
	register struct tbl *ap;
	int rv = 0, all = 0;
	int optc;

	while ((optc = ksh_getopt(wp, &builtin_opt, "adt")) != EOF)
		switch (optc) {
		  case 'a':
			all = 1;
			break;
		  case 'd':
			t = &homedirs;
			break;
		  case 't':
			t = &taliases;
			break;
		}
	wp += builtin_opt.optind;

	for (; *wp != NULL; wp++) {
		ap = tsearch(t, *wp, hash(*wp));
		if (ap == NULL) {
			rv = 1;	/* POSIX */
			continue;
		}
		if (ap->flag&ALLOC) {
			ap->flag &= ~(ALLOC|ISSET);
			afree((void*)ap->val.s, APERM);
		}
		ap->flag &= ~(DEFINED|ISSET|EXPORT);
	}

	if (all) {
		for (twalk(t); (ap = tnext()); ) {
			if (ap->flag&ALLOC) {
				ap->flag &= ~(ALLOC|ISSET);
				afree((void*)ap->val.s, APERM);
			}
			ap->flag &= ~(DEFINED|ISSET|EXPORT);
		}
	}

	return rv;
}

int
c_let(wp)
	char **wp;
{
	int rv = 1;

	if (wp[1] == (char *) 0) /* at&t ksh does this */
		errorf("%s: no arguments\n");
	for (wp++; *wp; wp++)
		rv = evaluate(*wp) == 0;
	return rv;
}

int
c_jobs(wp)
	char **wp;
{
	int optc;
	int flag = 0;
	int nflag = 0;

	while ((optc = ksh_getopt(wp, &builtin_opt, "lpnz")) != EOF)
		switch (optc) {
		case 'l':
			flag = 1;
			break;
		case 'p':
			flag = 2;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'z':	/* debugging: print zombies */
			nflag = -1;
			break;
		}
	wp += builtin_opt.optind;
	if (!*wp)
		j_jobs((char *) 0, flag, nflag);
	else
		for (; *wp; wp++)
			j_jobs(*wp, flag, nflag);
	return 0;
}

#ifdef JOBS
int
c_fgbg(wp)
	register char **wp;
{
	int bg = strcmp(*wp, "bg") == 0;
	int UNINITIALIZED(rv);

	if (!Flag(FMONITOR))
		errorf("%s: Job control not enabled\n", *wp);
	ksh_getopt(wp, &builtin_opt, "");
	wp += builtin_opt.optind;
	if (*wp)
		for (; *wp; wp++)
			rv = j_resume(*wp, bg);
	else
		rv = j_resume("%%", bg);
	/* POSIX says fg shall return 0 (unless an error occurs).
	 * at&t ksh returns the exit value of the job...
	 */
	return (bg || Flag(FPOSIX)) ? 0 : rv;
}
#endif

int
c_kill(wp)
	register char **wp;
{
	Trap *t = (Trap *) 0;
	char *p;
	int lflag = 0;
	int i, n, rv, sig;

	/* assume old style options if -digits or -UPPERCASE */
	if ((p = wp[1]) && *p == '-' && (digit(p[1]) || isupper(p[1]))) {
		if (!(t = gettrap(p + 1)))
			errorf("kill: bad signal `%s'\n", p + 1);
		i = (wp[2] && strcmp(wp[2], "--") == 0) ? 3 : 2;
	} else {
		int optc;

		while ((optc = ksh_getopt(wp, &builtin_opt, "ls:")) != EOF)
			switch (optc) {
			case 'l':
				lflag = 1;
				break;
			case 's':
				if (!(t = gettrap(builtin_opt.optarg)))
					errorf("kill: bad signal `%s'\n",
						builtin_opt.optarg);
			}
		i = builtin_opt.optind;
	}
	if ((lflag && t) || (!wp[i] && !lflag))
		errorf(
"Usage: kill [ -s signal | -signum | -NAME ] {pid|job}...\n\
       kill -l [exit_status]\n"
			);

	if (lflag) {
		if (wp[i]) {
			for (; wp[i]; i++) {
				n = getn(wp[i]);
				if (n > 128 && n < 128 + SIGNALS)
					n -= 128;
				if (n > 0 && n < SIGNALS && sigtraps[n].name)
					shprintf("%s\n", sigtraps[n].name);
				else
					shprintf("%d\n", n);
			}
		} else if (Flag(FPOSIX)) {
			p = "";
			for (i = 1; i < SIGNALS; i++, p = " ")
				if (sigtraps[i].name)
					shprintf("%s%s", p, sigtraps[i].name);
			shprintf("\n");
		} else {
			/* don't count signal 0, round up */
			int nrows = (SIGNALS - 1 + 1) / 2;
			int row, col;

			for (row = 0; row < nrows; row++)
				for (col = 0; col < 2; col++) {
					if (col)
						shprintf("  ");
					sig = row + col * nrows + 1;
					if (sig >= SIGNALS)
						break;
					if (col == 0 && row != 0)
						shprintf("\n");
					t = &sigtraps[sig];
					shprintf("%2d", t->signal);
					if (t->name)
						shprintf(" %8s", t->name);
					else
						shprintf(" %8d", t->signal);
					shprintf(" %-26s", t->mess);
				}
			shprintf("\n");
		}
		return 0;
	}
	rv = 0;
	sig = t ? t->signal : SIGTERM;
	for (; (p = wp[i]); i++) {
		if (*p == '%') {
			if (j_kill(p, sig))
				rv = 1;
		} else {
			n = getn(p);
			/* use killpg if < -1 since -1 does special things for
			 * some non-killpg-endowed kills
			 */
			if ((n < -1 ? killpg(-n, sig) : kill(n, sig)) < 0) {
				shellf("kill: %s: %s\n", p, strerror(errno));
				rv = 1;
			}
		}
	}
	return rv;
}

static Getopt	user_opt;	/* parsing state for getopts builtin command */
static int	getopts_noset;	/* stop OPTIND assign from resetting state */

void
getopts_reset(val)
	int val;
{
	if (!getopts_noset && val >= 1) {
		ksh_getopt_reset(&user_opt, Flag(FPOSIX) ? 0 : GF_PLUSOPT);
		user_opt.optind = val;
	}
}

int
c_getopts(wp)
	char **wp;
{
	int	argc;
	char	*options;
	char	*var;
	int	optc;
	char	buf[3];

	ksh_getopt(wp, &builtin_opt, "");
	wp += builtin_opt.optind;

	options = *wp++;
	if (options == (char *) 0)
		errorf("getopts: missing options argument\n");

	var = *wp++;
	if (var == (char *) 0)
		errorf("getopts: missing name argument\n");
	if (!*var || *skip_varname(var, TRUE))
		errorf("getopts: %s: is not an identifier\n", var);

	if (e->loc->next == (struct block *) 0)
		errorf("getopts: internal error (no argv)\n");
	/* Which arguments are we parsing... */
	if (*wp == (char *) 0)
		wp = e->loc->next->argv;
	else
		*--wp = e->loc->next->argv[0];

	/* Check that our saved state won't cause a core dump... */
	for (argc = 0; wp[argc]; argc++)
		;
	if (user_opt.optind > argc
	    || (user_opt.p != 0
		&& user_opt.p > strlen(wp[user_opt.optind - 1])))
	      errorf("getopts: arguments changed since last call\n");

	user_opt.optarg = (char *) 0;
	optc = ksh_getopt(wp, &user_opt, options);

	if (optc >= 0 && (user_opt.info & GI_PLUS)) {
		buf[0] = '+';
		buf[1] = optc;
		buf[2] = '\0';
	} else {
		buf[0] = optc < 0 ? '?' : optc;
		buf[1] = '\0';
	}
	/* todo: what if var is readonly */
	setstr(global(var), buf);

	getopts_noset = 1;
	setint(global("OPTIND"), (long) user_opt.optind);
	getopts_noset = 0;

	if (user_opt.optarg == (char *) 0)
		unset(global("OPTARG"));
	else
		setstr(global("OPTARG"), user_opt.optarg);

	if (optc < 0)
		return 1;

	return 0;
}

#ifdef EMACS
int
c_bind(wp)
	register char **wp;
{
	int macro = 0;
	register char *cp;

	for (wp++; (cp = *wp) != NULL && *cp == '-'; wp++)
		if (cp[1] == 'm')
			macro = 1;

	if (*wp == NULL)	/* list all */
		x_bind((char*)NULL, (char*)NULL, 0);

	for (; *wp != NULL; wp++) {
		cp = strchr(*wp, '=');
		if (cp != NULL)
			*cp++ = 0;
		x_bind(*wp, cp, macro);
	}

	return 0;
}
#endif

extern	c_fc();

/* A leading = means assignments before command are kept;
 * a leading * means a POSIX special builtin;
 * a leading + means a POSIX regular builtin
 * (* and + should not be combined).
 */
const struct builtin kshbuiltins [] = {
	{"+alias", c_alias},	/* no =: at&t manual wrong */
	{"+cd", c_cd},
	{"echo", c_print},
 	{"*=export", c_typeset},
	{"+fc", c_fc},
	{"+getopts", c_getopts},
	{"+jobs", c_jobs},
	{"+kill", c_kill},
	{"let", c_let},
	{"print", c_print},
 	{"*=readonly", c_typeset},
	{"=typeset", c_typeset},
	{"+unalias", c_unalias},
	{"whence", c_whence},
#ifdef JOBS
	{"+bg", c_fgbg},
	{"+fg", c_fgbg},
#endif
#ifdef EMACS
	{"bind", c_bind},
#endif
	{NULL, NULL}
};
