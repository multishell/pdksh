/*
 * PD ksh needs an ANSI-compatible setvbuf.
 * if (buf == NULL) it must also allocate a buffer
 * and arrange for fclose to deallocate it.
 * the reason for doing setvbuf(f, (char *)NULL, _IOFBF, BUFSIZ)
 * in the shell is to avoid 4/8K buffers on BSD like systems.
 */

/* $Id: setvbuf.c,v 1.2 1992/04/25 08:19:26 sjg Exp $ */

#include <stdlib.h>
#include <stdio.h>

#if _BSD || _SYSV
int
setvbuf(f, buf, type, size)
	register FILE *f;
	char *buf;
	int type;
	size_t size;
{
	if ((f->_flag&_IOMYBUF) && f->_base != NULL)
		free(f->_base);
	f->_flag &= ~(_IOMYBUF|_IONBF|_IOFBF|_IOLBF);
	switch (type) {
	  case _IONBF:
		size = 0;
		buf = NULL;
		break;
	  case _IOLBF:
	  case _IOFBF:
		if (size == 0)
			size = BUFSIZ;
#if _V7
		else if (size != BUFSIZ)
			return -1;
#endif
		if (buf == NULL) {
			buf = malloc(size);
			if (buf == NULL)
				return -1;
			f->_flag |= _IOMYBUF;
		}
		break;
	  default:
		return -1;
	}
	f->_flag |= type;
	f->_base = f->_ptr = buf;
	f->_cnt = 0;
#if _BSD
	f->_bufsiz = size;
#endif
#if _SYSV
	_bufend(f) = buf + size;
#endif
	return 0;
}
#endif

