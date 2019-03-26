/*
 * lexical analysis and source input
 */

#include "sh.h"
#include <ctype.h>

static void	readhere ARGS((struct ioword *iop));
static int	getsc_ ARGS((void));
static char	*get_brace_var ARGS((XString *wsp, char *wp));
static int	arraysub ARGS((char **strp));

static void gethere ARGS((void));

/* optimized getsc_() */
#define	getsc()	((*source->str != 0) ? *source->str++ : getsc_())
#define	ungetsc() (source->str != null ? source->str-- : source->str)

/*
 * Lexical analyzer
 *
 * tokens are not regular expressions, they are LL(1).
 * for example, "${var:-${PWD}}", and "$(size $(whence ksh))".
 * hence the state stack.
 */

int
yylex(cf)
	int cf;
{
	register int c, state;
	char states [64], *statep = states;
	XString ws;		/* expandable output word */
	register char *wp;	/* output word pointer */
	register char *sp, *dp;
	char UNINITIALIZED(*ddparen_start);
	int istate;
	int UNINITIALIZED(c2);
	int UNINITIALIZED(nparen), UNINITIALIZED(csstate);
	int UNINITIALIZED(ndparen);
	int UNINITIALIZED(indquotes);


  Again:
	Xinit(ws, wp, 64, ATEMP);

	if (cf&ONEWORD)
		istate = SWORD;
	else if (cf&LETEXPR) {
		*wp++ = OQUOTE;	 /* enclose arguments in (double) quotes */
		istate = SDPAREN;	
		ndparen = 0;
	} else {		/* normal lexing */
		istate = (cf & HEREDELIM) ? SHEREDELIM : SBASE;
		while ((c = getsc()) == ' ' || c == '\t')
			;
		if (c == '#')
			while ((c = getsc()) != '\0' && c != '\n')
				;
		ungetsc();
	}
	if (source->flags & SF_ALIAS) {	/* trailing ' ' in alias definition */
		source->flags &= ~SF_ALIAS;
		/* In POSIX mode, a trailing space only counts if we are
		 * parsing a simple command
		 */
		if (!Flag(FPOSIX) || (cf & CMDWORD))
			cf |= ALIAS;
	}

	/* collect non-special or quoted characters to form word */
	for (*statep = state = istate;
	     !((c = getsc()) == 0 || ((state == SBASE || state == SHEREDELIM)
				      && ctype(c, C_LEX1))); )
	{
		Xcheck(ws, wp);
		switch (state) {
		  case SBASE:
		  Sbase:
			switch (c) {
			  case '\\':
				c = getsc();
				if (c != '\n') {
#ifdef OS2
					if (isalnum(c)) {
						*wp++ = CHAR, *wp++ = '\\';
						*wp++ = CHAR, *wp++ = c;
					} else
#endif
						*wp++ = QCHAR, *wp++ = c;
				} else
					if (wp == Xstring(ws, wp)) {
						Xfree(ws, wp);	/* free word */
						goto Again;
					}
				break;
			  case '\'':
				*++statep = state = SSQUOTE;
				*wp++ = OQUOTE;
				break;
			  case '"':
				*++statep = state = SDQUOTE;
				*wp++ = OQUOTE;
				break;
			  default:
				goto Subst;
			}
			break;

		  Subst:
			switch (c) {
			  case '\\':
				c = getsc();
				switch (c) {
				  case '\n':
					break;
				  case '"': case '\\':
				  case '$': case '`':
					*wp++ = QCHAR, *wp++ = c;
					break;
				  default:
					Xcheck(ws, wp);
					*wp++ = CHAR, *wp++ = '\\';
					*wp++ = CHAR, *wp++ = c;
					break;
				}
				break;
			  case '$':
				c = getsc();
				if (c == '(') /*)*/ {
					c = getsc();
					if (c == '(') /*)*/ {
						*++statep = state = SDDPAREN;
						nparen = 2;
						ddparen_start = wp;
						*wp++ = EXPRSUB;
					} else {
						ungetsc();
						*++statep = state = SPAREN;
						nparen = 1;
						csstate = 0;
						*wp++ = COMSUB;
					}
				} else if (c == '{') /*}*/ {
					*++statep = state = SBRACE;
					*wp++ = OSUBST;
					wp = get_brace_var(&ws, wp);
				} else if (ctype(c, C_ALPHA)) {
					*wp++ = OSUBST;
					do {
						Xcheck(ws, wp);
						*wp++ = c;
						c = getsc();
					} while (ctype(c, C_ALPHA|C_DIGIT));
					*wp++ = '\0';
					*wp++ = CSUBST;
					ungetsc();
				} else if (ctype(c, C_DIGIT|C_VAR1)) {
					Xcheck(ws, wp);
					*wp++ = OSUBST;
					*wp++ = c;
					*wp++ = '\0';
					*wp++ = CSUBST;
				} else {
					*wp++ = CHAR, *wp++ = '$';
					ungetsc();
				}
				break;
			  case '`':
				*++statep = state = SBQUOTE;
				*wp++ = COMSUB;
				/* Need to know if we are inside double quotes
				 * since sh/at&t-ksh translate the \" to " in
				 * "`..\"..`".
				 */
				indquotes = 0;
				if (!Flag(FPOSIX))
					for (sp = statep; sp > states; --sp)
						if (*sp == SDQUOTE)
							indquotes = 1;
				break;
			  case '[': 
				*wp = EOS; /* temporary */
				if (state == SBASE && (cf & (VARASN|ARRAYVAR))
				    && is_wdvarname(Xstring(ws, wp), FALSE))
				{
					char *p, *tmp;

					ungetsc();
					if (arraysub(&tmp)) {
						for (p = tmp; *p; ) {
							Xcheck(ws, wp);
							*wp++ = CHAR;
							*wp++ = *p++;
						}
						afree(tmp, ATEMP);
						break;
					} else {
						Source *s;

						s = pushs(SREREAD,
							  source->areap);
						s->str = tmp;
						s->u.start = tmp;
						s->next = source;
						source = s;

						getsc(); /* '[' */
					}
				}
				*wp++ = CHAR;
				*wp++ = c;
				break;
			  default:
				*wp++ = CHAR, *wp++ = c;
			}
			break;

		  case SSQUOTE:
			if (c == '\'') {
				state = *--statep;
				*wp++ = CQUOTE;
			} else
				*wp++ = QCHAR, *wp++ = c;
			break;

		  case SDQUOTE:
			if (c == '"') {
				state = *--statep;
				*wp++ = CQUOTE;
			} else
				goto Subst;
			break;

		  case SPAREN: /* $( .. ) */
			/* todo: deal with $(...) quoting properly
			 * kludge to partly fake quoting inside $(..): doesn't
			 * really work because nested $(..) or ${..} inside
			 * double quotes aren't dealt with.
			 */
			switch (csstate) {
			  case 0: /* normal */
				switch (c) {
				  case '(':
					nparen++;
					break;
				  case ')':
					nparen--;
					break;
				  case '\\':
					csstate = 1;
					break;
				  case '"':
					csstate = 2;
					break;
				  case '\'':
					csstate = 4;
					break;
				}
				break;

			  case 1: /* backslash in normal mode */
			  case 3: /* backslash in double quotes */
				--csstate;
				break;

			  case 2: /* double quotes */
				if (c == '"')
					csstate = 0;
				else if (c == '\\')
					csstate = 3;
				break;

			  case 4: /* single quotes */
				if (c == '\'')
					csstate = 0;
				break;
			}
			if (nparen == 0) {
				state = *--statep;
				*wp++ = 0; /* end of COMSUB */
			} else
				*wp++ = c;
			break;

		  case SDDPAREN: /* $(( .. )) */
			/* todo: deal with $((...); (...)) properly */
			if (c == '(')
				nparen++;
			else if (c == ')') {
				nparen--;
				if (nparen == 1) {
					/*(*/
					if (getsc() == ')') {
						state = *--statep;
						*wp++ = 0; /* end of EXPRSUB */
						break;
					} else {
						ungetsc();
						/* mismatched parenthesis -
						 * assume we were really
						 * parsing a $(..) expression
						 */
						memmove(ddparen_start + 1,
							ddparen_start,
							wp - ddparen_start);
						*ddparen_start++ = COMSUB;
						*ddparen_start = '('; /*)*/
						wp++;
						csstate = 0;
						*statep = state = SPAREN;
					}
				}
			}
			*wp++ = c;
			break;

		  case SBRACE:
			/*{*/
			if (c == '}') {
				state = *--statep;
				*wp++ = CSUBST;
			} else
				goto Sbase;
			break;

		  case SBQUOTE:
			if (c == '`') {
				*wp++ = 0;
				state = *--statep;
			} else if (c == '\\') {
				switch (c = getsc()) {
				  case '\n':
					break;
				  case '\\':
				  case '$': case '`':
					*wp++ = c;
					break;
				  case '"':
					if (indquotes) {
						*wp++ = c;
						break;
					}
					/* fall through.. */
				  default:
					*wp++ = '\\';
					*wp++ = c;
					break;
				}
			} else
				*wp++ = c;
			break;

		  case SWORD:	/* ONEWORD */
			goto Subst;

		  case SDPAREN:	/* LETEXPR: (( ... )) */
			/*(*/
			if (c == ')') {
				if (ndparen > 0)
				    --ndparen;
				/*(*/
				else if (getsc() == ')') {
					c = 0;
					*wp++ = CQUOTE;
					goto Done;
				} else
					ungetsc();
			} else if (c == '(')
				/* parenthesis inside quotes and backslashes
				 * are lost, but at&t ksh doesn't count them
				 * either
				 */
				++ndparen;
			goto Sbase;

		  case SHEREDELIM:	/* <<,<<- delimiter */
			/* here delimiters need a special case since
			 * $ and `..` are not to be treated specially
			 */
			if (c == '\\') {
				*wp++ = QCHAR;
				*wp++ = getsc();
			} else if (c == '\'') {
				*++statep = state = SSQUOTE;
				*wp++ = OQUOTE;
			} else if (c == '"') {
				state = SHEREDQUOTE;
				*wp++ = OQUOTE;
			} else {
				*wp++ = CHAR;
				*wp++ = c;
			}
			break;

		  case SHEREDQUOTE:	/* " in <<,<<- delimiter */
			if (c == '"') {
				*wp++ = CQUOTE;
				state = SHEREDELIM;
			} else {
				*wp++ = CHAR;
				*wp++ = c == '\\' ? getsc() : c;
			}
			break;
		}
	}
Done:
	Xcheck(ws, wp);
	if (state != istate)
		yyerror("no closing quote\n");

	/* This done to avoid tests for SHEREDELIM wherever SBASE tested */
	if (state == SHEREDELIM)
		state = SBASE;

	if ((c == '<' || c == '>') && state == SBASE) {
		char *cp = Xstring(ws, wp);
		if (Xlength(ws, wp) == 2 && cp[0] == CHAR && digit(cp[1])) {
			wp = cp; /* throw away word */
			c2/*unit*/ = cp[1] - '0';
		} else
			c2/*unit*/ = c == '>'; /* 0 for <, 1 for > */
	}

	if (wp == Xstring(ws, wp) && state == SBASE) {
		Xfree(ws, wp);	/* free word */
		/* no word, process LEX1 character */
		switch (c) {
		  default:
			return c;

		  case '|':
		  case '&':
		  case ';':
			if ((c2 = getsc()) == c)
				c = (c == ';') ? BREAK :
				    (c == '|') ? LOGOR :
				    (c == '&') ? LOGAND :
				    YYERRCODE;
			else if (c == '|' && c2 == '&')
				c = COPROC;
			else
				ungetsc();
			return c;

		  case '>':
		  case '<': {
			register struct ioword *iop;

			iop = (struct ioword *) alloc(sizeof(*iop), ATEMP);
			iop->unit = c2/*unit*/;

			c2 = getsc();
			/* <<, >>, <> are ok, >< is not */
			if (c == c2 || (c == '<' && c2 == '>')) {
				iop->flag = c == c2 ?
					  (c == '>' ? IOCAT : IOHERE) : IORDWR;
				if (iop->flag == IOHERE)
					if (getsc() == '-')
						iop->flag |= IOSKIP;
					else
						ungetsc();
			} else if (c2 == '&')
				iop->flag = IODUP | (c == '<' ? IORDUP : 0);
			else {
				iop->flag = c == '>' ? IOWRITE : IOREAD;
				if (c == '>' && c2 == '|')
					iop->flag |= IOCLOB;
				else
					ungetsc();
			}

			iop->name = (char *) 0;
			iop->delim = (char *) 0;
			yylval.iop = iop;
			return REDIR;
		    }
		  case '\n':
			gethere();
			if (cf & CONTIN)
				goto Again;
			return c;

		  case '(':  /*)*/
			if (getsc() == '(') /*)*/
				c = MDPAREN;
			else
				ungetsc();
			return c;
		  /*(*/
		  case ')':
			return c;
		}
	}

	*wp++ = EOS;		/* terminate word */
	yylval.cp = Xclose(ws, wp);
	if (state == SWORD || state == SDPAREN)	/* ONEWORD? */
		return LWORD;
	ungetsc();		/* unget terminator */

	/* copy word to unprefixed string ident */
	for (sp = yylval.cp, dp = ident; dp < ident+IDENT && (c = *sp++) == CHAR; )
		*dp++ = *sp++;
	/* Make sure the ident array stays '\0' paded */
	memset(dp, 0, (ident+IDENT) - dp + 1);
	if (c != EOS)
		*ident = '\0';	/* word is not unquoted */

	if (*ident != '\0' && (cf&(KEYWORD|ALIAS))) {
		struct tbl *p;
		int h = hash(ident);

		if ((cf & KEYWORD) && (p = tsearch(&keywords, ident, h))
		    && (!(cf & ESACONLY) || p->val.i == ESAC))
		{
			afree(yylval.cp, ATEMP);
			return p->val.i;
		}
		if ((cf & ALIAS) && (p = tsearch(&aliases, ident, h))
		    && (p->flag & ISSET))
		{
			register Source *s;

			for (s = source; s->type == SALIAS; s = s->next)
				if (s->u.tblp == p)
					return LWORD;
			/* push alias expansion */
			s = pushs(SALIAS, source->areap);
			s->str = p->val.s;
			s->u.tblp = p;
			s->next = source;
			source = s;
			afree(yylval.cp, ATEMP);
			goto Again;
		}
	}

	return LWORD;
}

