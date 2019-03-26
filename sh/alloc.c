/*
 * area-based allocation built on malloc/free
 */

#ifndef lint
static char *RCSid = "$Id: alloc.c,v 1.2 1992/04/25 08:33:28 sjg Exp $";
#endif

#include "stdh.h"
#include <setjmp.h>
#include "sh.h"

#define	ICELLS	100		/* number of Cells in small Block */

typedef union Cell Cell;
typedef struct Block Block;

/*
 * The Cells in a Block are organized as a set of objects.
 * Each object (pointed to by dp) begins with a size in (dp-1)->size,
 * followed with "size" data Cells.  Free objects are
 * linked together via dp->next.
 */

union Cell {
	size_t	size;
	union Cell   *next;
	struct {int _;} junk;	/* alignment */
};

struct Block {
	struct Block  *next;		/* list of Blocks in Area */
	union Cell   *free;		/* object free list */
	union Cell   *last;		/* &b.cell[size] */
	union Cell	cell [1];	/* [size] Cells for allocation */
};

Block aempty = {&aempty, aempty.cell, aempty.cell};

/* create empty Area */
Area *
ainit(ap)
	register Area *ap;
{
	ap->free = &aempty;
	return ap;
}

/* free all object in Area */
void
afreeall(ap)
	register Area *ap;
{
	register Block *bp;
	register Block *tmp;

	bp = ap->free;
	if (bp != NULL && bp != &aempty) {
		do {
			tmp = bp->next;
			free((void*)bp);
			bp = tmp;
		} while (bp != ap->free);
		ap->free = &aempty;
	}
}

/* allocate object from Area */
void *
alloc(size, ap)
	size_t size;
	register Area *ap;
{
	int cells, split;
	register Block *bp;
	register Cell *dp, *fp, *fpp;

	if (size <= 0) {
		aerror(ap, "allocate bad size");
		return NULL;
	}
	cells = (unsigned)(size - 1) / sizeof(Cell) + 1;

	/* find Cell large enough */
	for (bp = ap->free; ; bp = bp->next) {
		for (fpp = NULL, fp = bp->free;
		     fp != bp->last; fpp = fp, fp = fpp->next)
			if ((fp-1)->size >= cells)
				goto Found;

		/* wrapped around Block list, create new Block */
		if (bp->next == ap->free) {
			bp = (Block*) malloc(offsetof(Block, cell[ICELLS + cells]));
			if (bp == NULL) {
				aerror(ap, "cannot allocate");
				return NULL;
			}
			if (ap->free == &aempty)
				bp->next = bp;
			else {
				bp->next = ap->free->next;
				ap->free->next = bp;
			}
			bp->last = bp->cell + ICELLS + cells;
			fp = bp->free = bp->cell + 1; /* initial free list */
			(fp-1)->size = ICELLS + cells - 1;
			fp->next = bp->last;
			fpp = NULL;
			break;
		}
	}
  Found:
	ap->free = bp;
	dp = fp;		/* allocated object */
	split = (dp-1)->size - cells;
	if (split < 0)
		aerror(ap, "allocated object too small");
	if (--split <= 0) {	/* allocate all */
		fp = fp->next;
	} else {		/* allocate head, free tail */
		(fp-1)->size = cells;
		fp += cells + 1;
		(fp-1)->size = split;
		fp->next = dp->next;
	}
	if (fpp == NULL)
		bp->free = fp;
	else
		fpp->next = fp;
	return (void*) dp;
}

/* change size of object -- like realloc */
void *
aresize(ptr, size, ap)
	register void *ptr;
	size_t size;
	Area *ap;
{
	int cells;
	register Cell *dp = (Cell*) ptr;

	if (size <= 0) {
		aerror(ap, "allocate bad size");
		return NULL;
	}
	cells = (unsigned)(size - 1) / sizeof(Cell) + 1;

	if (dp == NULL || (dp-1)->size < cells) { /* enlarge object */
		register Cell *np;
		register int i;
		void *optr = ptr;

		ptr = alloc(size, ap);
		np = (Cell*) ptr;
		if (dp != NULL)
			for (i = (dp-1)->size; i--; )
				*np++ = *dp++;
		afree(optr, ap);
	} else {		/* shrink object */
		int split;

		split = (dp-1)->size - cells;
		if (--split <= 0) /* cannot split */
			;
		else {		/* shrink head, free tail */
			(dp-1)->size = cells;
			dp += cells + 1;
			(dp-1)->size = split;
			afree((void*)dp, ap);
		}
	}
	return (void*) ptr;
}

void
afree(ptr, ap)
	void *ptr;
	register Area *ap;
{
	register Block *bp;
	register Cell *fp, *fpp;
	register Cell *dp = (Cell*)ptr;

	/* find Block containing Cell */
	for (bp = ap->free; ; bp = bp->next) {
		if (bp->cell <= dp && dp < bp->last)
			break;
		if (bp->next == ap->free) {
			aerror(ap, "freeing with invalid area");
			return;
		}
	}

	/* find position in free list */
	for (fpp = NULL, fp = bp->free; fp < dp; fpp = fp, fp = fpp->next)
		;

	if (fp == dp) {
		aerror(ap, "freeing free object");
		return;
	}

	/* join object with next */
	if (dp + (dp-1)->size == fp-1) { /* adjacent */
		(dp-1)->size += (fp-1)->size + 1;
		dp->next = fp->next;
	} else			/* non-adjacent */
		dp->next = fp;

	/* join previous with object */
	if (fpp == NULL)
		bp->free = dp;
	else if (fpp + (fpp-1)->size == dp-1) { /* adjacent */
		(fpp-1)->size += (dp-1)->size + 1;
		fpp->next = dp->next;
	} else			/* non-adjacent */
		fpp->next = dp;
}


#if TEST_ALLOC

Area a;

main(int argc, char **argv) {
	int i;
	char *p [9];

	ainit(&a);
	for (i = 0; i < 9; i++) {
		p[i] = alloc(124, &a);
		printf("alloc: %x\n", p[i]);
	}
	for (i = 1; i < argc; i++)
		afree(p[atoi(argv[i])], &a);
	afreeall(&a);
	return 0;
}

void aerror(Area *ap, const char *msg) {
	abort();
}

#endif

