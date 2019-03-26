/*
 * execute command tree
 */

#if !defined(lint) && !defined(no_RCSids)
static char *RCSid = "$Id: exec.c,v 1.4 1994/05/31 13:34:34 michael Exp $";
#endif

#include "sh.h"
#include "expand.h"
#include "ksh_stat.h"

static int	comexec ARGS((struct op *t, char **vp, char **ap,
			      int volatile flags));
static void	scriptexec ARGS((struct op *tp, char **ap));
static int	call_builtin ARGS((int (*f)(char **), char **wp));
static void	iosetup ARGS((struct ioword *iop));
static int	herein ARGS((char *hname, int sub));
static void	echo ARGS((char **vp, char **ap));
static char 	*do_selectargs ARGS((char **ap));
static int	pr_menu ARGS((register char **ap));


/*
 * handle systems that don't have F_SETFD
 */
#ifndef F_SETFD
# ifndef MAXFD
#   define  MAXFD 64
# endif
/* a bit field would be smaller, but this will work */
static char clexec_tab[MAXFD+1];
#endif

/*
 * we now use this function always.
 */
int
fd_clexec(fd)
    int fd;
{
#ifndef F_SETFD
	if (fd >= 0 && fd < sizeof(clexec_tab)) {
		clexec_tab[fd] = 1;
		return 0;
	}
	return -1;
#else
	return fcntl(fd, F_SETFD, 1);
#endif
}


/*
 * execute command tree
 */
