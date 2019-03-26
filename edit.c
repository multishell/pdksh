/*
 * Command line editing - common code
 *
 */

#include "config.h"
#ifdef EDIT

#if !defined(lint) && !defined(no_RCSids)
static char *RCSid = "$Id: edit.c,v 1.3 1994/05/31 13:34:34 michael Exp $";
#endif

#include "sh.h"
#include "tty.h"
#define EXTERN
#include "edit.h"
#undef EXTERN


/* Called from main */
void
x_init()
{
	/* set to -1 to force initial binding */
	edchars.erase = edchars.kill = edchars.intr = edchars.quit = -1;
	/* default value for deficient systems */
	edchars.werase = 027;	/* ^W */
#ifdef TIOCGWINSZ
	{
		struct winsize ws;

		if (ioctl(tty_fd, TIOCGWINSZ, &ws) >= 0 && ws.ws_col) {
			x_cols = ws.ws_col;
			setint(global("COLUMNS"), (long) x_cols);
		}
	}
#endif /* TIOCGWINSZ */
#ifdef EMACS
	x_init_emacs();
#endif /* EMACS */
}

/*
 * read an edited command line
 */
int
x_read(buf, len)
	char *buf;
	size_t len;
{
	int	i;

	x_mode(TRUE);
#ifdef EMACS
	if (Flag(FEMACS) || Flag(FGMACS))
		i = x_emacs(buf, len);
	else
#endif
#ifdef VI
	if (Flag(FVI))
		i = x_vi(buf, len);
	else
#endif
		i = -1;		/* internal error */
	x_mode(FALSE);
	return i;
}

/* tty I/O */

int
x_getc()
{
	char c;
	int n;

	while ((n = read(0, &c, 1)) < 0 && errno == EINTR)
		if (intrsig) {
			x_mode(FALSE);
			intrcheck();
			x_mode(TRUE);
		}
	if (n != 1)
		return -1;
	return c & 0x7F;
}

void
x_flush()
{
	shf_flush(shl_out);
}

void
x_putc(c)
	int c;
{
	shf_putc(c, shl_out);
}

void
x_puts(s)
	register char *s;
{
	while (*s != 0)
		shf_putc(*s++, shl_out);
}

