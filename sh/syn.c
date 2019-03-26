/*
 * shell parser (C version)
 */

#ifndef lint
static char *RCSid = "$Id: syn.c,v 1.2 1992/04/25 08:33:28 sjg Exp $";
#endif

#include "stdh.h"
#include <errno.h>
#include <setjmp.h>
#include "sh.h"
#include "expand.h"

static struct op *pipeline  ARGS((int cf));
static struct op *andor     ARGS((void));
static struct op *c_list    ARGS((void));
static struct ioword *synio ARGS((int cf));
static void     musthave    ARGS((int c, int cf));
static struct op *nested    ARGS((int type, int mark));
static struct op *command   ARGS((int cf));
static struct op *dogroup   ARGS((int onlydone));
static struct op *thenpart  ARGS((void));
static struct op *elsepart  ARGS((void));
static struct op *caselist  ARGS((void));
static struct op *casepart  ARGS((void));
static char **  wordlist    ARGS((void));
static struct op *block     ARGS((int type, struct op *t1, struct op *t2, char **wp));
static struct op *newtp     ARGS((int type));
static void     zzerr       ARGS((void));

static	struct	op	*outtree; /* yyparse output */

static	int	reject;		/* token(cf) gets symbol again */
static	int	symbol;		/* yylex value */

#define	REJECT	(reject = 1)
#define	ACCEPT	(reject = 0)
#define	token(cf) \
	((reject) ? (ACCEPT, symbol) : (symbol = yylex(cf)))
#define	tpeek(cf) \
	((reject) ? (symbol) : (REJECT, symbol = yylex(cf)))

int
yyparse()
{
	ACCEPT;
	yynerrs = 0;
	if ((tpeek(KEYWORD|ALIAS)) == 0) { /* EOF */
		outtree = newtp(TEOF);
		return 0;
	}
	outtree = c_list();
	musthave('\n', 0);
	return (yynerrs != 0);
}