int
execute(t, flags)
	struct op * volatile t;
	volatile int flags;	/* if XEXEC don't fork */
{
	int i;
	volatile int rv = 0;
	int pv[2];
	char ** volatile ap;
	char *s, *cp;
	struct ioword **iowp;

	if (t == NULL)
		return 0;

	if ((flags&XFORK) && !(flags&XEXEC) && t->type != TPIPE)
		return exchild(t, flags, -1); /* run in sub-process */

	newenv(E_EXEC);
	if (trap)
		runtraps(FALSE);
 
	if (t->ioact != NULL || t->type == TPIPE) {
		e->savefd = (short *) alloc(sizeofN(short, NUFILE), ATEMP);
		/* initialize to not redirected */
		memset(e->savefd, 0, sizeofN(short, NUFILE));
		/* mark fd 0/1 in-use if pipeline */
		if (flags&XPIPEI)
			e->savefd[0] = -1;
		if (flags&XPIPEO)
			e->savefd[1] = -1;
	}

	/* do redirection, to be restored in quitenv() */
	if (t->ioact != NULL)
		for (iowp = t->ioact; *iowp != NULL; iowp++) {
#if 0
/* the real [k]sh does not do this check and it can cause problems, e.g.:
 *	(output_on_stdout+err > /dev/null) 2>&1 | cat -n
 */
			if ((flags&XPIPEI) && (*iowp)->unit == 0 ||
			    (flags&XPIPEO) && (*iowp)->unit == 1)
				errorf("attempt to redirect fd 0/1 in pipe\n");
#endif /* 0 */
			iosetup(*iowp);
		}

	switch(t->type) {
	  case TCOM:
		e->type = E_TCOM;
		rv = comexec(t, eval(t->vars, DOASNTILDE),
			     eval(t->args, t->evalflags ? t->evalflags
					    : DOBLANK|DOGLOB|DOTILDE), flags);
		break;

	  case TPAREN:
		rv = execute(t->left, flags|XFORK);
		break;

	  case TPIPE:
		flags |= XFORK;
		flags &= ~XEXEC;
		e->savefd[0] = savefd(0);
		e->savefd[1] = savefd(1);
		(void) dup2(e->savefd[0], 0); /* stdin of first */
		while (t->type == TPIPE) {
			openpipe(pv);
			(void) dup2(pv[1], 1);	/* stdout of curr */
			/* Let exchild() close pv[0] in child
			 * (if this isn't done, commands like
			 *    (: ; cat /etc/termcap) | sleep 1
			 *  will hang forever).
			 */
			exchild(t->left, flags|XPIPEO|XCCLOSE, pv[0]);
			(void) dup2(pv[0], 0);	/* stdin of next */
			closepipe(pv);
			flags |= XPIPEI;
			t = t->right;
		}
		restfd(1, e->savefd[1]); /* stdout of last */
		e->savefd[1] = 0; /* no need to re-restore this */
		/* Let exchild() close 0 in parent, after fork, before wait */
		i = exchild(t, flags|XPCLOSE, 0);
		if (!(flags&XBGND) && !(flags&XXCOM))
			rv = i;
		break;

	  case TLIST:
		while (t->type == TLIST) {
			execute(t->left, flags & XERROK);
			t = t->right;
		}
		rv = execute(t, flags & XERROK);
		break;

	  case TASYNC:
		rv = execute(t->left, flags|XBGND|XFORK);
		break;

	  case TOR:
	  case TAND:
		rv = execute(t->left, XERROK);
		if (t->right != NULL && (rv == 0) == (t->type == TAND))
			rv = execute(t->right, 0);
		else
			flags |= XERROK;
		break;

	  case TBANG:
		rv = !execute(t->right, XERROK);
		break;

	  case TDBRACKET:
	  {
		XPtrV ap;

		/* Must build arguments the one by one becuase the second
		 * operand of = and != need to be eval'd with DOPAT.
		 */
		for (i = 0; t->args[i]; i++)
			;
		XPinit(ap, i + 1);
		for (i = 0; t->args[i]; i++)
			XPput(ap, evalstr(t->args[i], DOTILDE|DOASNTILDE
				    |(t->vars[i][1] == DB_PAT ? DOPAT : 0)));
		XPput(ap, (char *) 0);
		rv = call_builtin(c_test, (char **) XPptrv(ap));
		break;
	  }

	  case TFOR:
		ap = (t->vars != NULL) ?
			  eval(t->vars, DOBLANK|DOGLOB|DOTILDE)
			: e->loc->argv + 1;
		e->type = E_LOOP;
		while ((i = setjmp(e->jbuf)))
			if ((e->flags&EF_BRKCONT_PASS)
			    || (i != LBREAK && i != LCONTIN))
			{
				quitenv();
				unwind(i);
			} else if (i == LBREAK)
				goto Break;
		while (*ap != NULL) {
			setstr(global(t->str), *ap++);
			rv = execute(t->left, flags & XERROK);
		}
		break;

	  case TSELECT:
		ap = (t->vars != NULL) ?
			  eval(t->vars, DOBLANK|DOGLOB|DOTILDE)
			: e->loc->argv + 1;
		e->type = E_LOOP;
		while ((i = setjmp(e->jbuf)))
			if ((e->flags&EF_BRKCONT_PASS)
			    || (i != LBREAK && i != LCONTIN))
			{
				quitenv();
				unwind(i);
			} else if (i == LBREAK)
				goto Break;
		for (;;) {
			if ((cp = do_selectargs(ap)) == (char *)0)
				break;
			setstr(global(t->str), cp);
			rv = execute(t->left, flags & XERROK);
		}
		break;

	  case TWHILE:
	  case TUNTIL:
		e->type = E_LOOP;
		while ((i = setjmp(e->jbuf)))
			if ((e->flags&EF_BRKCONT_PASS)
			    || (i != LBREAK && i != LCONTIN))
			{
				quitenv();
				unwind(i);
			} else if (i == LBREAK)
				goto Break;
		while ((execute(t->left, XERROK) == 0) == (t->type == TWHILE))
			rv = execute(t->right, flags & XERROK);
		break;

	  case TIF:
	  case TELIF:
		if (t->right == NULL)
			break;	/* should be error */
		rv = execute(t->left, XERROK) == 0 ?
			execute(t->right->left, flags & XERROK) :
			execute(t->right->right, flags & XERROK);
		break;

	  case TCASE:
		cp = evalstr(t->str, DOTILDE);
		for (t = t->left; t != NULL && t->type == TPAT; t = t->right)
		    for (ap = t->vars; *ap; ap++)
			if ((s = evalstr(*ap, DOTILDE|DOPAT)) && gmatch(cp, s))
				goto Found;
		break;
	  Found:
		rv = execute(t->left, flags & XERROK);
		break;

	  case TBRACE:
		rv = execute(t->left, flags & XERROK);
		break;

	  case TFUNCT:
		rv = define(t->str, t->left);
		break;

	  case TTIME:
		rv = timex(t, flags);
		break;

	  case TEXEC:		/* an eval'd TCOM */
		s = t->args[0];
		ap = makenv();
#ifndef F_SETFD
		for (i = 0; i < sizeof(clexec_tab); i++)
			if (clexec_tab[i]) {
				close(i);
				clexec_tab[i] = 0;
			}
#endif
		restoresigs();
		execve(t->str, t->args, ap);
		if (errno == ENOEXEC)
			scriptexec(t, ap);
		else
			errorf("%s: %s\n", s, strerror(errno));
	}
    Break:
	exstat = rv;

	quitenv();		/* restores IO */
	if ((flags&XEXEC))
		exit(rv);	/* exit child */
	if (rv != 0 && !(flags & XERROK)) {
		if (Flag(FERREXIT)) {
			exstat = rv;
			unwind(LERROR);
		}
		trapsig(SIGERR_);
	}
	return rv;
}

