/*
 * command tree climbing
 */

#if !defined(lint) && !defined(no_RCSids)
static char *RCSid = "$Id: tree.c,v 1.3 1994/05/31 13:34:34 michael Exp $";
#endif

#include "sh.h"

#define tputc(c, shf)	shf_putchar(c, shf);
static	void	tputC ARGS((int c, struct shf *shf));
static	void	tputS ARGS((char *wp, struct shf *shf));
static	void	vfptreef ARGS((struct shf *shf, char *fmt, va_list va));
static	struct ioword **iocopy ARGS((struct ioword **iow, Area *ap));
static	void     iofree ARGS((struct ioword **iow, Area *ap));

/*
 * print a command tree
 */

void
ptree(t, shf)
	register struct op *t;
	register struct shf *shf;
{
	register char **w;
	struct ioword **ioact;
	struct op *t1;

 Chain:
	if (t == NULL)
		return;
	switch (t->type) {
	  case TCOM:
		if (t->vars)
			for (w = t->vars; *w != NULL; )
				fptreef(shf, "%S ", *w++);
		else
			fptreef(shf, "#no-vars# ");
		if (t->args)
			for (w = t->args; *w != NULL; )
				fptreef(shf, "%S ", *w++);
		else
			fptreef(shf, "#no-args# ");
		break;
	  case TEXEC:
		t = t->left;
		goto Chain;
	  case TPAREN:
		fptreef(shf, "(%T)", t->left);
		break;
	  case TPIPE:
		fptreef(shf, "%T | ", t->left);
		t = t->right;
		goto Chain;
	  case TLIST:
		fptreef(shf, "%T%;", t->left);
		t = t->right;
		goto Chain;
	  case TOR:
	  case TAND:
		fptreef(shf, "%T %s %T",
			t->left, (t->type==TOR) ? "||" : "&&", t->right);
		break;
	  case TBANG:
		fptreef(shf, "! ");
		t = t->right;
		goto Chain;
	  case TDBRACKET:
	  {
		int i;

		/* the opening [[ is in args[0] */
		for (i = 0; t->args[i]; i++)
			switch (t->vars[i][1]) {
			  case DB_NORM:
				fptreef(shf, "%S ", t->args[i]);
				break;
			  case DB_OR:
				fptreef(shf, "|| ");
				break;
			  case DB_AND:
				fptreef(shf, "&& ");
			  case DB_BE:
				/* print nothing */
				break;
			}
		fptreef(shf, "]] ");
		break;
	  }
	  case TFOR:
		fptreef(shf, "for %s ", t->str);
		if (t->vars != NULL) {
			fptreef(shf, "in ");
			for (w = t->vars; *w; )
				fptreef(shf, "%S ", *w++);
			fptreef(shf, "%;");
		}
		fptreef(shf, "do %T%;done ", t->left);
		break;
	  case TCASE:
		fptreef(shf, "case %S in%;", t->str);
		for (t1 = t->left; t1 != NULL; t1 = t1->right) {
			fptreef(shf, "(");
			for (w = t1->vars; *w != NULL; w++)
				fptreef(shf, "%S%c", *w, (w[1] != NULL) ? '|' : ')');
			fptreef(shf, " %T;;%;", t1->left);
		}
		fptreef(shf, "esac ");
		break;
	  case TIF:
		fptreef(shf, "if %T%;", t->left);
		t = t->right;
		if (t->left != NULL)
			fptreef(shf, "then %T%;", t->left);
		if (t->right != NULL)
			fptreef(shf, "else %T%;", t->right);
		fptreef(shf, "fi ");
		break;
	  case TWHILE:
	  case TUNTIL:
		fptreef(shf, "%s %T%;do %T%;done ",
			(t->type==TWHILE) ? "while" : "until",
			t->left, t->right);
		break;
	  case TBRACE:
		fptreef(shf, "{%;%T%;} ", t->left);
		break;
	  case TASYNC:
		fptreef(shf, "%T &", t->left);
		break;
	  case TFUNCT:
		fptreef(shf, "function %s %T", t->str, t->left);
		break;
	  case TTIME:
		fptreef(shf, "time %T", t->left);
		break;
	  default:
		fptreef(shf, "<botch>");
		break;
	}
	if ((ioact = t->ioact) != NULL)
		while (*ioact != NULL)
			pioact(shf, *ioact++);
}

