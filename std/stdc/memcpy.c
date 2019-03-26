/* $Id: memcpy.c,v 1.2 1992/04/25 08:19:26 sjg Exp $ */

#include <string.h>

void *
memcpy(dap, sap, n)
	void *dap;
	const void *sap;
	register size_t n;
{
	register char *dp = dap, *sp = (void*) sap;

	if (n++ > 0)
		while (--n > 0)
			*dp++ = *sp++;
	return dap;
}