/*
 * execute simple command
 */

static int
comexec(t, vp, ap, flags)
	struct op *t;
	register char **ap, **vp;
	int volatile flags;
{
	int i;
	int rv = 0;
	register char *cp;
	register char **lastp;
	register struct tbl *tp = NULL;
	static struct op texec = {TEXEC};
	extern int c_exec(), c_builtin();

	if (Flag(FXTRACE))
		echo(vp, ap);

	/* snag the last argument for $_ */
	if ((lastp = ap) && *lastp) {
		while (*++lastp)
			;
		setstr(typeset("_",LOCAL,0,0,0),*--lastp);
	}	

	/* create new variable/function block */
	newblock();

 Doexec:
	if ((cp = *ap) == NULL)
		cp = ":";
	tp = findcom(cp, Flag(FTRACKALL), TRUE, FALSE);

	switch (tp->type) {
	  case CSHELL:			/* shell built-in */
		while (tp->val.f == c_builtin) {
			if ((cp = *++ap) == NULL)
				break;
			tp = tsearch(&builtins, cp, hash(cp));
			if (tp == NULL)
				errorf("%s: not builtin\n", cp);
		}
		if (tp->val.f == c_exec) {
			if (*++ap == NULL) {
				/* don't restore redirection */
				if (e->savefd != NULL) {
					for (i = 0; i < NUFILE; i++) {
						if (e->savefd[i] > 0)
							close(e->savefd[i]);
						/* keep anything > 2 private */
						if (i > 2 && e->savefd[i])
							fd_clexec(i);
					}
					e->savefd = NULL; 
				}
				/* fall through to variable assignments */
			} else {
				flags |= XEXEC;
				goto Doexec;
			}
		}
		if ((tp->flag&KEEPASN)) {
			e->loc = e->loc->next; /* no local block */
			i = 0;
		} else
			i = LOCAL;
		while (*vp != NULL)
			(void) typeset(*vp++, i, 0, 0, 0);
		if (tp->val.f != c_exec)
			rv = call_builtin(tp->val.f, ap);
		break;

	  case CFUNC:			/* function call */
	  {
		volatile int old_xflag;
		volatile int old_inuse;

		if (!(tp->flag&ISSET))
			errorf("%s: undefined function\n", cp);
		while (*vp != NULL)
			(void) typeset(*vp++, LOCAL, 0, 0, 0);

		e->loc->argv = ap;
		for (i = 0; *ap++ != NULL; i++)
			;
		e->loc->argc = i - 1;
		getopts_reset(1);

		old_xflag = Flag(FXTRACE);
		Flag(FXTRACE) = tp->flag & TRACE ? TRUE : FALSE;

		old_inuse = tp->flag & FINUSE;
		tp->flag |= FINUSE;

		e->type = E_FUNC;
		if ((i = setjmp(e->jbuf)) == 0) {
			/* seems odd to pass XERROK here, but at&t ksh does */
			exstat = execute(tp->val.t, flags & XERROK);
			i = LRETURN;
		}
		Flag(FXTRACE) = old_xflag;
		tp->flag = (tp->flag & ~FINUSE) | old_inuse;
		/* Were we deleted while executing?  If so, free the execution
		 * tree.  Unfortunately, the table entry is never re-used.
		 */
		if ((tp->flag & (FDELETE|FINUSE)) == FDELETE) {
			if (tp->flag & ALLOC) {
				tp->flag &= ~ALLOC;
				tfree(tp->val.t, tp->areap);
			}
			tp->flag = 0;
		}
		switch (i) {
		  case LRETURN:
		  case LERROR:
			rv = exstat;
			break;
		  case LINTR:
		  case LEXIT:
		  case LLEAVE:
		  case LSHELL:
			quitenv();
			unwind(i);
			/*NOTREACHED*/
		  default:
			quitenv();
			errorf("internal error: E_FUNC %d\n", i);
		}
		break;
	  }

	  case CEXEC:		/* executable command */
	  case CTALIAS:		/* tracked alias */
		if (!(tp->flag&ISSET)) {
			/*
			 * mlj addition:
			 *
			 * If you specify a full path to a file
			 * (or type the name of a file in .) which
			 * doesn't have execute priv's, it used to
			 * just say "not found".  Kind of annoying,
			 * particularly if you just wrote a script
			 * but forgot to say chmod 755 script.
			 *
			 * This should probably be done in eaccess(),
			 * but it works here (at least I haven't noticed
			 * changing errno here breaking something
			 * else).
			 *
			 * So, we assume that if the file exists, it
			 * doesn't have execute privs; else, it really
			 * is not found.
			 */
			if (access(cp, F_OK) < 0)
			    shellf("%s: not found\n", cp);
			else
			    shellf("%s: cannot execute\n", cp);
			shf_flush(shl_out);
			rv = 1;
			break;
		}

		/* set $_ to program's full path */
		setstr(typeset("_", LOCAL|EXPORT, 0, 0, 0), tp->val.s);
		while (*vp != NULL)
			(void) typeset(*vp++, LOCAL|EXPORT, 0, 0, 0);

		if ((flags&XEXEC)) {
			j_exit();
			if (!(flags&XBGND) || Flag(FMONITOR)) {
				setexecsig(&sigtraps[SIGINT], SS_RESTORE_ORIG);
				setexecsig(&sigtraps[SIGQUIT], SS_RESTORE_ORIG);
			}
		}

		/* to fork we set up a TEXEC node and call execute */
		texec.left = t;	/* for tprint */
		texec.str = tp->val.s;
		texec.args = ap;
		rv = exchild(&texec, flags, -1);
		break;
	}
	if (flags & XEXEC) {
		exstat = rv;
		unwind(LLEAVE);
	}
	return rv;
}

