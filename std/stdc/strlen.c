#include <string.h>
/* $Id: strlen.c,v 1.2 1992/04/25 08:19:26 sjg Exp $ */

/*
 * strlen - length of string (not including NUL)
 */
size_t
strlen(s)
const char *s;
{
	register const char *scan;
	register size_t count;

	count = 0;
	scan = s;
	while (*scan++ != '\0')
		count++;
	return(count);
}