static void
gethere()
{
	register struct ioword **p;

	for (p = heres; p < herep; p++)
		readhere(*p);
	herep = heres;
}

/*
 * read "<<word" text into temp file
 */

static void
readhere(iop)
	register struct ioword *iop;
{
	struct shf *volatile shf;
	struct temp *h;
	register int c;
	char *volatile eof;
	char *eofp;
	int skiptabs;
	int i;

	eof = evalstr(iop->delim, 0);

	if (e->flags & EF_FUNC_PARSE) {
		h = maketemp(APERM);
		h->next = func_heredocs;
		func_heredocs = h;
	} else {
		h = maketemp(ATEMP);
		h->next = e->temps;
		e->temps = h;
	}
	iop->name = h->name;
	shf = shf_open(h->name, O_WRONLY|O_CREAT|O_TRUNC, 0666, 0);
	if (shf == NULL)
		yyerror("cannot create temporary file\n");

	newenv(E_ERRH);
	if ((i = setjmp(e->jbuf))) {
		quitenv();
		shf_close(shf);
		unwind(i);
	}

	for (;;) {
		eofp = eof;
		skiptabs = iop->flag & IOSKIP;
		while ((c = getsc()) != 0) {
			if (skiptabs) {
				if (c == '\t')
					continue;
				skiptabs = 0;
			}
			if (c != *eofp)
				break;
			eofp++;
		}
		/* Allow EOF here so commands with out trailing newlines
		 * (eg, ksh -c '...', $(...), etc) will work
		 */
		if (*eofp == '\0' && (c == 0 || c == '\n'))
			break;
		ungetsc();
		shf_write(eof, eofp - eof, shf);
		while ((c = getsc()) != '\n') {
			if (c == 0)
				yyerror("here document `%s' unclosed\n", eof);
			shf_putc(c, shf);
		}
		shf_putc(c, shf);
	}
	shf_flush(shf);
	if (shf_error(shf))
		yyerror("error saving here document `%s': %s\n",
			eof, strerror(shf_errno(shf)));
	/*XXX add similar checks for write errors everywhere */
	quitenv();
	shf_close(shf);
}