static void
scriptexec(tp, ap)
	register struct op *tp;
	register char **ap;
{
	char *shell;

	shell = strval(global("EXECSHELL"));
	if (shell && *shell) {
		if ((shell = search(shell,path, X_OK)) == NULL)
			shell = EXECSHELL;
	} else {
		shell = EXECSHELL;
	}

	*tp->args-- = tp->str;
#ifdef	SHARPBANG
	{
		char line[LINE];
		register char *cp;
		register int fd, n;

		line[0] = '\0';
		if ((fd = open(tp->str, O_RDONLY)) >= 0) {
			if ((n = read(fd, line, LINE - 1)) > 0)
				line[n] = '\0';
			(void) close(fd);
		}
		if (line[0] == '#' && line[1] == '!') {
			cp = &line[2];
			while (*cp && (*cp == ' ' || *cp == '\t'))
				cp++;
			if (*cp && *cp != '\n') {
				char *a0 = cp, *a1 = (char *) 0;

				while (*cp && *cp != '\n' && *cp != ' '
				       && *cp != '\t')
					cp++;
				if (*cp && *cp != '\n') {
					*cp++ = '\0';
					while (*cp
					       && (*cp == ' ' || *cp == '\t'))
						cp++;
					if (*cp && *cp != '\n') {
						a1 = cp;
						/* all one argument */
						while (*cp && *cp != '\n')
							cp++;
					}
				}
				if (*cp == '\n') {
					*cp = '\0';
					if (a1)
						*tp->args-- = a1;
					shell = a0;
				}
			}
		}
	}
#endif	/* SHARPBANG */
	*tp->args = shell;

	(void) execve(tp->args[0], tp->args, ap);
	/* report both the program that was run and the bogus shell */
	errorf("%s: %s: %s\n", tp->str, shell, strerror(errno));
}

