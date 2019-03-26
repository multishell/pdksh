/*
 * lexical analysis and source input
 */

#if !defined(lint) && !defined(no_RCSids)
static char *RCSid = "$Id: lex.c,v 1.4 1994/05/31 13:34:34 michael Exp $";
#endif

#include "sh.h"
#include "expand.h"
#include <ctype.h>

static void	readhere ARGS((struct ioword *iop));
static int	getsc_ ARGS((void));
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
	int UNINITIALIZED(nparen), UNINITIALIZED(ndparen);
	int UNINITIALIZED(indquotes);


  Again:
	Xinit(ws, wp, 64);

	if (cf&ONEWORD)
		istate = SWORD;
	else if (cf&LETEXPR) {
		*wp++ = OQUOTE;	 /* enclose arguments in (double) quotes */
		istate = SDPAREN;	
		ndparen = 0;
	} else {			/* normal lexing */
		istate = SBASE;
		while ((c = getsc()) == ' ' || c == '\t')
			;
		if (c == '#')
			while ((c = getsc()) != 0 && c != '\n')
				;
		ungetsc();
	}
	if (source->flags & SF_ALIAS) {	/* trailing ' ' in alias definition */
		source->flags &= ~SF_ALIAS;
		cf |= ALIAS;
	}

	/* collect non-special or quoted characters to form word */
	for (*statep = state = istate;
	     !((c = getsc()) == 0 || (state == SBASE && ctype(c, C_LEX1))); ) {
		Xcheck(ws, wp);
		switch (state) {
		  case SBASE:
		  Sbase:
			switch (c) {
			  case '\\':
				c = getsc();
				if (c != '\n')
					*wp++ = QCHAR, *wp++ = c;
				else
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
						*wp++ = COMSUB;
					}
				} else if (c == '{') /*}*/ {
					*++statep = state = SBRACE;
					*wp++ = OSUBST;
					c = getsc();
					/* Let ${[...]} be delt with later */
					if (c != '[')
						do {
							Xcheck(ws, wp);
							*wp++ = c;
							c = getsc();
						} while (letnum(c));
					/*
					 * Append an array reference 
					 * afterwards.
					 */
					if (c == '[') {
						char *tmp, *p;
						ungetsc();
						if (!arraysub(&tmp))
							yyerror("missing ]\n");
						for (p = tmp; *p; ) {
							Xcheck(ws, wp);
							*wp++ = *p++;
						}
						afree(tmp, ATEMP);
						c = getsc();
					}
					*wp++ = 0;
					/* todo: more compile-time checking */
					/*{*/
					if (c == '}')
						ungetsc();
					else if (c == '#' || c == '%') {
						/* Korn pattern trimming */
						if (getsc() == c)
							c |= 0x80;
						else
							ungetsc();
						*wp++ = c;
					} else if (c == ':')
						*wp++ = 0x80|getsc();
					else
						*wp++ = c;
				} else if (ctype(c, C_ALPHA)) {
					*wp++ = OSUBST;
					do {
						Xcheck(ws, wp);
						*wp++ = c;
						c = getsc();
					} while (ctype(c, C_ALPHA|C_DIGIT));
					*wp++ = 0;
					*wp++ = CSUBST;
					ungetsc();
				} else if (ctype(c, C_DIGIT|C_VAR1)) {
					Xcheck(ws, wp);
					*wp++ = OSUBST;
					*wp++ = c;
					*wp++ = 0;
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
						Source *scur;

						scur = source;
						source = pushs(SREREAD);
						source->str = tmp;
						source->u.start = tmp;
						source->next = scur;

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
			if (c == '(') 
				nparen++;
			else if (c == ')')
				nparen--;
			if (nparen == 0) {
				state = *--statep;
				*wp++ = 0; /* end of COMSUB */
			} else
				*wp++ = c;
			break;

		  case SDDPAREN: /* $(( .. )) */
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
		}
	}
Done:
	Xcheck(ws, wp);
	if (state != istate)
		yyerror("no closing quote\n");

	if (c == '<' || c == '>') {
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
			if (getsc() == c)
				c = (c == ';') ? BREAK :
				    (c == '|') ? LOGOR :
				    (c == '&') ? LOGAND :
				    YYERRCODE;
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
			if ((c2 == '>' || c2 == '<') && (c == c2 || c == '<')) {
				iop->flag = c != c2 ? IORDWR : c == '>' ? IOCAT : IOHERE;
				c2 = getsc();
			} else
				iop->flag = c == '>' ? IOWRITE : IOREAD;

			if (iop->flag == IOHERE)
				if (c2 == '-')
					iop->flag |= IOSKIP;
				else
					ungetsc();
			else
				if (c2 == '&')
					iop->flag = IODUP;
				else if (c2 == '|' && iop->flag == IOWRITE)
					iop->flag |= IOCLOB;
				else
					ungetsc();
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
		*ident = 0;	/* word is not unquoted */

	if (*ident != 0 && (cf&(KEYWORD|ALIAS))) {
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
			s = pushs(SALIAS);
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

	eof = evalstr(iop->name, 0);

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
		while ((c = getsc()) != '\n') {
			if (c == 0)
				yyerror("here document `%s' unclosed\n", eof);
			if (skiptabs) {
				if (c == '\t')
					continue;
				skiptabs = 0;
			}
			if (*eofp == '\0' || c != *eofp)
				break;
			eofp++;
		}
		ungetsc();
		if (c == 0 || (c == '\n' && *eofp == '\0'))
			break;
		shf_write(eof, eofp - eof, shf);
		while ((c = getsc()) != '\n') {
			if (c == 0)
				yyerror("here document `%s' unclosed\n", eof);
			shf_putc(c, shf);
		}
		shf_putc(c, shf);
	}
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

	SH_VA_START(va, fmt);
	if (source->file != NULL) {
		shellf("%s[%d]: ", source->file,
			source->errline > 0 ? source->errline : source->line);
		source->errline = 0;
	}
	shf_vfprintf(shl_out, fmt, va);
	va_end(va);
	errorf((char *) 0);
}

