#ifndef lint
static char *RCSid = "$Id: table.c,v 1.2 1992/04/25 08:33:28 sjg Exp $";
#endif

/*
 * dynamic hashed associative table for commands and variables
 */

#include "stdh.h"
#include <errno.h>
#include <setjmp.h>
#include "sh.h"

#define	INIT_TBLS	8	/* initial table size (power of 2) */

static struct tstate {
	int left;
	struct tbl **next;
} tstate;

static void     texpand     ARGS((struct table *tp, int nsize));
static int      tnamecmp    ARGS((void *p1, void *p2));


unsigned int
hash(n)
	register char * n;
{
	register unsigned int h = 0;

	while (*n != '\0')
		h = 2*h + *n++;
	return h * 32821;	/* scatter bits */
}

#if 0
phash(s) char *s; {
	printf("%2d: %s\n", hash(s)%32, s);
}
#endif

void
tinit(tp, ap)
	register struct table *tp;
	register Area *ap;
{
	tp->areap = ap;
	tp->size = tp->free = 0;
	tp->tbls = NULL;
}

static void
texpand(tp, nsize)
	register struct table *tp;
	int nsize;
{
	register int i;
	register struct tbl *tblp, **p;
	register struct tbl **ntblp, **otblp = tp->tbls;
	int osize = tp->size;

	ntblp = (struct tbl**) alloc(sizeofN(struct tbl *, nsize), tp->areap);
	for (i = 0; i < nsize; i++)
		ntblp[i] = NULL;
	tp->size = nsize;
	tp->free = 8*nsize/10;	/* table can get 80% full */
	tp->tbls = ntblp;
	if (otblp == NULL)
		return;
	for (i = 0; i < osize; i++)
		if ((tblp = otblp[i]) != NULL)
			if ((tblp->flag&DEFINED)) {
				for (p = &ntblp[hash(tblp->name) &
						(tp->size-1)];
				     *p != NULL; p--)
					if (p == ntblp) /* wrap */
						p += tp->size;
				*p = tblp;
				tp->free--;
			} else {
				afree((void*)tblp, tp->areap);
			}
	afree((void*)otblp, tp->areap);
}

struct tbl *
tsearch(tp, n, h)
	register struct table *tp;	/* table */
	register char *n;		/* name to enter */
	unsigned int h;			/* hash(n) */
{
	register struct tbl **pp, *p;

	if (tp->size == 0)
		return NULL;

	/* search for name in hashed table */
	for (pp = &tp->tbls[h & (tp->size-1)]; (p = *pp) != NULL; pp--) {
		if (*p->name == *n && strcmp(p->name, n) == 0
		    && (p->flag&DEFINED))
			return p;
		if (pp == tp->tbls) /* wrap */
			pp += tp->size;
	}

	return NULL;
}

struct tbl *
tenter(tp, n, h)
	register struct table *tp;	/* table */
	register char *n;		/* name to enter */
	unsigned int h;			/* hash(n) */
{
	register struct tbl **pp, *p;
	register char *cp;

	if (tp->size == 0)
		texpand(tp, INIT_TBLS);
  Search:
	/* search for name in hashed table */
	for (pp = &tp->tbls[h & (tp->size-1)]; (p = *pp) != NULL; pp--) {
		if (*p->name == *n && strcmp(p->name, n) == 0)
			return p; 	/* found */
		if (pp == tp->tbls) /* wrap */
			pp += tp->size;
	}

	if (tp->free <= 0) {	/* too full */
		texpand(tp, 2*tp->size);
		goto Search;
	}

	/* create new tbl entry */
	for (cp = n; *cp != '\0'; cp++)
		;
	p = (struct tbl *) alloc(offsetof(struct tbl, name[(cp-n)+1]), tp->areap);
	p->flag = 0;
	p->type = 0;
	for (cp = p->name; *n != '\0';)
		*cp++ = *n++;
	*cp = '\0';

	/* enter in tp->tbls */
	tp->free--;
	*pp = p;
	return p;
}

void
tdelete(p)
	register struct tbl *p;
{
	p->flag = 0;
}

void
twalk(tp)
	register struct table *tp;
{
	tstate.left = tp->size;
	tstate.next = tp->tbls;
}

struct tbl *
tnext()
{
	while (--tstate.left >= 0) {
		struct tbl *p = *tstate.next++;
		if (p != NULL && (p->flag&DEFINED))
			return p;
	}
	return NULL;
}

static int
tnamecmp(p1, p2)
	void *p1, *p2;
{
	return strcmp(((struct tbl *)p1)->name, ((struct tbl *)p2)->name);
}

struct tbl **
tsort(tp)
	register struct table *tp;
{
	register int i;
	register struct tbl **p, **sp, **dp;

	p = (struct tbl **)alloc(sizeofN(struct tbl *, tp->size+1), ATEMP);
	sp = tp->tbls;		/* source */
	dp = p;			/* dest */
	for (i = 0; i < tp->size; i++)
		if ((*dp = *sp++) != NULL && ((*dp)->flag&DEFINED))
			dp++;
	i = dp - p;
	qsortp((void**)p, (size_t)i, tnamecmp);
	p[i] = NULL;
	return p;
}

