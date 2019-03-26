/* P1003.1 fcntl/open definitions */
/* Based on a version by Terrence W. Holm */
/*  for fcntl(2)  */
/* $Id: fcntl.h,v 1.2 1992/04/25 08:22:14 sjg Exp $ */

#define	F_DUPFD		0
#define	F_GETFD		1
#define	F_SETFD		2
#define	F_GETFL		3
#define	F_SETFL		4

#define	FD_CLEXEC	1		/* fcntl F_SETFD close on exec mode */

/*  for open(2)  */

#define	O_RDONLY	0
#define	O_WRONLY	1
#define	O_RDWR		2

#if _BSD
#undef	O_RDONLY
#undef	O_WRONLY
#undef	O_RDWR
#include "/./usr/include/fcntl.h"
#endif

