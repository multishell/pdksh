/*
 * area-based allocation built on malloc/free
 */

typedef struct Area {
	struct Block *free;	/* free list */
} Area;

