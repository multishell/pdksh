/* $Id: memcmp.c,v 1.2 1992/04/25 08:19:26 sjg Exp $ */

#include <string.h>

int
memcmp(dap, sap, n)
	const void *dap;
	const void *sap;
	register size_t n;
{
	register const unsigned char *dp = (unsigned char const *) dap;
	register const unsigned char *sp = (unsigned char const *) sap;

	if (n++ > 0)
		while (--n > 0)
			if (*dp++ != *sp++)
				return *--dp - *--sp; /* (int)? */
	return 0;
}