void
pioact(shf, iop)
	register struct shf *shf;
	register struct ioword *iop;
{
	int flag = iop->flag;
	int type = flag & IOTYPE;
	int expected;

	expected = (type == IOREAD || type == IORDWR || type == IOHERE) ? 0
		    : (type == IOCAT || type == IOWRITE) ? 1
		    : (type == IODUP && (iop->unit == 0 || iop->unit == 1)) ?
			iop->unit
		    : iop->unit + 1;
	if (iop->unit != expected)
		fptreef(shf, "%c", '0' + iop->unit );

	switch(flag&IOTYPE) {
	case IOREAD:
		fptreef(shf, "< ");
		break;
	case IOHERE:
		if (flag&IOSKIP)
			fptreef(shf, "<<- ");
		else
			fptreef(shf, "<< ");
		break;
	case IOCAT:
		fptreef(shf, ">> ");
		break;
	case IOWRITE:
		if (flag&IOCLOB)
			fptreef(shf, ">| ");
		else
			fptreef(shf, "> ");
		break;
	case IORDWR:
		fptreef(shf, "<> ");
		break;
	case IODUP:
		if (iop->unit == 0)
			fptreef(shf, "<&");
		else
			fptreef(shf, ">&");
		break;
	}

	if (iop->name) {	/* name is 0 when printing syntax errors */
		if ((flag&IOTYPE) == IOHERE) {
			if (flag&IOEVAL)
				fptreef(shf, "%s ", iop->name);
			else
				fptreef(shf, "'%s' ", iop->name);
		} else
			fptreef(shf, "%S ", iop->name);
	}
}


/*
 * variants of fputc, fputs for ptreef and snptreef
 */

static void
tputC(c, shf)
	register int c;
	register struct shf *shf;
{
	if ((c&0x60) == 0) {		/* C0|C1 */
		tputc((c&0x80) ? '$' : '^', shf);
		tputc(((c&0x7F)|0x40), shf);
	} else if ((c&0x7F) == 0x7F) {	/* DEL */
		tputc((c&0x80) ? '$' : '^', shf);
		tputc('?', shf);
	} else
		tputc(c, shf);
}

static void
tputS(wp, shf)
	register char *wp;
	register struct shf *shf;
{
	register int c, quoted=0;

	while (1)
		switch ((c = *wp++)) {
		  case EOS:
			return;
		  case CHAR:
			tputC(*wp++, shf);
			break;
		  case QCHAR:
			if (!quoted)
				tputc('\\', shf);
			tputC(*wp++, shf);
			break;
		  case OQUOTE:
		  	quoted = 1;
			tputc('"', shf);
			break;
		  case CQUOTE:
			quoted = 0;
			tputc('"', shf);
			break;
		  case OSUBST:
			tputc('$', shf);
			tputc('{', shf);
			while ((c = *wp++) != 0)
				tputc(c, shf);
			if (*wp != CSUBST) {
				int c = *wp & 0x7f;
				if (*wp++ & 0x80)
					tputc(ctype(c, C_SUBOP) ? ':' : c, shf);
				tputC(c, shf);
			}
			break;
		  case CSUBST:
			tputc('}', shf);
			break;
		  case COMSUB:
			tputc('$', shf);
			tputc('(', shf);
			while (*wp != 0)
				tputC(*wp++, shf);
			tputc(')', shf);
			break;
		  case EXPRSUB:
			tputc('$', shf);
			tputc('(', shf);
			tputc('(', shf);
			while (*wp != 0)
				tputC(*wp++, shf);
			tputc(')', shf);
			tputc(')', shf);
			break;
		}
}

/*
 * this is the _only_ way to reliably handle
 * variable args with an ANSI compiler
 */
/* VARARGS */
int
#ifdef HAVE_PROTOTYPES
fptreef(struct shf *shf, char *fmt, ...)
#else
fptreef(shf, fmt, va_alist) 
  struct shf *shf;
  char *fmt;
  va_dcl
#endif
{
  va_list	va;

  SH_VA_START(va, fmt);
  
  vfptreef(shf, fmt, va);
  va_end(va);
  return 0;
}

/* VARARGS */
char *
#ifdef HAVE_PROTOTYPES
snptreef(char *s, int n, char *fmt, ...)
#else
snptreef(s, n, fmt, va_alist)
  char *s;
  int n;
  char *fmt;
  va_dcl
#endif
{
  va_list va;
  struct shf shf;

  shf_sopen(s, n, SHF_WR | (s ? 0 : SHF_DYNAMIC), &shf);

  SH_VA_START(va, fmt);
  vfptreef(&shf, fmt, va);
  va_end(va);

  return shf_sclose(&shf); /* null terminates */
}