int
shcomexec(wp)
	register char **wp;
{
	register struct tbl *tp;

	tp = tsearch(&builtins, *wp, hash(*wp));
	if (tp == NULL)
		errorf("%s: shcomexec botch\n", *wp);
	return call_builtin(tp->val.f, wp);
}

/*
 * Search function tables for a function.  If create set, a table entry
 * is created if none is found.
 */
struct tbl *
findfunc(name, h, create)
	char	*name;
	int	h;
	int	create;
{
	struct block *l;
	struct tbl *tp = (struct tbl *) 0;

	for (l = e->loc; l; l = l->next) {
		tp = tsearch(&l->funs, name, h);
		if (tp && (tp->flag & DEFINED))
			break;
		if (!l->next && create) {
			tp = tenter(&l->funs, name, h);
			tp->flag = DEFINED;
			tp->type = CFUNC;
			break;
		}
	}
	return tp;
}

/*
 * define function
 */
int
define(name, t)
	char	*name;
	struct op *t;
{
	register struct tbl *tp;

	tp = findfunc(name, hash(name), TRUE);

	/* If this function is currently being executed, we zap this
	 * table entry so findfunc() won't see it
	 */
	if (tp->flag & FINUSE) {
		tp->name[0] = '\0';
		tp->flag &= ~DEFINED; /* ensure it won't be found */
		tp->flag |= FDELETE;
		tp = findfunc(name, hash(name), TRUE);
	}

	if (tp->flag & ALLOC) {
		tp->flag &= ~(ISSET|ALLOC);
		tfree(tp->val.t, tp->areap);
	}

	if (t == NULL) {		/* undefine */
		tdelete(tp);
		return 0;
	}

	tp->val.t = tcopy(t, tp->areap);
	tp->flag |= (ISSET|ALLOC);

	return 0;
}

/*
 * add builtin
 */
void
builtin(name, func)
	char *name;
	int (*func)();
{
	register struct tbl *tp;
	int flag;

	/* see if any flags should be set for this builtin */
	for (flag = 0; ; name++) {
		if (*name == '=')	/* command does variable assignment */
			flag |= KEEPASN;
		else if (*name == '*')	/* POSIX special builtin */
			flag |= SPEC_BI;
		else if (*name == '+')	/* POSIX regular builtin */
			flag |= REG_BI;
		else
			break;
	}

	tp = tenter(&builtins, name, hash(name));
	tp->flag = DEFINED|flag;
	tp->type = CSHELL;
	tp->val.f = func;
}

/*
 * find command
 * either function, hashed command, or built-in (in that order)
 */
