/*
 * shell parser (C version)
 */

#if !defined(lint) && !defined(no_RCSids)
static char *RCSid = "$Id: syn.c,v 1.3 1994/05/31 13:34:34 michael Exp $";
#endif

#include "sh.h"
#include "expand.h"

struct multiline_state {
	int	on;		/* set in multiline commands (\n becomes ;) */
	int	start_token;	/* token multiline is for (eg, FOR, {, etc.) */
	int	start_line;	/* line multiline command started on */
};

static void	yyparse		ARGS((void));
static struct op *pipeline	ARGS((int cf));
static struct op *andor		ARGS((void));
static struct op *c_list	ARGS((void));
static struct ioword *synio	ARGS((int cf));
static void	musthave	ARGS((int c, int cf));
static struct op *nested	ARGS((int type, int smark, int emark));
static struct op *get_command	ARGS((int cf));
static struct op *dogroup	ARGS((void));
static struct op *thenpart	ARGS((void));
static struct op *elsepart	ARGS((void));
static struct op *caselist	ARGS((void));
static struct op *casepart	ARGS((void));
static struct op *function_body	ARGS((char *name, int ksh_func));
static char **	wordlist	ARGS((void));
static struct op *block		ARGS((int type, struct op *t1, struct op *t2,
				      char **wp));
static struct op *newtp		ARGS((int type));
static void	syntaxerr	ARGS((void)) GCC_FA_NORETURN;
static void	multiline_push ARGS((struct multiline_state *save, int tok));
static void	multiline_pop ARGS((struct multiline_state *saved));
static int	assign_command ARGS((char *s));
static void	db_parse	ARGS((XPtrV *argsp, XPtrV *varps));
static void	db_oaexpr	ARGS((XPtrV *argsp, XPtrV *varps));
static void	db_nexpr	ARGS((XPtrV *argsp, XPtrV *varps));
static void	db_primary	ARGS((XPtrV *argsp, XPtrV *varps));

static	struct	op	*outtree; /* yyparse output */

static struct multiline_state multiline;	/* \n changed to ; */

static	int	reject;		/* token(cf) gets symbol again */
static	int	symbol;		/* yylex value */

#define	REJECT	(reject = 1)
#define	ACCEPT	(reject = 0)
#define	token(cf) \
	((reject) ? (ACCEPT, symbol) : (symbol = yylex(cf)))
#define	tpeek(cf) \
	((reject) ? (symbol) : (REJECT, symbol = yylex(cf)))

static void
yyparse()
{
	ACCEPT;
	yynerrs = 0;
	if ((tpeek(KEYWORD|ALIAS|VARASN)) == 0) /* EOF */
		outtree = newtp(TEOF);
	else {
		outtree = c_list();
		musthave('\n', 0);
	}
}

static struct op *
pipeline(cf)
	int cf;
{
	register struct op *t, *p, *tl = NULL;

	t = get_command(cf);
	if (t != NULL) {
		while (token(0) == '|') {
			if ((p = get_command(CONTIN)) == NULL)
				syntaxerr();
			if (tl == NULL)
				t = tl = block(TPIPE, t, p, NOWORDS);
			else
				tl = tl->right = block(TPIPE, tl->right, p, NOWORDS);
		}
		REJECT;
	}
	return (t);
}

static struct op *
andor()
{
	register struct op *t, *p;
	register int c;

	t = pipeline(0);
	if (t != NULL) {
		while ((c = token(0)) == LOGAND || c == LOGOR) {
			if ((p = pipeline(CONTIN)) == NULL)
				syntaxerr();
			t = block(c == LOGAND? TAND: TOR, t, p, NOWORDS);
		}
		REJECT;
	}
	return (t);
}

static struct op *
c_list()
{
	register struct op *t, *p, *tl = NULL;
	register int c;

	t = andor();
	if (t != NULL) {
		while ((c = token(0)) == ';' || c == '&' ||
		       (c == '\n' && (multiline.on || source->type == SSTRING
				      || source->type == SALIAS)))
		{
			if (c == '&') {
				if (tl)
					tl->right = block(TASYNC, tl->right, NOBLOCK, NOWORDS);
				else
					t = block(TASYNC, t, NOBLOCK, NOWORDS);
			}
			if ((p = andor()) == NULL)
				return (t);
			if (tl == NULL)
				t = tl = block(TLIST, t, p, NOWORDS);
			else
				tl = tl->right = block(TLIST, tl->right, p, NOWORDS);
		}
		REJECT;
	}
	return (t);
}