static struct op *
pipeline(cf)
	int cf;
{
	register struct op *t, *p, *tl = NULL;
	register int c;

	t = command(cf);
	if (t != NULL) {
		while ((c = token(0)) == '|') {
			if ((p = command(CONTIN)) == NULL)
				SYNTAXERR;
			if (tl == NULL)
				t = tl = block(TPIPE, t, p, NOWORDS);
			else
				tl = tl->right = block(TPIPE, tl->right, p, NOWORDS);
			/*t = block(TPIPE, t, p, NOWORDS);*/
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
				SYNTAXERR;
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
		       ((multiline || source->type == SSTRING
		        || (source->type == SALIAS)) && c == '\n')) {
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
			errorf("too many <<'s\n");
		*herep++ = iop;
	}
	return iop;
}

static void
musthave(c, cf)
	int c, cf;
{
	if ((token(cf)) != c)
		SYNTAXERR;
}

static struct op *
nested(type, mark)
	int type, mark;
{
	register struct op *t;

	multiline++;
	t = c_list();
	musthave(mark, KEYWORD);
	multiline--;
	return (block(type, t, NOBLOCK, NOWORDS));
}

static struct op *
command(cf)
	int cf;
{
	register struct op *t;
	register int c, iopn = 0;
	struct ioword *iop, **iops;
	XPtrV args, vars;

	iops = (struct ioword **) alloc(sizeofN(struct ioword *, NUFILE+1), ATEMP);
	XPinit(args, 16);
	XPinit(vars, 16);

	if (multiline)
		cf = CONTIN;
	cf |= KEYWORD|ALIAS;

	while ((iop = synio(cf)) != NULL) {
		if (iopn >= NUFILE)
			yyerror("too many redirections");
		iops[iopn++] = iop;
		cf &=~ CONTIN;
	}

	switch (c = token(cf)) {
	  case 0:
		yyerror("unexpected EOF");
		return NULL;

	  default:
		REJECT;
		if (iopn == 0)
			return NULL; /* empty line */
		t = newtp(TCOM);
		break;

	  case LWORD:
	  case MDPAREN:
		REJECT;
		t = newtp(TCOM);
		if (c == MDPAREN) {
			ACCEPT;
			XPput(args,"let");
			musthave(LWORD,LETEXPR);
			XPput(args,yylval.cp);
		}
		while (1)
			switch (tpeek(0)) {
			  case REDIR:
				if (iopn >= NUFILE)
					yyerror("too many redirections");
				iops[iopn++] = synio(0);
				break;

			  case LWORD:
				ACCEPT;
				if ((XPsize(args) == 0 || flag[FKEYWORD])
				    && strchr(ident+1, '='))
					{XPput(vars, yylval.cp);}
				else
					{XPput(args, yylval.cp);}
				break;

			  case MPAREN:
				ACCEPT;
				if (XPsize(args) != 1)
					SYNTAXERR;
				if (*ident == 0)
					yyerror("invalid function name\n");
				t = newtp(TFUNCT);
				t->str = strsave(ident, ATEMP);
				musthave('{', CONTIN|KEYWORD);
				t->left = nested(TBRACE, '}');
				return t;

			  default:
				goto Leave;
			}
	  Leave:
		break;

	  case '(':
		t = nested(TPAREN, ')');
		break;

	  case '{':
		t = nested(TBRACE, '}');
		break;

	  case FOR:
		t = newtp(TFOR);
		musthave(LWORD, 0);
		t->str = strsave(ident, ATEMP);
		multiline++;
		t->vars = wordlist();
		t->left = dogroup(0);
		multiline--;
		break;

	  case WHILE:
	  case UNTIL:
		multiline++;
		t = newtp((c == WHILE) ? TWHILE: TUNTIL);
		t->left = c_list();
		t->right = dogroup(1);
		multiline--;
		break;

	  case CASE:
		t = newtp(TCASE);
		musthave(LWORD, 0);
		t->str = yylval.cp;
		multiline++;
		musthave(IN, KEYWORD|CONTIN);
		t->left = caselist();
		musthave(ESAC, KEYWORD);
		multiline--;
		break;

	  case IF:
		multiline++;
		t = newtp(TIF);
		t->left = c_list();
		t->right = thenpart();
		musthave(FI, KEYWORD);
		multiline--;
		break;

	  case TIME:
		t = pipeline(CONTIN);
		t = block(TTIME, t, NOBLOCK, NOWORDS);
		break;

	  case FUNCTION:
		t = newtp(TFUNCT);
		musthave(LWORD, 0);
		t->str = strsave(ident, ATEMP);
		musthave('{', CONTIN|KEYWORD);
		t->left = nested(TBRACE, '}');
		break;

#if 0
	  case MDPAREN:
		t = newtp(TCOM);
		XPput(args, "let");
		musthave(LWORD, LETEXPR);
		XPput(args, yylval.cp);
		while (tpeek(0) == REDIR) {
			if (iopn >= NUFILE)
				yyerror("too many redirections");
			iops[iopn++] = synio(0);
		}
		break;
#endif
	}

	while ((iop = synio(0)) != NULL) {
		if (iopn >= NUFILE)
			yyerror("too many redirections");
		iops[iopn++] = iop;
	}

	if (iopn == 0) {
		afree((void*) iops, ATEMP);
		t->ioact = NULL;
	} else {
		iops[iopn++] = NULL;
		aresize((void*) iops, sizeofN(struct ioword *, iopn), ATEMP);
		t->ioact = iops;
	}

	if (t->type == TCOM) {
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
dogroup(onlydone)
	int onlydone;
{
	register int c;
	register struct op *list;

	c = token(CONTIN|KEYWORD);
	if (c == DONE && onlydone)
		return NULL;
	if (c != DO)
		SYNTAXERR;
	list = c_list();
	musthave(DONE, KEYWORD);
	return list;
}

static struct op *
thenpart()
{
	register int c;
	register struct op *t;

	if ((c = token(0)) != THEN) {
		REJECT;
		return NULL;
	}
	t = newtp(0);
	t->left = c_list();
	if (t->left == NULL)
		SYNTAXERR;
	t->right = elsepart();
	return (t);
}

static struct op *
elsepart()
{
	register int c;
	register struct op *t;

	switch (c = token(0)) {
	  case ELSE:
		if ((t = c_list()) == NULL)
			SYNTAXERR;
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
	while ((tpeek(CONTIN|KEYWORD)) != ESAC) {
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
	register int c, cf;
	XPtrV ptns;

	XPinit(ptns, 16);
	t = newtp(TPAT);
	cf = CONTIN|KEYWORD;
	c = token(cf);
	if (c != '(')
		REJECT;
	else
		cf = 0;
	do {
		musthave(LWORD, cf);
		XPput(ptns, yylval.cp);
		cf = 0;
	} while ((c = token(0)) == '|');
	REJECT;
	XPput(ptns, NULL);
	t->vars = (char **) XPclose(ptns);
	musthave(')', 0);

	t->left = c_list();
	if ((tpeek(CONTIN|KEYWORD)) != ESAC)
		musthave(BREAK, CONTIN|KEYWORD);
	return (t);
}

static char **
wordlist()
{
	register int c;
	XPtrV args;

	XPinit(args, 16);
	if ((c = token(CONTIN|KEYWORD)) != IN) {
		REJECT;
		return NULL;
	}
	while ((c = token(0)) == LWORD)
		XPput(args, yylval.cp);
	if (c != '\n' && c != ';')
		SYNTAXERR;
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

const	struct res {
	char	*name;
	int	val;
} restab[] = {
	"for",		FOR,
	"case",		CASE,
	"esac",		ESAC,
	"while",	WHILE,
	"do",		DO,
	"done",		DONE,
	"if",		IF,
	"in",		IN,
	"then",		THEN,
	"else",		ELSE,
	"elif",		ELIF,
	"until",	UNTIL,
	"fi",		FI,
	"function",	FUNCTION,
	"time",		TIME,
	"{",		'{',
	"}",		'}',
	0
};

keywords()
{
	register struct res const *rp;
	register struct tbl *p;

	for (rp = restab; rp->name; rp++) {
		p = tenter(&lexicals, rp->name, hash(rp->name));
		p->flag |= DEFINED|ISSET;
		p->type = CKEYWD;
		p->val.i = rp->val;
	}
}

static struct op *
newtp(type)
	int type;
{
	register struct op *t;

	t = (struct op *) alloc(sizeof(*t), ATEMP);
	t->type = type;
	t->args = t->vars = NULL;
	t->ioact = NULL;
	t->left = t->right = NULL;
	t->str = NULL;
	return (t);
}

static void
zzerr()
{
	yyerror("syntax error");
}

struct op *
compile(s)
	Source *s;
{
	yynerrs = 0;
	multiline = 0;
	herep = heres;
	source = s;
	if (yyparse())
		unwind();
	if (s->type == STTY || s->type == SFILE || s->type == SHIST)
		s->str = null;	/* line is not preserved */
	return outtree;
}

