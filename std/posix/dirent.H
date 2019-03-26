/*
	<sys/dirent.h> -- file system independent directory entry (SVR3)

	last edit:	25-Apr-1987	D A Gwyn
	last hack:	14-Aug-1987	Eric Gisin

	prerequisite:	<sys/types.h>
*/
/* $Id: dirent.H,v 1.2 1992/04/25 08:22:14 sjg Exp $ */

#define	MAXNAMLEN	16		/* maximum filename length */

struct dirent				/* data from getdents()/readdir() */
	{
	long		d_ino;		/* inode number of entry */
	off_t		d_off;		/* offset of disk directory entry */
	unsigned short	d_reclen;	/* length of this record */
	char	d_name[MAXNAMLEN];	/* name of file */
};

#ifndef NAME_MAX
#define	NAME_MAX	(MAXNAMLEN - 1)	/* DAG -- added for POSIX */
#endif

typedef struct {			/* returned by opendir() */
	int	fd;
	struct dirent ent;
} DIR;

extern DIR		*opendir();
extern struct dirent	*readdir();
extern off_t		telldir();
extern void		seekdir();
extern void		rewinddir();
extern int		closedir();

#ifndef NULL
#define	NULL	0			/* DAG -- added for convenience */
#endif
