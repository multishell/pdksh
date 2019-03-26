/*
 * command tree climbing
 */

#ifndef lint
static char *RCSid = "$Id: tree.c,v 1.2 1992/04/25 08:33:28 sjg Exp $";
#endif

#include "stdh.h"
#include <errno.h>
#include <setjmp.h>

#ifdef __STDC__
# include <stdarg.h>
#else
# include <varargs.h>
#endif
#include "sh.h"

#define	FSTRING	(FILE*)NULL

static int      tputc       ARGS((int c, FILE *f));
static void     tputC       ARGS((int c, FILE *f));
static void     tputS       ARGS((char *wp, FILE *f));
static struct ioword **iocopy ARGS((struct ioword **iow, Area *ap));
static void     iofree      ARGS((struct ioword **iow, Area *ap));

/*
 * print a command tree
 */

void
ptree(t, f)
	register struct op *t;
	register FILE *f;
{
	register char **w;
	struct ioword **ioact;
	struct op *t1;

 Chain:
	if (t == NULL)
		return;
	switch (t->type) {
	  case TCOM:
		for (w = t->vars; *w != NULL; )
			fptreef(f, "%S ", *w++);
		for (w = t->args; *w != NULL; )
			fptreef(f, "%S ", *w++);
		break;
	  case TEXEC:
		t = t->left;
		goto Chain;
	  case TPAREN:
		fptreef(f, "(%T)", t->left);
		break;
	  case TPIPE:
		fptreef(f, "%T| ", t->left);
		t = t->right;
		goto Chain;
	  case TLIST:
		fptreef(f, "%T", t->left);
		fptreef(f, "%;");
		t = t->right;
		goto Chain;
	  case TOR:
	  case TAND:
		fptreef(f, "%T", t->left);
		fptreef(f, "%s %T", (t->type==TOR) ? "||" : "&&", t->right);
		break;
	  case TFOR:
		fptreef(f, "for %s ", t->str);
		if (t->vars != NULL) {
			fptreef(f, "in ");
			for (w = t->vars; *w; )
				fptreef(f, "%S ", *w++);
			fptreef(f, "%;");
		}
		fptreef(f, "do %T", t->left);
		fptreef(f, "%;done ");
		break;
	  case TCASE:
		fptreef(f, "case %S in%;", t->str);
		for (t1 = t->left; t1 != NULL; t1 = t1->right) {
			fptreef(f, "(");
			for (w = t1->vars; *w != NULL; w++) {
				fptreef(f, "%S", *w);
				fptreef(f, "%c", (w[1] != NULL) ? '|' : ')');
			}
			fptreef(f, " %T;;%;", t1->left);
		}
		fptreef(f, "esac ");
		break;
	  case TIF:
		fptreef(f, "if %T", t->left);
		fptreef(f, "%;");
		t = t->right;
		if (t->left != NULL) {
			fptreef(f, "then %T", t->left);
			fptreef(f, "%;");
		}
		if (t->right != NULL) {
			fptreef(f, "else %T", t->right);
			fptreef(f, "%;");
		}
		fptreef(f, "fi ");
		break;
	  case TWHILE:
	  case TUNTIL:
		fptreef(f, "%s %T",
			(t->type==TWHILE) ? "while" : "until",
			t->left);
		fptreef(f, "%;do %T", t->right);
		fptreef(f, "%;done ");
		break;
	  case TBRACE:
		fptreef(f, "{%;%T", t->left);
		fptreef(f, "%;} ");
		break;
	  case TASYNC:
		fptreef(f, "%T&", t->left);
		break;
	  case TFUNCT:
		fptreef(f, "function %s %T", t->str, t->left);
		break;
	  case TTIME:
		fptreef(f, "time %T", t->left);
		break;
	  default:
		fptreef(f, "<botch>");
		break;
	}
	if ((ioact = t->ioact) != NULL)
		while (*ioact != NULL)
			pioact(f, *ioact++);
}

pioact(f, iop)
	register FILE *f;
	register struct ioword *iop;
{
	register int flag = iop->flag;
	if (iop->unit > 1)
		fptreef(f, "%c", '0' + iop->unit );

	switch(flag&IOTYPE) {
	case IOREAD:
		fptreef(f, "< ");
		break;
	case IOHERE:
		if (flag&IOSKIP)
			fptreef(f, "<<- ");
		else
			fptreef(f, "<< ");
		if (!(flag&IOEVAL))
			fptreef(f, "'");
		break;
	case IOCAT:
		fptreef(f, ">> ");
		break;
	case IOWRITE:
		if (flag&IOCLOB)
			fptreef(f, ">! ");
		else
			fptreef(f, "> ");
		break;
	case IODUP:			/* Needs help */
		if (iop->unit == 0)
			fptreef(f, "<&");
		else
			fptreef(f, ">&");
		break;
	}

	if ((flag&IOTYPE) == IOHERE) {
		if (flag&IOEVAL)
			fptreef(f, "%s ", iop->name);
		else
			fptreef(f, "%s' ", iop->name);
	} else {
		fptreef(f, "%S ", iop->name);
	}
}