static struct ioword *
synio(cf)
	int cf;
{
	register struct ioword *iop;

	if (tpeek(cf) != REDIR)
		return NULL;
	ACCEPT;
	iop = yylval.iop;
	musthave(LWORD, 0);
	iop->name = yylval.cp;
	if ((iop->flag&IOTYPE) == IOHERE) {
		if (*ident != 0) /* unquoted */
			iop->flag |= IOEVAL;
		if (herep >= &heres[HERES])
			yyerror("too many <<'s\n");
		*herep++ = iop;
	}
	return iop;
}

static void
musthave(c, cf)
	int c, cf;
{
	if ((token(cf)) != c)
		syntaxerr();
}

static struct op *
nested(type, smark, emark)
	int type, smark, emark;
{
	register struct op *t;
	struct multiline_state old_multiline;

	multiline_push(&old_multiline, smark);
	t = c_list();
	musthave(emark, KEYWORD|ALIAS);
	multiline_pop(&old_multiline);
	return (block(type, t, NOBLOCK, NOWORDS));
}

static struct op *
get_command(cf)
	int cf;
{
	register struct op *t;
	register int c, iopn = 0, syniocf;
	struct ioword *iop, **iops;
	XPtrV args, vars;
	struct multiline_state old_multiline;

	iops = (struct ioword **) alloc(sizeofN(struct ioword *, NUFILE+1),
					ATEMP);
	XPinit(args, 16);
	XPinit(vars, 16);

	if (multiline.on)
		cf = CONTIN;
	syniocf = KEYWORD|ALIAS;
	switch (c = token(cf|KEYWORD|ALIAS|VARASN)) {
	  case 0:
		syntaxerr();

	  default:
		REJECT;
		return NULL; /* empty line */

	  case LWORD:
	  case REDIR:
		REJECT;
		syniocf &= ~(KEYWORD|ALIAS);
		t = newtp(TCOM);
		while (1) {
			cf = (t->evalflags ? ARRAYVAR : 0)
			     | (XPsize(args) == 0 ? ALIAS|VARASN : 0);
			switch (tpeek(cf)) {
			  case REDIR:
				if (iopn >= NUFILE)
					yyerror("too many redirections\n");
				iops[iopn++] = synio(cf);
				break;

			  case LWORD:
				ACCEPT;
				/* the iopn == 0 and XPsize(vars) == 0 are
				 * dubious but at&t ksh acts this way
				 */
				if (iopn == 0 && XPsize(vars) == 0
				    && XPsize(args) == 0
				    && assign_command(ident))
					t->evalflags = DOASNTILDE;
				if ((XPsize(args) == 0 || Flag(FKEYWORD))
				    && is_wdvarassign(yylval.cp))
					XPput(vars, yylval.cp);
				else
					XPput(args, yylval.cp);
				break;

			  case '(':
				/* Check for "> foo (echo hi)", which at&t ksh
				 * allows (not POSIX, but not disallowed)
				 */
				afree(t, ATEMP);
				if (XPsize(args) == 0 && XPsize(vars) != 0) {
					ACCEPT;
					goto Subshell;
				}
				/* Must be a function */
				if (iopn != 0 || XPsize(args) != 1
				    || XPsize(vars) != 0)
					syntaxerr();
				ACCEPT;
				/*(*/
				musthave(')', 0);
				t = function_body(XPptrv(args)[0], FALSE);
				goto Leave;

			  default:
				goto Leave;
			}
		}
	  Leave:
		break;

	  Subshell:
	  case '(':
		t = nested(TPAREN, '(', ')');
		break;

	  case '{': /*}*/
		t = nested(TBRACE, '{', '}');
		break;

	  case MDPAREN:
	  {
		static const char let_cmd[] = { CHAR, 'l', CHAR, 'e',
						CHAR, 't', EOS };
		syniocf &= ~(KEYWORD|ALIAS);
		t = newtp(TCOM);
		ACCEPT;
		XPput(args, wdcopy(let_cmd, ATEMP));
		musthave(LWORD,LETEXPR);
		XPput(args, yylval.cp);
		break;
	  }

	  case DBRACKET: /* [[ .. ]] */
		syniocf &= ~(KEYWORD|ALIAS);
		t = newtp(TDBRACKET);
		ACCEPT;
		db_parse(&args, &vars);
		break;

	  case FOR:
	  case SELECT:
		t = newtp((c == FOR) ? TFOR : TSELECT);
		musthave(LWORD, ARRAYVAR);
		if (!is_wdvarname(yylval.cp, TRUE))
			yyerror("%s: bad identifier\n",
				c == FOR ? "for" : "select");
		t->str = strsave(ident, ATEMP);
		multiline_push(&old_multiline, c);
		t->vars = wordlist();
		t->left = dogroup();
		multiline_pop(&old_multiline);
		break;

	  case WHILE:
	  case UNTIL:
		multiline_push(&old_multiline, c);
		t = newtp((c == WHILE) ? TWHILE: TUNTIL);
		t->left = c_list();
		t->right = dogroup();
		multiline_pop(&old_multiline);
		break;

	  case CASE:
		t = newtp(TCASE);
		musthave(LWORD, 0);
		t->str = yylval.cp;
		multiline_push(&old_multiline, c);
		musthave(IN, CONTIN|KEYWORD|ALIAS);
		t->left = caselist();
		musthave(ESAC, KEYWORD|ALIAS);
		multiline_pop(&old_multiline);
		break;

	  case IF:
		multiline_push(&old_multiline, c);
		t = newtp(TIF);
		t->left = c_list();
		t->right = thenpart();
		musthave(FI, KEYWORD|ALIAS);
		multiline_pop(&old_multiline);
		break;

	  case BANG:
		syniocf &= ~(KEYWORD|ALIAS);
		t = pipeline(0);
		if (t == (struct op *) 0)
			syntaxerr();
		t = block(TBANG, NOBLOCK, t, NOWORDS);
		break;

	  case TIME:
		syniocf &= ~(KEYWORD|ALIAS);
		t = pipeline(0);
		t = block(TTIME, t, NOBLOCK, NOWORDS);
		break;

	  case FUNCTION:
		musthave(LWORD, 0);
		t = function_body(yylval.cp, TRUE);
		break;
	}

	while ((iop = synio(syniocf)) != NULL) {
		if (iopn >= NUFILE)
			yyerror("too many redirections\n");
		iops[iopn++] = iop;
	}

	if (iopn == 0) {
		afree((void*) iops, ATEMP);
		t->ioact = NULL;
	} else {
		iops[iopn++] = NULL;
		iops = (struct ioword **) aresize((void*) iops,
					sizeofN(struct ioword *, iopn), ATEMP);
		t->ioact = iops;
	}

	if (t->type == TCOM || t->type == TDBRACKET) {
		XPput(args, NULL);
		t->args = (char **) XPclose(args);
		XPput(vars, NULL);
		t->vars = (char **) XPclose(vars);
	} else {
		XPfree(args);
		XPfree(vars);
	}

	return t;
}