static void
vfptreef(shf, fmt, va)
	register struct shf *shf;
	register char *fmt;
	register va_list va;
{
	register int c;

	while ((c = *fmt++))
	    if (c == '%') {
		register long n;
		register char *p;
		int neg;

		switch ((c = *fmt++)) {
		  case 'c':
			tputc(va_arg(va, int), shf);
			break;
		  case 's':
			p = va_arg(va, char *);
			while (*p)
				tputc(*p++, shf);
			break;
		  case 'S':	/* word */
			p = va_arg(va, char *);
			tputS(p, shf);
			break;
		  case 'd': case 'u': /* decimal */
			n = (c == 'd') ? va_arg(va, int) : va_arg(va, unsigned int);
			neg = c=='d' && n<0;
			p = ulton((neg) ? -n : n, 10);
			if (neg)
				*--p = '-';
			while (*p)
				tputc(*p++, shf);
			break;
		  case 'T':	/* format tree */
			ptree(va_arg(va, struct op *), shf);
			break;
		  case ';':	/* newline or ; */
			p = (shf->flags & SHF_STRING) ? "; " : "\n";
			while (*p)
				tputc(*p++, shf);
			break;
		  case 'R':
			pioact(shf, va_arg(va, struct ioword *));
			break;
		  default:
			tputc(c, shf);
			break;
		}
	    } else
		tputc(c, shf);
}

/*
 * copy tree (for function definition)
 */

struct op *
tcopy(t, ap)
	register struct op *t;
	Area *ap;
{
	register struct op *r;
	register char **tw, **rw;

	if (t == NULL)
		return NULL;

	r = (struct op *) alloc(sizeof(struct op), ap);

	r->type = t->type;
	r->evalflags = t->evalflags;

	r->str = t->type == TCASE ? wdcopy(t->str, ap) : strsave(t->str, ap);

	if (t->vars == NULL)
		r->vars = NULL;
	else {
		for (tw = t->vars; *tw++ != NULL; )
			;
		rw = r->vars = (char **)
			alloc((int)(tw - t->vars) * sizeof(*tw), ap);
		for (tw = t->vars; *tw != NULL; )
			*rw++ = wdcopy(*tw++, ap);
		*rw = NULL;
	}

	if (t->args == NULL)
		r->args = NULL;
	else {
		for (tw = t->args; *tw++ != NULL; )
			;
		rw = r->args = (char **)
			alloc((int)(tw - t->args) * sizeof(*tw), ap);
		for (tw = t->args; *tw != NULL; )
			*rw++ = wdcopy(*tw++, ap);
		*rw = NULL;
	}

	r->ioact = (t->ioact == NULL) ? NULL : iocopy(t->ioact, ap);

	r->left = tcopy(t->left, ap);
	r->right = tcopy(t->right, ap);

	return r;
}

char *
wdcopy(wp, ap)
	const char *wp;
	Area *ap;
{
	size_t len = wdscan(wp, EOS) - wp;
	return memcpy(alloc(len, ap), wp, len);
}

/* return the position of prefix c in wp plus 1 */
const char *
wdscan(wp, c)
	register const char *wp;
	register int c;
{
	register int nest = 0;

	while (1)
		switch (*wp++) {
		  case EOS:
			return wp;
		  case CHAR:
		  case QCHAR:
			wp++;
			break;
		  case OQUOTE:
		  case CQUOTE:
			break;
		  case OSUBST:
			nest++;
			while (*wp++ != 0)
				;
			if (*wp != CSUBST)
				wp++;
			break;
		  case CSUBST:
			if (c == CSUBST && nest == 0)
				return wp;
			nest--;
			break;
		  case COMSUB:
		  case EXPRSUB:
			while (*wp++ != 0)
				;
			break;
		}
}

static	struct ioword **
iocopy(iow, ap)
	register struct ioword **iow;
	Area *ap;
{
	register struct ioword **ior;
	register int i;

	for (ior = iow; *ior++ != NULL; )
		;
	ior = (struct ioword **) alloc((int)(ior - iow) * sizeof(*ior), ap);

	for (i = 0; iow[i] != NULL; i++) {
		register struct ioword *p, *q;

		p = iow[i];
		q = (struct ioword *) alloc(sizeof(*p), ap);
		ior[i] = q;
		*q = *p;
		if (p->name != NULL)
			q->name = wdcopy(p->name, ap);
	}
	ior[i] = NULL;

	return ior;
}

/*
 * free tree (for function definition)
 */

void
tfree(t, ap)
	register struct op *t;
	Area *ap;
{
	register char **w;

	if (t == NULL)
		return;

	if (t->str != NULL)
		afree((void*)t->str, ap);

	if (t->vars != NULL) {
		for (w = t->vars; *w != NULL; w++)
			afree((void*)*w, ap);
		afree((void*)t->vars, ap);
	}

	if (t->args != NULL) {
		for (w = t->args; *w != NULL; w++)
			afree((void*)*w, ap);
		afree((void*)t->args, ap);
	}

	if (t->ioact != NULL)
		iofree(t->ioact, ap);

	tfree(t->left, ap);
	tfree(t->right, ap);

	afree((void*)t, ap);
}

static	void
iofree(iow, ap)
	struct ioword **iow;
	Area *ap;
{
	register struct ioword **iop;
	register struct ioword *p;

	for (iop = iow; (p = *iop++) != NULL; ) {
		if (p->name != NULL)
			afree((void*)p->name, ap);
		afree((void*)p, ap);
	}
}
