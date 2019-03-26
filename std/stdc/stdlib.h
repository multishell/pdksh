/* ANSI utility functions */

/* $Id: stdlib.h,v 1.2 1992/04/25 08:19:26 sjg Exp $ */

#if ! _STDLIB_H
#define	_STDLIB_H 1

#include <stddef.h>

double	atof ARGS((const char *s));
int	atoi ARGS((const char *s));
long	atol ARGS((const char *s));
double	strtod ARGS((const char *s, char **));
long	strtol ARGS((const char *s, char **, int base));
unsigned long	strtoul ARGS((const char *s, char **, int base));
int	rand ARGS((void));
void	srand ARGS((unsigned int seed));
void   *malloc ARGS((size_t size));
void   *realloc ARGS((void *ptr, size_t size));
void   *calloc ARGS((size_t n, size_t size));
void	free ARGS((void *ptr));
void	abort ARGS((void));
int	atexit ARGS((void (*func)(void)));
void	exit ARGS((int status));
char   *getenv ARGS((const char *name));
int	system ARGS((const char *cmd));
void   *bsearch ARGS ((const void *key, const void *base, size_t n, size_t size,
		       int (*compar)(const void *, const void *)));
void   *qsort ARGS ((const void *base, size_t n, size_t size,
		     int (*compar)(const void *, const void *)));
#define	abs(a)	((a) < 0 : -(a) : (a))

#endif