static struct op *
dogroup()
{
	register int c;
	register struct op *list;

	c = token(CONTIN|KEYWORD|ALIAS);
	if (c != DO)
		syntaxerr();
	list = c_list();
	musthave(DONE, KEYWORD|ALIAS);
	return list;
}

static struct op *
thenpart()
{
	register struct op *t;

	musthave(THEN, KEYWORD|ALIAS);
	t = newtp(0);
	t->left = c_list();
	if (t->left == NULL)
		syntaxerr();
	t->right = elsepart();
	return (t);
}

static struct op *
elsepart()
{
	register struct op *t;

	switch (token(KEYWORD|ALIAS|VARASN)) {
	  case ELSE:
		if ((t = c_list()) == NULL)
			syntaxerr();
		return (t);

	  case ELIF:
		t = newtp(TELIF);
		t->left = c_list();
		t->right = thenpart();
		return (t);

	  default:
		REJECT;
		return NULL;
	}
}

static struct op *
caselist()
{
	register struct op *t, *tl;

	t = tl = NULL;
	while ((tpeek(CONTIN|KEYWORD|ESACONLY)) != ESAC) { /* no ALIAS here */
		struct op *tc = casepart();
		if (tl == NULL)
			t = tl = tc, tl->right = NULL;
		else
			tl->right = tc, tl = tc;
	}
	return (t);
}