void
#ifdef HAVE_PROTOTYPES
yyerror(const char *fmt, ...)
#else
yyerror(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	va_list va;

	yynerrs++;
	/* pop aliases and re-reads */
	while (source->type == SALIAS || source->type == SREREAD)
		source = source->next;
	source->str = null;	/* zap pending input */

	error_prefix(TRUE);
	SH_VA_START(va, fmt);
	shf_vfprintf(shl_out, fmt, va);
	va_end(va);
	errorf((char *) 0);
}

/*
 * input for yylex with alias expansion
 */

Source *
pushs(type, areap)
	int type;
	Area *areap;
{
	register Source *s;

	s = (Source *) alloc(sizeof(Source), areap);
	s->type = type;
	s->str = null;
	s->line = 0;
	s->errline = 0;
	s->file = NULL;
	s->flags = 0;
	s->next = NULL;
	s->areap = areap;
	if (type == SFILE || type == SSTDIN) {
		char *dummy;
		Xinit(s->xs, dummy, 256, s->areap);
	} else
		memset(&s->xs, 0, sizeof(s->xs));
	return s;
}

static int
getsc_()
{
	register Source *s = source;
	register int c;
	static char line[LINE + 1];

	while ((c = *s->str++) == 0) {
		s->str = NULL;		/* return 0 for EOF by default */
		switch (s->type) {
		  case SEOF:
			s->str = null;
			return 0;

		  case STTY:
			s->str = line;
			line[0] = '\0';
			mprint(); /* print mail messages */
#ifdef KSH
			if (ksh_tmout) {
				ksh_tmout_state = TMOUT_READING;
				alarm(ksh_tmout);
			}
#endif /* KSH */
#ifdef EDIT
			if (0
# ifdef VI
			    || Flag(FVI)
# endif /* VI */
# ifdef EMACS
			    || Flag(FEMACS) || Flag(FGMACS)
# endif /* EMACS */
			    )
				c = x_read(line, LINE);
			else
#endif
			{
				pprompt(prompt, 0);
				/*
				 * This allows the arrival of a SIGCHLD 
				 * to not disturb us until we are ready. 
				 */
				while ((c = blocking_read(0, line, LINE)) < 0
					&& errno == EINTR)
					if (trap)
						runtraps(0);
			}

			/* XXX: temporary kludge to restore source after a
			 * trap may have been executed.
			 */
			source = s;

#ifdef KSH
			if (ksh_tmout) {
				ksh_tmout_state = TMOUT_EXECUTING;
				alarm(0);
			}
#endif /* KSH */
			if (c < 0)	/* read error */
				c = 0;
			strip_nuls(line, c);
			line[c] = '\0';
			if (c == 0) /* EOF */
				s->str = NULL;
			else {
				c = 0;
				while (line[c] && ctype(line[c], C_IFS)
					       && ctype(line[c], C_IFSWS))
					c++;
#ifdef HISTORY
				if (line[c]) {
# ifdef EASY_HISTORY
					if (cur_prompt == PS2)
						histappend(line, 1);
					else
# endif /* EASY_HISTORY */
					    {
						s->line++;
						histsave(s->line, s->str, 1);
					    }
				}
#endif /* HISTORY */
			}
			set_prompt(PS2, (Source *) 0);
			break;

		  case SFILE:
		  case SSTDIN:
		  {
			char *xp = Xstring(s->xs, xp), *p;

			s->line++;
			while (1) {
				p = shf_getse(xp, Xnleft(s->xs, xp), s->u.shf);
				if (!p || (xp = p, xp[-1] == '\n')) {
					s->str = Xlength(s->xs, xp) ?
						    Xstring(s->xs, xp) : NULL;
					strip_nuls(Xstring(s->xs, xp),
						Xlength(s->xs, xp));
					break;
				}
				/* double buffer size */
				xp++; /* move past null so doubling works... */
				XcheckN(s->xs, xp, Xlength(s->xs, xp));
				xp--; /* ...and move back again */
			}
			/* flush any unwanted input so other programs/builtins
			 * can read it.  Not very optimal, but less error prone
			 * than flushing else where, dealing with redirections,
			 * etc..
			 * todo: reduce size of shf buffer (~128?) if SSTDIN
			 */
			if (s->type == SSTDIN)
				shf_flush(s->u.shf);
			else if (s->str == NULL)
				shf_fdclose(s->u.shf);
			break;
		  }

		  case SWSTR:
			break;

		  case SSTRING:
			break;

		  case SWORDS:
			s->str = *s->u.strv++;
			s->type = SWORDSEP;
			break;

		  case SWORDSEP:
			if (*s->u.strv == NULL) {
				s->str = newline;
				s->type = SEOF;
			} else {
				s->str = space;
				s->type = SWORDS;
			}
			break;

		  case SALIAS:
			if (s->flags & SF_ALIASEND) {
				/* pass on an unused SF_ALIAS flag */
				source = s->next;
				source->flags |= s->flags & SF_ALIAS;
				s = source;
			} else if (*s->u.tblp->val.s
				 && isspace(strchr(s->u.tblp->val.s, 0)[-1]))
			{
				source = s = s->next;	/* pop source stack */
				s->flags |= SF_ALIAS;
			} else {
				/* put a fake space at the end of the alias.
				 * This keeps the current alias in the source
				 * list so recursive aliases can be detected.
				 * The addition of a space after an alias
				 * never affects anything (I think).
				 */
				s->flags |= SF_ALIASEND;
				s->str = space;
			}
			continue;

		  case SREREAD:
			afree(s->u.start, ATEMP);
			source = s = s->next;
			continue;
		}
		if (s->str == NULL) {
			s->type = SEOF;
			s->str = null;
			return 0;
		}
		if (s->flags & SF_ECHO) {
			shf_puts(s->str, shl_out);
			shf_flush(shl_out);
		}
	}
	return c;
}

