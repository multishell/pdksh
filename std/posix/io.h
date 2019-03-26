/* POSIX IO functions */
/* $Id: io.h,v 1.2 1992/04/25 08:22:14 sjg Exp $ */

/*
 * the incomplete type "struct stat"
 * will get warnings from GCC,
 * errors from Turbo C. Too bad.
 */

/* include <unistd.h> to get this */

#if ! _IO_H
#define	_IO_H	1

#include <unistd.h>

#if _ST				/* dLibs hack */
#define	unlink	remove
#endif

struct stat;			/* create global incompletely-typed structure */

int chdir ARGS ((const char *path));
#ifndef sparc
int umask ARGS ((int mode));
#endif

int open ARGS ((const char *path, int flags, ... /*mode*/));
int creat ARGS ((const char *path, int mode));
int pipe ARGS ((int pv[2]));
int close ARGS ((int fd));

int fcntl ARGS ((int fd, int cmd, int arg));
int dup ARGS ((int fd));
int dup2 ARGS ((int ofd, int nfd));

int link ARGS ((const char *opath, const char *npath));
int unlink ARGS ((const char *path));
int rename ARGS ((const char *opath, const char *npath));
int mkdir ARGS ((const char *path, int mode));

long lseek ARGS ((int fd, long off, int how));
int read ARGS ((int fd, char *buf, unsigned len));
int write ARGS ((int fd, char *buf, unsigned len));

int access ARGS ((const char *path, int mode));
int stat ARGS ((const char *path, struct stat *sp));
int fstat ARGS ((int fd, struct stat *sp));

int chmod ARGS ((const char *path, int mode));
int chown ARGS ((const char *path, int uid));
int chgrp ARGS ((const char *path, int gid));
int utime ARGS ((const char *path, long tv[2]));

#if _BSD || _V7
int ioctl ARGS ((int fd, int cmd, void *argp)); /* BSD is "uns long cmd" */
#endif

#endif
