/*
 * Mailbox checking code by Robert J. Gibson, adapted for PD ksh by
 * John R. MacMillan
 */
#if !defined(lint) && !defined(no_RCSids)
static char *RCSid = "$Id: mail.c,v 1.3 1994/05/31 13:34:34 michael Exp $";
#endif

#include "sh.h"
#include "ksh_stat.h"
#include "ksh_time.h"

#define MBMESSAGE	"you have mail in $_"

typedef struct mbox {
	struct mbox    *mb_next;	/* next mbox in list */
	char	       *mb_path;	/* path to mail file */
	char	       *mb_msg;		/* to announce arrival of new mail */
	unsigned int	mb_size;	/* size of mail file (bytes) */
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

	if (mlastchkd == 0)
		mlastchkd = time((time_t *)0);

	if ((now=time((time_t *)0)) - mlastchkd >= mailcheck) {
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
				if (mbp->mb_size < stbuf.st_size)
					maddmsg( mbp );
				mbp->mb_size = stbuf.st_size;
			} else {
				/*
				 * Some mail readers remove the mail
				 * file if all mail is read.  If file
				 * does not exist, assume this is the
				 * case and set size to zero.
				 */
				mbp->mb_size = 0;
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
		mbox.mb_size = stbuf.st_size;
	else
		mbox.mb_size = 0;
}

void
mpset(mptoparse)
	register char	*mptoparse;
{
	register mbox_t	*mbp;
	register char	*mpath, *mmsg, *mval;

	munset( mplist );
	mplist = NULL;
	mval = strsave(mptoparse, APERM);
	while (mval) {
		mpath = mval;
		if ((mval = strchr(mval, ':')) != NULL) {
			*mval = '\0', mval++;
		}
		if ((mmsg = strchr(mpath, '?')) != NULL) {
			*mmsg = '\0', mmsg++;
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
		mbp->mb_size = stbuf.st_size;
	else
		mbp->mb_size = 0;
	return(mbp);
}

void
mprint()
{
	struct mailmsg *mm;

	while ((mm = mmsgs) != NULL) {
		shellf( "%s\n", mm->msg );
		shf_flush(shl_out);
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

	unset(vp);
	message->next = mmsgs;
	mmsgs = message;
}