static struct op *
casepart()
{
	register struct op *t;
	register int c;
	XPtrV ptns;

	XPinit(ptns, 16);
	t = newtp(TPAT);
	c = token(CONTIN|KEYWORD); /* no ALIAS here */
	if (c != '(')
		REJECT;
	do {
		musthave(LWORD, 0);
		XPput(ptns, yylval.cp);
	} while ((c = token(0)) == '|');
	REJECT;
	XPput(ptns, NULL);
	t->vars = (char **) XPclose(ptns);
	musthave(')', 0);

	t->left = c_list();
	if ((tpeek(CONTIN|KEYWORD|ALIAS)) != ESAC)
		musthave(BREAK, CONTIN|KEYWORD|ALIAS);
	return (t);
}

static struct op *
function_body(name, ksh_func)
	char *name;
	int ksh_func;	/* function foo { } vs foo() { .. } */
{
	XString xs;
	char *xp, *p;
	struct op *t;
	int old_func_parse;

	Xinit(xs, xp, 16);
	for (p = name; ; ) {
		if ((*p == EOS && Xlength(xs, xp) == 0)
		    || (*p != EOS && *p != CHAR && *p != QCHAR
			&& *p != OQUOTE && *p != CQUOTE))
		{
			p = snptreef((char *) 0, 32, "%S", name);
			yyerror("%s: invalid function name\n", p);
		}
		Xcheck(xs, xp);
		if (*p == EOS) {
			Xput(xs, xp, '\0');
			break;
		} else if (*p == CHAR || *p == QCHAR) {
			Xput(xs, xp, p[1]);
			p += 2;
		} else
			p++;	/* OQUOTE/CQUOTE */
	}
	t = newtp(TFUNCT);
	t->str = Xclose(xs, xp);

	/* Note that POSIX allows only compound statements after foo(), sh and
	 * at&t ksh allow any command, go with the later since it shouldn't
	 * break anything.  However, for function foo, at&t ksh only accepts
	 * an open-brace.
	 */
	if (ksh_func) {
		musthave('{', CONTIN|KEYWORD|ALIAS); /* } */
		REJECT;
	}

	old_func_parse = e->flags & EF_FUNC_PARSE;
	e->flags |= EF_FUNC_PARSE;
	if ((t->left = get_command(CONTIN)) == (struct op *) 0) {
		/* create empty command so foo(): will work */
		t->left = newtp(TCOM);
		t->args = (char **) alloc(sizeof(char *), ATEMP);
		t->args[0] = (char *) 0;
		t->vars = (char **) alloc(sizeof(char *), ATEMP);
		t->vars[0] = (char *) 0;
	}
	if (!old_func_parse)
		e->flags &= ~EF_FUNC_PARSE;

	return t;
}

static char **
wordlist()
{
	register int c;
	XPtrV args;

	XPinit(args, 16);
	if ((c = token(CONTIN|KEYWORD|ALIAS)) != IN) {
		REJECT;
		return NULL;
	}
	while ((c = token(0)) == LWORD)
		XPput(args, yylval.cp);
	if (c != '\n' && c != ';')
		syntaxerr();
	if (XPsize(args) == 0) {
		XPfree(args);
		return NULL;
	} else {
		XPput(args, NULL);
		return (char **) XPclose(args);
	}
}

/*
 * supporting functions
 */

static struct op *
block(type, t1, t2, wp)
	int type;
	struct op *t1, *t2;
	char **wp;
{
	register struct op *t;

	t = newtp(type);
	t->left = t1;
	t->right = t2;
	t->vars = wp;
	return (t);
}

const	struct tokeninfo {
	char	*name;
	short	val;
	short	reserved;
} tokentab[] = {
	/* Reserved words */
	{ "if",		IF,	TRUE },
	{ "then",	THEN,	TRUE },
	{ "else",	ELSE,	TRUE },
	{ "elif",	ELIF,	TRUE },
	{ "fi",		FI,	TRUE },
	{ "case",	CASE,	TRUE },
	{ "esac",	ESAC,	TRUE },
	{ "for",	FOR,	TRUE },
	{ "select",	SELECT,	TRUE },
	{ "while",	WHILE,	TRUE },
	{ "until",	UNTIL,	TRUE },
	{ "do",		DO,	TRUE },
	{ "done",	DONE,	TRUE },
	{ "in",		IN,	TRUE },
	{ "function",	FUNCTION, TRUE },
	{ "time",	TIME,	TRUE },
	{ "{",		'{',	TRUE },
	{ "}",		'}',	TRUE },
	{ "!",		BANG,	TRUE },
	{ "[[",		DBRACKET, TRUE },
	/* Lexical tokens (0[EOF], LWORD and REDIR handled specially) */
	{ "&&",		LOGAND,	FALSE },
	{ "||",		LOGOR,	FALSE },
	{ ";;",		BREAK,	FALSE },
	{ "((",		MDPAREN, FALSE },
	/* and some special cases... */
	{ "newline",	'\n',	FALSE },
	{ 0 }
};

