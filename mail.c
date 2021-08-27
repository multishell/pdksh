/*
 * Mailbox checking code by Robert J. Gibson, adapted for PD ksh by
 * John R. MacMillan
 */

#include "sh.h"
#include "ksh_stat.h"
#include "ksh_time.h"

#define MBMESSAGE	"you have mail in $_"

typedef struct mbox {
	struct mbox    *mb_next;	/* next mbox in list */
	char	       *mb_path;	/* path to mail file */
	char	       *mb_msg;		/* to announce arrival of new mail */
	time_t		mb_mtime;	/* mtime of mail file */
} mbox_t;

struct mailmsg {
	char		*msg;		/* Text of message */
	struct mailmsg	*next;		/* Next message */
};

/*
 * $MAILPATH is a linked list of mboxes.  $MAIL is a treated as a
 * special case of $MAILPATH, where the list has only one node.  The
 * same list is used for both since they are exclusive.
 */

static mbox_t  *mplist = NULL;
static mbox_t  mbox = { NULL, NULL, NULL, 0 };
static time_t	mlastchkd = 0;	/* when mail was last checked */
static struct mailmsg *mmsgs = NULL;	/* Messages to be printed */

static void     munset      ARGS((mbox_t *mlist)); /* free mlist and mval */
static mbox_t * mballoc     ARGS((char *p, char *m)); /* allocate a new mbox */
static void     maddmsg     ARGS((mbox_t *mbp));

void
mcheck()
{
	register mbox_t	*mbp;
	time_t		 now;
	long		 mailcheck;
	struct tbl	*vp;
	struct stat	 stbuf;

	if (getint(global("MAILCHECK"), &mailcheck) < 0)
		return;

	now = time((time_t *) 0);
	if (mlastchkd == 0)
		mlastchkd = now;
	if (now - mlastchkd >= mailcheck) {
		mlastchkd = now;

		vp = global("MAILPATH");
		if (vp && (vp->flag & ISSET))
			mbp = mplist;
		else if ((vp = global("MAIL")) && (vp->flag & ISSET))
			mbp = &mbox;
		else
			mbp = NULL;

		while (mbp) {
			if (mbp->mb_path && stat(mbp->mb_path, &stbuf) == 0
			    && S_ISREG(stbuf.st_mode))
			{
				if (stbuf.st_size
				    && mbp->mb_mtime != stbuf.st_mtime
				    && stbuf.st_atime <= stbuf.st_mtime)
					maddmsg(mbp);
				mbp->mb_mtime = stbuf.st_mtime;
			} else {
				/*
				 * Some mail readers remove the mail
				 * file if all mail is read.  If file
				 * does not exist, assume this is the
				 * case and set mtime to zero.
				 */
				mbp->mb_mtime = 0;
			}
			mbp = mbp->mb_next;
		}
	}
}

void
mbset(p)
	register char	*p;
{
	struct stat	stbuf;

	if (mbox.mb_msg)
		afree((void *)mbox.mb_msg, APERM);
	mbox.mb_path = p;
	mbox.mb_msg = NULL;
	if (p && stat(p,&stbuf) == 0 && S_ISREG(stbuf.st_mode))
		mbox.mb_mtime = stbuf.st_mtime;
	else
		mbox.mb_mtime = 0;
}

void
mpset(mptoparse)
	register char	*mptoparse;
{
	register mbox_t	*mbp;
	register char	*mpath, *mmsg, *mval;
	char *p;

	munset( mplist );
	mplist = NULL;
	mval = strsave(mptoparse, APERM);
	while (mval) {
		mpath = mval;
		if ((mval = strchr(mval, PATHSEP)) != NULL) {
			*mval = '\0', mval++;
		}
		/* POSIX/bourne-shell say file%message */
		for (p = mpath; (mmsg = strchr(p, '%')); ) {
			/* a literal percent? (POSIXism) */
			if (mmsg[-1] == '\\') {
				/* use memmove() to avoid overlap problems */
				memmove(mmsg - 1, mmsg, strlen(mmsg) + 1);
				p = mmsg + 1;
				continue;
			}
			break;
		}
		/* at&t ksh says file?message */
		if (!mmsg && !Flag(FPOSIX))
			mmsg = strchr(mpath, '?');
		if (mmsg) {
			*mmsg = '\0';
			mmsg++;
		}
		mbp = mballoc(mpath, mmsg);
		mbp->mb_next = mplist;
		mplist = mbp;
	}
}

static void
munset(mlist)
register mbox_t	*mlist;
{
	register mbox_t	*mbp;

	while (mlist != NULL) {
		mbp = mlist;
		mlist = mbp->mb_next;
		if (!mlist)
			afree((void *)mbp->mb_path, APERM);
		afree((void *)mbp, APERM);
	}
}

static mbox_t *
mballoc(p, m)
	char	*p;
	char	*m;
{
	struct stat	stbuf;
	register mbox_t	*mbp;

	mbp = (mbox_t *)alloc(sizeof(mbox_t), APERM);
	mbp->mb_next = NULL;
	mbp->mb_path = p;
	mbp->mb_msg = m;
	if (stat(mbp->mb_path, &stbuf) == 0 && S_ISREG(stbuf.st_mode))
		mbp->mb_mtime = stbuf.st_mtime;
	else
		mbp->mb_mtime = 0;
	return(mbp);
}

void
mprint()
{
	struct mailmsg *mm;

	while ((mm = mmsgs) != NULL) {
		shellf("%s\n", mm->msg);
		afree((void *)mm->msg, APERM);
		mmsgs = mm->next;
		afree((void *)mm, APERM);
	}
}

static void
maddmsg( mbp )
mbox_t	*mbp;
{
	struct mailmsg	*message;
	struct tbl	*vp;

	message = (struct mailmsg *)alloc(sizeof(struct mailmsg), APERM);
	setstr((vp = typeset("_", LOCAL, 0, 0, 0)), mbp->mb_path);

	if (mbp->mb_msg)
		message->msg = strsave(substitute(mbp->mb_msg,0),APERM);
	else
		message->msg = strsave(substitute(MBMESSAGE,0),APERM);

	unset(vp, 0);
	message->next = mmsgs;
	mmsgs = message;
}