/*
 * input for yylex with alias expansion
 */

Source *
pushs(type)
	int type;
{
	register Source *s;

	s = (Source *) alloc(sizeof(Source), ATEMP);
	s->type = type;
	s->str = null;
	s->line = 0;
	s->errline = 0;
	s->file = NULL;
	s->flags = 0;
	s->next = NULL;
	return s;
}

static int
getsc_()
{
	register Source *s = source;
	register int c;
	extern void	mprint();

	while ((c = *s->str++) == 0) {
		s->str = NULL;		/* return 0 for EOF by default */
		switch (s->type) {
		  case SEOF:
			s->str = null;
			return 0;

		  case STTY:
			if (histpush < 0) {	/* commands pushed by dofc */
				s->type = SHIST;
				s->str = null;
				continue;
			}
			s->str = line;
			line[0] = '\0';
			mprint(); /* print mail messages */
			pprompt(prompt);
			if (ksh_tmout) {
				ksh_tmout_state = TMOUT_READING;
				alarm(ksh_tmout);
			}
#ifdef EDIT
			if (Flag(FVI) || Flag(FEMACS) || Flag(FGMACS))
				c = x_read(line, LINE);
			else
#endif
			{
				/*
				 * This allows the arival of a SIGCHLD 
				 * to not disturb us until we are ready. 
				 */
				while ((c = read(0, line, LINE)) < 0
					&& errno == EINTR)
					intrcheck();
			}
			if (ksh_tmout) {
				ksh_tmout_state = TMOUT_EXECUTING;
				alarm(0);
			}
			if (c < 0)	/* read error */
				c = 0;
			line[c] = '\0';
			if (c == 0) /* EOF */
				s->str = NULL;
			else {
				c = 0;
				while (line[c] && ctype(line[c], C_IFS)
					       && ctype(line[c], C_IFSWS))
					c++;
				if (line[c]) {
#ifdef EASY_HISTORY
					if (cur_prompt != PS2) {
						s->line++;
						histsave(line);
					} else
						histappend(line, 1);
#else
					s->line++;
					histsave(s->line, s->str, 1);
#endif
				}
			}
			set_prompt(PS2);
			break;

		  case SHIST:
			if (histpush == 0) {
				s->type = STTY;
				s->str = null;
				continue;
			}
			s->str = histptr[++histpush];
#if 0 /* PS9 no longer in att ksh */
			pprompt("!< ");	/* PS9 */
#endif
			s->line++;
			shellf("%s\n", s->str);
			shf_flush(shl_out);
			strcpy(line, s->str);
			s->str = strchr(line, 0);
			*s->str++ = '\n';
			*s->str = 0;
			s->str = line;
			break;

		  case SFILE:
			s->line++;
			s->str = shf_gets(line, LINE, s->u.shf);
			if (s->str == NULL)
				shf_fdclose(s->u.shf);
			break;

		  case SWSTR:
			break;

		  case SSTRING:
			s->str = "\n";
			s->type = SEOF;
			break;

		  case SWORDS:
			s->str = *s->u.strv++;
			s->type = SWORDSEP;
			break;

		  case SWORDSEP:
			if (*s->u.strv == NULL) {
				s->str = "\n";
				s->type = SEOF;
			} else {
				s->str = " ";
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
				s->str = " ";
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
			shf_write(s->str, strlen(s->str), shl_out);
			shf_flush(shl_out);
		}
	}
	return c;
}

void
set_prompt(to)
	int to;
{
	cur_prompt = to;

	switch (to) {
	case PS1: /* command */
		prompt = substitute(strval(global("PS1")), 0);
		break;

	case PS2: /* command continuation */
		prompt = strval(global("PS2"));
		break;
	}
}

/* See also related routine, promptlen() in edit.c */
void
pprompt(cp)
	register char *cp;
{
	while (*cp != 0)
		if (*cp != '!')
			shf_putc(*cp++, shl_out);
		else
			if (*++cp == '!')
				shf_putc(*cp++, shl_out);
			else
				shellf("%d", source->line + 1);
	shf_flush(shl_out);
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

	Xinit(ws, wp, 32);

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