void
initkeywords()
{
	register struct tokeninfo const *tt;
	register struct tbl *p;

	for (tt = tokentab; tt->name; tt++) {
		if (tt->reserved) {
			p = tenter(&keywords, tt->name, hash(tt->name));
			p->flag |= DEFINED|ISSET;
			p->type = CKEYWD;
			p->val.i = tt->val;
		}
	}
}

static void
syntaxerr()
{
	char redir[6];	/* 2<<- is the longest redirection, I think */
	char *s;
	char *what = "unexpected";
	struct tokeninfo const *tt;
	int c;

	REJECT;
	c = token(0);
    Again:
	switch (c) {
	case 0:
		if (multiline.on) {
			c = multiline.start_token;
			source->errline = multiline.start_line;
			what = "unmatched";
			goto Again;
		}
		/* don't quote the EOF */
		yyerror("syntax error: unexpected EOF\n");
		/*NOTREACHED*/

	case LWORD:
		s = snptreef((char *) 0, 32, "%S", yylval.cp);
		break;

	case REDIR:
		yylval.iop->name = (char *) 0;
		snptreef(s = redir, sizeof(redir), "%R", yylval.iop);
		break;

	default:
		for (tt = tokentab; tt->name; tt++)
			if (tt->val == c)
			    break;
		if (tt->name)
			s = tt->name;
		else {
			if (c > 0 && c < 256) {
				s = redir;
				redir[0] = c;
				redir[1] = '\0';
			} else
				shf_snprintf(s = redir, sizeof(redir),
					"?%d", c);
		}
	}
	yyerror("syntax error: `%s' %s\n", s, what);
}

static void
multiline_push(save, tok)
	struct multiline_state *save;
	int tok;
{
	*save = multiline;
	multiline.on = TRUE;
	multiline.start_token = tok;
	multiline.start_line = source->line;
}

static void
multiline_pop(saved)
	struct multiline_state *saved;
{
	multiline = *saved;
}

static struct op *
newtp(type)
	int type;
{
	register struct op *t;

	t = (struct op *) alloc(sizeof(*t), ATEMP);
	t->type = type;
	t->evalflags = 0;
	t->args = t->vars = NULL;
	t->ioact = NULL;
	t->left = t->right = NULL;
	t->str = NULL;
	return (t);
}

struct op *
compile(s)
	Source *s;
{
	yynerrs = 0;
	multiline.on = FALSE;
	herep = heres;
	source = s;
	yyparse();
	if (s->type == STTY || s->type == SFILE || s->type == SHIST)
		s->str = null;	/* line is not preserved */
	return outtree;
}

/* This kludge exists to take care of sh/at&t ksh oddity in which
 * the arguments of alias/export/readonly/typeset have no field
 * splitting, file globbing, or (normal) tilde expansion done.
 * at&t ksh seems to do something similar to this since
 *	$ touch a=a; typeset a=[ab]; echo "$a"
 *	a=[ab]
 *	$ x=typeset; $x a=[ab]; echo "$a"
 *	a=a
 *	$ 
 */
static int
assign_command(s)
	char *s;
{
	char c = *s;

	if (Flag(FPOSIX) || !*s)
		return 0;
	return     (c == 'a' && strcmp(s, "alias") == 0)
		|| (c == 'e' && strcmp(s, "export") == 0)
		|| (c == 'r' && strcmp(s, "readonly") == 0)
		|| (c == 't' && strcmp(s, "typeset") == 0);
}


#define db_makevar(c)	(db_fakearg[1] = (c), wdcopy(db_fakearg, ATEMP))

/* used by db_makevar() */
static char db_fakearg[3] = { CHAR, 'x', EOS };

/*
 * Parse a [[ ]] expression, converting it to a [ .. ] style expression,
 * saving the result in t->args.
 */
