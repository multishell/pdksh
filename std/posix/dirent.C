/*
 * simple implementation of directory(3) routines for V7 and Minix.
 * completly untested. not designed to be efficient.
 * missing telldir and seekdir.
 */
/* $Id: dirent.C,v 1.2 1992/04/25 08:22:14 sjg Exp $ */

#include <sys/types.h>
#include <dirent.h>

char	*malloc();

#define	DIRSIZ	14
struct	direct_v7
{
	unsigned short	d_ino;
	char	d_name[DIRSIZ];
};

DIR *opendir(filename)
	char *filename;
{
	DIR *dirp;

	dirp = (DIR *) malloc(sizeof(DIR));
	if (dirp == NULL)
		return NULL;
	dirp->fd = open(filename, 0);
	if (dirp->fd < 0) {
		free((char *) dirp);
		return NULL;
	}
	return dirp;
}

struct dirent *readdir(dirp)
	register DIR *dirp;
{
	static	struct direct_v7 ent;

	while (read(dirp->fd, (char *)&ent, (int)sizeof(ent)) == sizeof(ent))
		if (ent.d_ino != 0)
			goto found;
	return (struct dirent *) NULL;
 found:
	dirp->ent.d_ino = ent.d_ino;
	strncpy(dirp->ent.d_name, ent.d_name, DIRSIZ);
	return &dirp->ent;
}

void rewinddir(dirp)
	DIR *dirp;
{
	lseek(dirp->fd, 0L, 0);
}

closedir(dirp)
	DIR *dirp;
{
	close(dirp->fd);
	dirp->fd = -1;
	free((char *) dirp);
	return 0;
}
