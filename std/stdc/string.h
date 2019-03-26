/* ANSI string handling (missing wide char stuff) */
/* $Id: string.h,v 1.2 1992/04/25 08:19:26 sjg Exp $ */

#if ! _STRING_H
#define _STRING_H 1

#include <stddef.h>		/* define NULL and size_t */

#ifndef __GNUC__
void   *memcpy ARGS((void *s1, const void *s2, size_t));
int	memcmp ARGS((const void *s1, const void *s2, size_t));
size_t	strlen ARGS((const char *s));
#endif
void   *memmove ARGS((void *s1, const void *s2, size_t));
void   *memchr ARGS((const void *s, int c, size_t));
void   *memset ARGS((void *s, int c, size_t));
char   *strcpy ARGS((char *s1, const char *s2));
char   *strncpy ARGS((char *s1, const char *s2, size_t));
char   *strcat ARGS((char *s1, const char *s2));
char   *strncat ARGS((char *s1, const char *s2, size_t));
int	strcmp ARGS((const char *s1, const char *s2));
int	strncmp ARGS((const char *s1, const char *s2, size_t));
char   *strchr ARGS((const char *s1, int c));
char   *strrchr ARGS((const char *s1, int c));
size_t	strspn ARGS((const char *s1, const char *s2));
size_t	strcspn ARGS((const char *s1, const char *s2));
char   *strpbrk ARGS((const char *s1, const char *s2));
char   *strstr ARGS((const char *s1, const char *s2));
char   *strtok ARGS((char *s1, const char *s2));
char   *strerror ARGS((int errno));

#endif /* _STRING_H */