static void
db_parse(argsp, varsp)
	XPtrV *argsp, *varsp;
{
	static const char db_start[] = { CHAR, '[', CHAR, '[', EOS };
	static const char db_close[] = { CHAR, ']', CHAR, ']', EOS };

	XPput(*argsp, wdcopy(db_start, ATEMP));
	XPput(*varsp, db_makevar(DB_NORM));
	db_oaexpr(argsp, varsp);
	if (tpeek(ARRAYVAR|CONTIN) != LWORD || strcmp(yylval.cp, db_close) != 0)
		syntaxerr();
	ACCEPT;
}

static void
db_oaexpr(argsp, varsp)
	XPtrV *argsp, *varsp;
{
	static const char or[] = { CHAR, '-', CHAR, 'o', EOS };
	static const char and[] = { CHAR, '-', CHAR, 'a', EOS };
	int c;

	db_nexpr(argsp, varsp);
	if ((c = tpeek(ARRAYVAR|CONTIN)) == LOGOR || c == LOGAND) {
		ACCEPT;
		XPput(*argsp, wdcopy(c == LOGOR ? or : and, ATEMP));
		XPput(*varsp, db_makevar(c == LOGOR ? DB_OR : DB_AND));
		db_oaexpr(argsp, varsp);
	}
}

static void
db_nexpr(argsp, varsp)
	XPtrV *argsp, *varsp;
{
	static const char not[] = { CHAR, '!', EOS };

	if (tpeek(ARRAYVAR|CONTIN) == LWORD && strcmp(yylval.cp, not) == 0) {
		ACCEPT;
		XPput(*argsp, yylval.cp);
		XPput(*varsp, db_makevar(DB_NORM));
		db_nexpr(argsp, varsp);
	} else
		db_primary(argsp, varsp);
}

static void
db_primary(argsp, varsp)
	XPtrV *argsp, *varsp;
{
	static const char oparen[] = { CHAR, '(', EOS };
	static const char cparen[] = { CHAR, ')', EOS };
	int c;

	c = token(ARRAYVAR|CONTIN);
	if (c == '(' /*)*/) {
		XPput(*argsp, wdcopy(oparen, ATEMP));
		XPput(*varsp, db_makevar(DB_NORM));
		db_oaexpr(argsp, varsp);
		/*(*/
		if (token(ARRAYVAR|CONTIN) != ')')
			/*(*/
			yyerror("missing )\n");
		XPput(*argsp, wdcopy(cparen, ATEMP));
		XPput(*varsp, db_makevar(DB_NORM));
	} else if (c == LWORD) {
		if (*ident && is_db_unop(ident)) {
			XPput(*argsp, yylval.cp);
			XPput(*varsp, db_makevar(DB_NORM));
			if (token(ARRAYVAR) != LWORD)
				syntaxerr();
			XPput(*argsp, yylval.cp);
			XPput(*varsp, db_makevar(DB_NORM));
		} else {
			static const char binop_marker[] =
				    { CHAR, '-', CHAR, 'B', CHAR, 'E', EOS };

			/* must be a binary operator: mark this
			 * with special -BE operator so we can't confuse
			 * the binary expression '-z = foobar' for
			 * something else. (-BE is only recognized
			 * when parsing [[ ]] expressions)
			 */
			XPput(*argsp, wdcopy(binop_marker, ATEMP));
			XPput(*varsp, db_makevar(DB_BE));
			XPput(*argsp, yylval.cp);
			XPput(*varsp, db_makevar(DB_NORM));
			c = token(ARRAYVAR);
			/* convert REDIR < and > to LWORD < and > */
			if (c == REDIR
			    && (yylval.iop->flag == IOREAD
				|| yylval.iop->flag == IOWRITE))
			{
				static const char lthan[] = { CHAR, '<', EOS };
				static const char gthan[] = { CHAR, '>', EOS };
				const char *what = yylval.iop->flag == IOREAD ?
						lthan : gthan;

				afree(yylval.iop, ATEMP);
				yylval.cp = wdcopy(what, ATEMP);
				c = symbol = LWORD;
				ident[0] = what[1];
				ident[1] = '\0';
			}
			if (c == LWORD && *ident && (c = is_db_binop(ident))) {
				XPput(*argsp, yylval.cp);
				XPput(*varsp, db_makevar(DB_NORM));
				if (token(ARRAYVAR) != LWORD)
					syntaxerr();
				XPput(*argsp, yylval.cp);
				XPput(*varsp, db_makevar(is_db_patop(c) ?
							    DB_PAT : DB_NORM));
			} else
				syntaxerr();
		}
	} else
		syntaxerr();
}