void
set_prompt(to, s)
	int to;
	Source *s;
{
	cur_prompt = to;

	switch (to) {
	case PS1: /* command */
		/* Substitute ! and !! here, before substitutions are done
		 * so ! in expanded variables are not expanded.
		 * NOTE: this is not what at&t ksh does (it does it after
		 * substitutions, POSIX doesn't say which is to be done.
		 */
		{
			struct shf *shf;
			char *ps1;
			Area *saved_atemp;

			ps1 = strval(global("PS1"));
			shf = shf_sopen((char *) 0, strlen(ps1),
				SHF_WR | SHF_DYNAMIC, (struct shf *) 0);
			while (*ps1) {
				if (*ps1 != '!' || *++ps1 == '!')
					shf_putchar(*ps1++, shf);
				else
					shf_fprintf(shf, "%d",
						s ? s->line + 1 : 0);
			}
			ps1 = shf_sclose(shf);
			saved_atemp = ATEMP;
			newenv(E_ERRH);
			if (setjmp(e->jbuf)) {
				prompt = safe_prompt;
				warningf(TRUE, "error during expansion of PS1");
			} else
				prompt = strsave(substitute(ps1, 0),
						 saved_atemp);
			quitenv();
		}
		break;

	case PS2: /* command continuation */
		prompt = strval(global("PS2"));
		break;
	}
}

