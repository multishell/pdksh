/*
 * printf and fprintf
 */

/* $Id: fprintf.c,v 1.3 1992/05/12 09:30:58 sjg Exp $ */

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <stdio.h>

#if _V7 || _BSD

/* printf to stdout */
int
#ifdef __STDC__
printf(const char *fmt, ...) {
#else
printf(va_alist) va_dcl
{
	char *fmt;
#endif
	va_list va;

#ifdef __STDC__
	va_start(va, fmt);
#else
	va_start(va);
	fmt = va_arg(va, char *);
#endif
	vfprintf(stdout, fmt, va);
	va_end(va);
	return 0;
}

int
#ifdef __STDC__
fprintf(FILE *f, const char *fmt, ...) {
#else
fprintf(va_alist) va_dcl
{
	FILE *f;
	char *fmt;
#endif
	va_list va;

#ifdef __STDC__
	va_start(va, fmt);
#else
	va_start(va);
	f = va_arg(va, FILE *);
	fmt = va_arg(va, char *);
#endif
	vfprintf(f, fmt, va);
	va_end(va);
	return 0;
}

#endif