/*
 * variants of fputc, fputs for ptreef and snptreef
 */

static	char   *snpf_s;		/* snptreef string */
static	int	snpf_n;		/* snptreef length */

static int
tputc(c, f)
	int c;
	register FILE *f;
{
	if (f != NULL)
		putc(c, f);
	else
		if (--snpf_n >= 0)
			*snpf_s++ = c;
	return c;
}

static void
tputC(c, f)
	register int c;
	register FILE *f;
{
	if ((c&0x60) == 0) {		/* C0|C1 */
		tputc((c&0x80) ? '$' : '^', f);
		tputc(((c&0x7F)|0x40), f);
	} else if ((c&0x7F) == 0x7F) {	/* DEL */
		tputc((c&0x80) ? '$' : '^', f);
		tputc('?', f);
	} else
		tputc(c, f);
}

static void
tputS(wp, f)
	register char *wp;
	register FILE *f;
{
	register int c, quoted=0;

	while (1)
		switch ((c = *wp++)) {
		  case EOS:
			return;
		  case CHAR:
			tputC(*wp++, f);
			break;
		  case QCHAR:
			if (!quoted)
				tputc('\\', f);
			tputC(*wp++, f);
			break;
		  case OQUOTE:
		  	quoted = 1;
			tputc('"', f);
			break;
		  case CQUOTE:
			quoted = 0;
			tputc('"', f);
			break;
		  case OSUBST:
			tputc('$', f);
			tputc('{', f);
			while ((c = *wp++) != 0)
				tputc(c, f);
			if (*wp != CSUBST)
				tputC(*wp++, f);
			break;
		  case CSUBST:
			tputc('}', f);
			break;
		  case COMSUB:
			tputc('$', f);
			tputc('(', f);
			while (*wp != 0)
				tputC(*wp++, f);
			tputc(')', f);
			break;
		}
}

/*
 * this is the _only_ way to reliably handle
 * variable args with an ANSI compiler
 */
#ifdef __STDC__
/* VARARGS */ int
fptreef(FILE *f, char *fmt, ...)
{
#else
fptreef(va_alist) 
  va_dcl
{
  FILE *f;
  char *fmt;
#endif
  va_list	va;

#ifdef __STDC__
  va_start(va, fmt);
#else
  va_start(va);
  f = va_arg(va, FILE *);
  fmt = va_arg(va, char *);
#endif
  
  vfptreef(f, fmt, va);
  va_end(va);
  return 0;
}

/* VARARGS */ int
#ifdef __STDC__
snptreef(char *s, int n, char *fmt, ...)
{
#else
snptreef(va_alist)
  va_dcl
{
  char *s;
  int n;
  char *fmt;
#endif
  va_list va;

#ifdef __STDC__
  va_start(va, fmt);
#else
  va_start(va);
  s = va_arg(va, char *);
  n = va_arg(va, int);
  fmt = va_arg(va, char *);
#endif

  snpf_s = s;
  snpf_n = n;
  vfptreef(FSTRING, fmt, va);
  tputc('\0', FSTRING);
  va_end(va);
  return 0;
}


vfptreef(f, fmt, va)
	register FILE *f;
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
			tputc(va_arg(va, int), f);
			break;
		  case 's':
			p = va_arg(va, char *);
			while (*p)
				tputc(*p++, f);
			break;
		  case 'S':	/* word */
			p = va_arg(va, char *);
			tputS(p, f);
			break;
		  case 'd': case 'u': /* decimal */
			n = (c == 'd') ? va_arg(va, int) : va_arg(va, unsigned int);
			neg = c=='d' && n<0;
			p = ulton((neg) ? -n : n, 10);
			if (neg)
				*--p = '-';
			while (*p)
				tputc(*p++, f);
			break;
		  case 'T':	/* format tree */
			ptree(va_arg(va, struct op *), f);
			break;
		  case ';':	/* newline or ; */
			p = (f == FSTRING) ? "; " : "\n";
			while (*p)
				tputc(*p++, f);
			break;
		  default:
			tputc(c, f);
			break;
		}
	    } else
		tputc(c, f);
}

/*
 * copy tree (for function definition)
 */

static	struct ioword **iocopy();

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

	/* this will copy function and for identifiers quite accidently */
	r->str = (t->str == NULL) ? NULL : wdcopy(t->str, ap);

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
	char *wp;
	Area *ap;
{
	size_t len = wdscan(wp, EOS) - wp;
	return memcpy(alloc(len, ap), wp, len);
}

/* return the position of prefix c in wp plus 1 */
char *
wdscan(wp, c)
	register char *wp;
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

static	void iofree();

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