/* See also related routine, promptlen() in edit.c */
void
pprompt(cp, ntruncate)
	register char *cp;
	int ntruncate;
{
#if 0
	char nbuf[32];
	int c;

	while (*cp != 0) {
		if (*cp != '!')
			c = *cp++;
		else if (*++cp == '!')
			c = *cp++;
		else {
			int len;
			char *p;

			shf_snprintf(p = nbuf, sizeof(nbuf), "%d",
				source->line + 1);
			len = strlen(nbuf);
			if (ntruncate) {
				if (ntruncate >= len) {
					ntruncate -= len;
					continue;
				}
				p += ntruncate;
				len -= ntruncate;
				ntruncate = 0;
			}
			shf_write(p, len, shl_out);
			continue;
		}
		if (ntruncate)
			--ntruncate;
		else
			shf_putc(c, shl_out);
	}
#endif /* 0 */
	if (ntruncate)
		shellf("%.*s", ntruncate, cp);
	else {
		shf_puts(cp, shl_out);
		shf_flush(shl_out);
	}
}

/* Read the variable part of a ${...} expression (ie, up to but not including
 * the :[-+?=#%] or close-brace.
 */
static char *
get_brace_var(wsp, wp)
	XString *wsp;
	char *wp;
{
	enum parse_state {
			   PS_INITIAL, PS_SAW_HASH, PS_IDENT,
			   PS_NUMBER, PS_VAR1, PS_END
			 }
		state;
	char c;

	state = PS_INITIAL;
	while (1) {
		c = getsc();
		/* State machine to figure out where the variable part ends. */
		switch (state) {
		  case PS_INITIAL:
			if (c == '#') {
				state = PS_SAW_HASH;
				break;
			}
			/* fall through.. */
		  case PS_SAW_HASH:
			if (letter(c))
				state = PS_IDENT;
			else if (digit(c))
				state = PS_NUMBER;
			else if (ctype(c, C_VAR1))
				state = PS_VAR1;
			else
				state = PS_END;
			break;
		  case PS_IDENT:
			if (!letnum(c)) {
				state = PS_END;
				if (c == '[') {
					char *tmp, *p;

					ungetsc();
					if (!arraysub(&tmp))
						yyerror("missing ]\n");
					for (p = tmp; *p; ) {
						Xcheck(*wsp, wp);
						*wp++ = *p++;
					}
					afree(tmp, ATEMP);
					c = getsc();
				}
			}
			break;
		  case PS_NUMBER:
			if (!digit(c))
				state = PS_END;
			break;
		  case PS_VAR1:
			state = PS_END;
			break;
		  case PS_END: /* keep gcc happy */
			break;
		}
		if (state == PS_END) {
			*wp++ = '\0';	/* end of variable part */
			ungetsc();
			break;
		}
		Xcheck(*wsp, wp);
		*wp++ = c;
	}
	return wp;
}

/*
 * Save an array subscript - returns true if matching bracket found, false
 * if eof or newline was found.
 * (Returned string double null terminated)
 */
static int
arraysub(strp)
	char **strp;
{
	XString ws;
	char	*wp;
	char	c;
	int 	depth = 0;

	Xinit(ws, wp, 32, ATEMP);

	do {
		c = getsc();
		Xcheck(ws, wp);
		*wp++ = c;
		if (c == '[')
			depth++;
		else if (c == ']')
			depth--;
	} while (depth > 0 && c && c != '\n');

	*wp++ = '\0';
	*strp = Xclose(ws, wp);

	return depth == 0 ? 1 : 0;
}