struct tbl *
findcom(name, insert, autoload, justsearch)
	char	*name;
	int	insert;		/* insert if not found */
	int	autoload;	/* do autoloading? */
	int	justsearch;	/* search path, no functions/builtins */
{
	unsigned int h = hash(name);
	register struct tbl *tp = NULL;
	static struct tbl temp;

	if (strchr(name, '/') != NULL) {
		insert = 0;
		goto Search;
	}
	if (!justsearch) {
		struct tbl *tbi;
		/* POSIX says special builtins first, then functions, then
		 * POSIX regular builtins, then search path...
		 */
		tp = tbi = tsearch(&builtins, name, h);
		if (tp && !(tp->flag & SPEC_BI))
			tp = NULL;
		if (tp == NULL) {
			tp = findfunc(name, h, FALSE);
			if (tp && !(tp->flag & ISSET)) {
				char *fpath, *fname;

				if (!autoload
				    || (fpath = strval(global("FPATH"))) == null
				    || (fname = search(name, fpath, R_OK)) == 0
				    || include(fname, 0, (char **) 0) == 0
				    || (tp = findfunc(name, h, FALSE)) == 0
				    || !(tp->flag & ISSET))
					tp = NULL;
			}
		}
		/* todo: posix says non-special/non-regular builtins must
		 * be triggered by some user-controllable means like a
		 * special directory in PATH.  Requires modifications to
		 * the search() function.  Tracked aliases should be
		 * modified to allow tracking of builtin commands.
		 * This should be under control of the FPOSIX flag.
		 */
		if (tp == NULL && tbi /* && (tbi->flag & REG_BI) */ )
			tp = tbi;
		if (tp == NULL) {
			tp = tsearch(&taliases, name, h);
			if (tp != NULL && (tp->flag&ISSET)
			    && eaccess(tp->val.s, X_OK) != 0) {
				if (tp->flag & ALLOC) {
					tp->flag &= ~(ALLOC|ISSET);
					afree(tp->val.s, APERM);
				}
				tp->flag &= ~ISSET;
			}
		}
	}
  Search:
	if (!tp || (tp->type == CTALIAS && !(tp->flag&ISSET))) {
		if (!tp) {
			if (insert && !justsearch) {
				tp = tenter(&taliases, name, h);
				tp->type = CTALIAS;
			} else {
				tp = &temp;
				tp->type = CEXEC;
			}
			tp->flag = DEFINED;	/* make ~ISSET */
		}
		name = search(name, path, X_OK);
		if (name != NULL) {
			tp->val.s = strsave(name, tp == &temp ? ATEMP : APERM);
			tp->flag |= ISSET|ALLOC;
		}
	}
	return tp;
}

/*
 * flush executable commands with relative paths
 */
void
flushcom(all)
	int all;		/* just relative or all */
{
	register struct tbl *tp;

	for (twalk(&taliases); (tp = tnext()) != NULL; )
		if ((tp->flag&ISSET) && (all || tp->val.s[0] != '/')) {
			if (tp->flag&ALLOC) {
				tp->flag &= ~(ALLOC|ISSET);
				afree(tp->val.s, APERM);
			}
			tp->flag = ~ISSET;
		}
}

/*
 * search for command with PATH
 */
char *
search(name, path, mode)
	char *name, *path;
	int mode;		/* R_OK or X_OK */
{
	register char *sp, *tp;
	struct stat	buf;

	if (strchr(name, '/')) {
		/* if executable pipes come along, this will have to change */
		if (eaccess(name, mode) == 0
		    && (mode != X_OK || (stat(name, &buf) == 0
				         && S_ISREG(buf.st_mode))))
			return name;
		return NULL;
	}

	sp = path;
	while (sp != NULL) {
		tp = line;
		for (; *sp != '\0'; tp++)
			if ((*tp = *sp++) == ':') {
				--sp;
				break;
			}
		if (tp != line)
			*tp++ = '/';
		strcpy(tp, name);
		if (eaccess(line, mode) == 0
		    && (mode != X_OK || (stat(line,&buf) == 0
					 && S_ISREG(buf.st_mode))))
			return line;
		/* what should we do about EACCES? */
		if (*sp++ == '\0')
			sp = NULL;
	}
	return NULL;
}