bool_t
x_mode(onoff)
	bool_t	onoff;
{
	static bool_t	x_cur_mode = FALSE;
	bool_t		prev;

	if (x_cur_mode == onoff)
		return x_cur_mode;
	prev = x_cur_mode;
	x_cur_mode = onoff;
	if (onoff) {
		TTY_state	cb;
		X_chars		oldchars;
		
		oldchars = edchars;
		cb = tty_state;

#if defined(HAVE_TERMIOS_H) || defined(HAVE_TERMIO_H)
		edchars.erase = cb.c_cc[VERASE];
		edchars.kill = cb.c_cc[VKILL];
		edchars.intr = cb.c_cc[VINTR];
		edchars.quit = cb.c_cc[VQUIT];
# ifdef VWERASE
		edchars.werase = cb.c_cc[VWERASE];
# endif
# ifdef _CRAY2		/* brain-damaged terminal handler */
		cb.c_lflag &= ~(ICANON|ECHO);
		/* rely on print routine to map '\n' to CR,LF */
# else
		cb.c_iflag &= ~(INLCR|ICRNL);
#  ifdef _BSD_SYSV	/* need to force CBREAK instead of RAW (need CRMOD on output) */
		cb.c_lflag &= ~(ICANON|ECHO);
#  else
#   ifdef SWTCH	/* need CBREAK to handle swtch char */
		cb.c_lflag &= ~(ICANON|ECHO);
		cb.c_lflag |= ISIG;
		cb.c_cc[VINTR] = 0377;
		cb.c_cc[VQUIT] = 0377;
#   else
		cb.c_lflag &= ~(ISIG|ICANON|ECHO);
#   endif
#  endif
#  ifdef VLNEXT
		cb.c_cc[VLNEXT] = 0377; /* osf/1 processes lnext when ~icanon */
#  endif /* VLNEXT */
		cb.c_cc[VTIME] = 0;
		cb.c_cc[VMIN] = 1;
# endif	/* _CRAY2 */
#else
	/* Assume BSD tty stuff. */
		edchars.erase = cb.sgttyb.sg_erase;
		edchars.kill = cb.sgttyb.sg_kill;
		cb.sgttyb.sg_flags &= ~ECHO;
		cb.sgttyb.sg_flags |= CBREAK;
#  ifdef TIOCGATC
		edchars.intr = cb.lchars.tc_intrc;
		edchars.quit = cb.lchars.tc_quitc;
		edchars.werase = cb.lchars.tc_werasc;
		cb.lchars.tc_suspc = -1;
		cb.lchars.tc_dsuspc = -1;
		cb.lchars.tc_lnextc = -1;
		cb.lchars.tc_statc = -1;
		cb.lchars.tc_intrc = -1;
		cb.lchars.tc_quitc = -1;
		cb.lchars.tc_rprntc = -1;
#  else
		edchars.intr = cb.tchars.t_intrc;
		edchars.quit = cb.tchars.t_quitc;
		cb.tchars.t_intrc = -1;
		cb.tchars.t_quitc = -1;
#   ifdef TIOCGLTC
		edchars.werase = cb.ltchars.t_werasc;
		cb.ltchars.t_suspc = -1;
		cb.ltchars.t_dsuspc = -1;
		cb.ltchars.t_lnextc = -1;
		cb.ltchars.t_rprntc = -1;
#   endif
#  endif /* TIOCGATC */
#endif /* HAVE_TERMIOS_H || HAVE_TERMIO_H */

		set_tty(tty_fd, &cb, TF_WAIT);

		if (memcmp(&edchars, &oldchars, sizeof(edchars)) != 0) {
#ifdef EMACS
			x_emacs_keys(&edchars);
#endif
		}
	} else
		/* TF_WAIT doesn't seem to be necessary when leaving xmode */
		set_tty(tty_fd, &tty_state, TF_NONE);
	return prev;
}

/* NAME:
 *      promptlen - calculate the length of PS1 etc.
 *
 * DESCRIPTION:
 *      This function is based on a fix from guy@demon.co.uk
 *      It fixes a bug in that if PS1 contains '!', the length 
 *      given by strlen() is probably wrong.
 *
 * RETURN VALUE:
 *      length
 */
 
int
promptlen(cp)
  register char  *cp;
{
  register int count = 0;

  while (*cp)
  {
    if ( *cp == '\n' || *cp == '\r' )
      {
	count = 0;
	cp++;
      }
    else if ( *cp == '\t' )
      {
	count = (count | 7) + 1;
	cp++;
      }
    else if ( *cp == '\b' )
      {
	if (count > 0)
		count--;
	cp++;
      }
    else if ( *cp++ != '!' )
      count++;
    else if ( *cp == '!' )
      {
	cp++;
	count++;
      }
      else
      {
	register int i = source->line + 1;

	do
	{
	  count ++;
	}
	while( ( i /= 10 ) > 0 );
      }
  }
  return count;
}

void
set_editmode(ed)
	char *ed;
{
	static const enum sh_flag edit_flags[] = {
#ifdef EMACS
			FEMACS, FGMACS,
#endif
#ifdef VI
			FVI,
#endif
		    };
	char *rcp;
	int i;
  
	if ((rcp = strrchr(ed, '/')))
		ed = ++rcp;
	for (i = 0; i < NELEM(edit_flags); i++)
		if (strstr(options[(int) edit_flags[i]].name, ed)) {
			change_flag(edit_flags[i], OF_SPECIAL, 1);
			return;
		}
}
#endif /* EDIT */