static int
call_builtin(f, wp)
	int (*f)();
	char **wp;
{
	int rv;

	shf_reopen(1, SHF_WR, shl_stdout);
	shl_stdout_ok = 1;
	ksh_getopt_reset(&builtin_opt, GF_ERROR);
	rv = (*f)(wp);
	shf_flush(shl_stdout);
	shl_stdout_ok = 0;
	return rv;
}

/*
 * set up redirection, saving old fd's in e->savefd
 */
static void
iosetup(iop)
	register struct ioword *iop;
{
	register int u = -1;
	char *cp = iop->name;
	int do_open, UNINITIALIZED(flags);

	/* Do not save if it has already been redirected (i.e. "cat >x >y") */
	if (e->savefd[iop->unit] == 0)
		/* comexec() assumes e->savefd[fd] set for any redirections */
		e->savefd[iop->unit] = savefd(iop->unit);

	if ((iop->flag&IOTYPE) != IOHERE)
		cp = evalonestr(cp, DOTILDE|DOGLOB);

	do_open = 1;
	switch (iop->flag&IOTYPE) {
	  case IOREAD:
		flags = O_RDONLY;
		break;

	  case IOCAT:
		flags = O_WRONLY | O_APPEND | O_CREAT;
		break;

	  case IOWRITE:
		flags = O_WRONLY | O_CREAT | O_TRUNC;
		if (Flag(FNOCLOBBER) && !(iop->flag & IOCLOB))
			flags |= O_EXCL;
		break;

	  case IORDWR:
		flags = O_RDWR | O_CREAT;
		break;

	  case IOHERE:
		do_open = 0;
		u = herein(cp, iop->flag&IOEVAL);
		/* cp may have wrong name */
		break;

	  case IODUP:
		do_open = 0;
		if (*cp == '-')
			close(u = iop->unit);
		else if (digit(*cp) && !cp[1])
			u = dup2(*cp - '0', iop->unit);
		else
			errorf("%s: illegal >& argument\n", cp);
		break;
	}
	if (do_open)
		u = open(cp, flags, 0666);
	if (u < 0)
		errorf("%s: cannot %s\n", cp,
		       (iop->flag&IOTYPE) == IODUP ? "dup"
			: (((iop->flag&IOTYPE) == IOREAD
			     || (iop->flag&IOTYPE) == IOHERE) ? "open"
			    : "create"));
	if (u != iop->unit) {
		(void) dup2(u, iop->unit);
		if (iop->flag != IODUP) /* always true */
			close(u);
	}
	if (u == 2) /* Clear any write errors */
		shf_reopen(2, SHF_WR, shl_out);
}

/*
 * open here document temp file.
 * if unquoted here, expand here temp file into second temp file.
 */
static int
herein(hname, sub)
	char *hname;
	int sub;
{
	int fd;

	if (sub) {
		char *cp;
		struct source *s, *volatile osource = source;
		struct temp *h;
		struct shf *volatile shf;
		int i;

		/* must be before newenv() 'cause shf uses ATEMP */
		shf = shf_open(hname, O_RDONLY, 0, SHF_MAPHI|SHF_CLEXEC);
		if (shf == NULL)
			return -1;
		newenv(E_ERRH);
		if ((i = setjmp(e->jbuf))) {
			if (shf)
				shf_close(shf);
			source = osource;
			quitenv(); /* after shf_close() due to alloc */
			unwind(i);
		}
		/* set up yylex input from here file */
		s = pushs(SFILE);
		s->u.shf = shf;
		source = s;
		if (yylex(ONEWORD) != LWORD)
			errorf("internal error: herein/yylex\n");
		shf_close(shf);
		shf = (struct shf *) 0;
		cp = evalstr(yylval.cp, 0);

		/* write expanded input to another temp file */
		h = maketemp(ATEMP);
		h->next = e->temps; e->temps = h;
		shf = shf_open(h->name, O_WRONLY|O_CREAT|O_TRUNC, 0666, 0);
		if (shf == NULL
		    || (fd = open(h->name, O_RDONLY, 0)) < 0)
			errorf("%s: %s\n", h->name, strerror(errno));
		shf_write(cp, strlen(cp), shf);
		if (shf_close(shf) == EOF) {
			close(fd);
			shf = (struct shf *) 0;
			errorf("error writing %s: %s\n", h->name,
				strerror(errno));
		}
		shf = (struct shf *) 0;

		quitenv();
	} else {
		fd = open(hname, O_RDONLY, 0);
		if (fd < 0)
			return -1;
	}

	return fd;
}

static void
echo(vp, ap)
	register char **vp, **ap;
{
	shellf("+");
	while (*vp != NULL)
		shellf(" %s", *vp++);
	while (*ap != NULL)
		shellf(" %s", *ap++);
	shellf("\n");
	shf_flush(shl_out);
}

/*
 *	ksh special - the select command processing section
 *	print the args in column form - assuming that we can
 */
#define	COLARGS		20

static char *
do_selectargs(ap)
	register char **ap;
{
	extern int c_read ARGS((char **wp));

	static char *read_args[] = {
					"read", "-r", "REPLY", (char *) 0
				    };
	char *s;
	int i, UNINITIALIZED(argct);
	int pmenu = 1;

	while (1) {
		if (pmenu) {
			pmenu = 0;
			argct = pr_menu(ap);
		}
		shellf("%s", strval(global("PS3")));
		shf_flush(shl_out);
		if (call_builtin(c_read, read_args) != 0) {
			shellf("\n");
			shf_flush(shl_out);
			return (char *) 0;
		}
		s = strval(global("REPLY"));
		while (*s && ctype(C_IFS, *s))
			s++;
		if (*s) {
			i = atoi(s);
			return (i >= 1 && i <= argct) ? ap[i - 1] : null;
		}
		pmenu = 1;
	}
}

/*
 *	print a select style menu
 */
static int
pr_menu(ap)
	register char **ap;
{
	register char **pp;
	register i, j;
	register int ix;
	int argct;
	int nwidth, dwidth;
	int ncols, nrows;

	/* Width/column calculations were done once and saved, but this
	 * means select can't be used recursively so we re-calculate each
	 * time (could save in a structure that is returned, but its probably
	 * not worth the bother).
	 */

	/*
	 * get dimensions of the list
	 */
	for (argct = 0, nwidth = 0, pp = ap; *pp; argct++, pp++) {
		i = strlen(*pp);
		nwidth = (i > nwidth) ? i : nwidth;
	}
	/*
	 * we will print an index of the form
	 *	%d)
	 * in front of each entry
	 * get the max width of this
	 */
	for (i = argct, dwidth = 1; i >= 10; i /= 10)
		dwidth++;

	/*
	if (argct < COLARGS)
		ncols = 1, nrows = argct;
	else */ {
		/* x_cols-1 cause some terminals wrap at end of line */
		ncols = (x_cols - 1)/(nwidth+dwidth+3);
		nrows = argct / ncols;
		if (argct % ncols)
			nrows++;
		if (ncols > nrows) {
			i = nrows, nrows = ncols, ncols = i;
			if (nrows > argct)
				nrows = argct;
		}
	}

	/*
	 * display the menu
	 */
	for (i = 0; i < nrows; i++) {
		for (j = 0; j < ncols; j++) {
			ix = j*nrows + i;
			if (ix < argct)
				shellf("%*d) %-*.*s ",
					dwidth, ix+1, nwidth, nwidth, ap[ix]);
		}
		shellf("\n");
	}
	return argct;
}
