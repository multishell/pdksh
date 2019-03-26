/*
 *  Emacs-like command line editing and history
 *
 *  created by Ron Natalie at BRL
 *  modified by Doug Kingston, Doug Gwyn, and Lou Salkind
 *  adapted to PD ksh by Eric Gisin
 */

#include "config.h"
#ifdef EMACS

#if !defined(lint) && !defined(no_RCSids)
static char *RCSid = "$Id: emacs.c,v 1.3 1994/05/31 13:34:34 michael Exp $";
#endif

#include "sh.h"
#include "ksh_stat.h"
#include "ksh_dir.h"
#include <ctype.h>
#include "expand.h"
#include "edit.h"

#define PUSH_DELETE 1			/* push all deletes of >1 char */

static	Area	aedit;
#define	AEDIT	&aedit		/* area for kill ring and macro defns */

#undef CTRL			/* _BSD brain damage */
#define	CTRL(x)		((x) == '?' ? 0x7F : (x) & 0x1F)	/* ASCII */
#define	UNCTRL(x)	((x) == 0x7F ? '?' : (x) | 0x40)	/* ASCII */


/* values returned by keyboard functions */
#define	KSTD	0
#define	KPREF	1		/* ^[, ^X */
#define	KEOL	2		/* ^M, ^J */
#define	KINTR	3		/* ^G, ^C */
#define	KNULL	4
	
struct	x_ftab  {
	int	(*xf_func)();
	char	*xf_name;
	char	xf_db_tab;
	char	xf_db_char;
	short	xf_flags;
};

#define	XF_ALLOC	2
#define	XF_NOBIND	4

#define	iscfs(c)	(c == ' ' || c == '\t')	/* Separator for completion */
#define	ismfs(c)	(!(isalnum(c)|| c == '$'))  /* Separator for motion */
#define	CMASK		0x7F	/* 7-bit ASCII character mask */

#define X_TABSZ	128			/* size of keydef tables etc */

/* { from 4.9 edit.h */
/*
 * The following are used for my horizontal scrolling stuff
 */
static char   *xbuf;		/* beg input buffer */
static char   *xend;		/* end input buffer */
static char    *xcp;		/* current position */
static char    *xep;		/* current end */
static char    *xbp;		/* start of visible portion of input buffer */
static char    *xlp;		/* last char visible on screen */
static int	x_adj_ok;
/*
 * we use x_adj_done so that functions can tell 
 * whether x_adjust() has been called while they are active.
 */
static int	x_adj_done;

static int	x_col;
static int	x_displen;
static int	x_arg;		/* general purpose arg */

static int	xlp_valid;
/* end from 4.9 edit.h } */

static	int	x_prefix1 = CTRL('['), x_prefix2 = CTRL('X');
static	char   **x_histp;	/* history position */
static	char   **x_nextcmdp;	/* for newline-and-next */
static	char	*xmp;		/* mark pointer */
static	int    (*x_last_command)();
static	struct	x_ftab const *(*x_tab)[X_TABSZ] = NULL; /* key definition */
static	char    *(*x_atab)[X_TABSZ] = NULL; /* macro definitions */
#define	KILLSIZE	20
static	char    *killstack[KILLSIZE];
static	int	killsp, killtp;
static	int	x_curprefix;
static	char    *macroptr;
static	int	x_maxlen;	/* to determine column width */

static int      x_insert    ARGS((int c));
static int      x_ins_string ARGS((int c));
static void     x_ins       ARGS((char *cp));
static int      x_del_back  ARGS((int c));
static int      x_del_char  ARGS((int c));
static void     x_delete    ARGS((int nc));
static int      x_del_bword ARGS((int c));
static int      x_mv_bword  ARGS((int c));
static int      x_mv_fword  ARGS((int c));
static int      x_del_fword ARGS((int c));
static int      x_bword     ARGS((void));
static int      x_fword     ARGS((void));
static void     x_goto      ARGS((char *cp));
static void     x_bs        ARGS((int c));
static int      x_size_str  ARGS((char *cp));
static int      x_size      ARGS((int c));
static void     x_zots      ARGS((char *str));
static void     x_zotc      ARGS((int c));
static int      x_mv_back   ARGS((int c));
static int      x_mv_forw   ARGS((int c));
static int      x_search_char ARGS((int c));
static int      x_newline   ARGS((int c));
static int      x_end_of_text ARGS((int c));
static int      x_beg_hist  ARGS((int c));
static int      x_end_hist  ARGS((int c));
static int      x_prev_com  ARGS((int c));
static int      x_next_com  ARGS((int c));
static void     x_load_hist ARGS((char **hp));
static int      x_nl_next_com ARGS((int c));
static int      x_eot_del   ARGS((int c));
static int      x_search_hist ARGS((int c));
static int      x_search    ARGS((char *pat, int offset));
static int      x_match     ARGS((char *str, char *pat));
static int      x_del_line  ARGS((int c));
static int      x_mv_end    ARGS((int c));
static int      x_mv_begin  ARGS((int c));
static int      x_draw_line ARGS((int c));
static void	x_redraw    ARGS((int limit));
static int      x_transpose ARGS((int c));
static int      x_literal   ARGS((int c));
static int      x_meta1     ARGS((int c));
static int      x_meta2     ARGS((int c));
static int      x_kill      ARGS((int c));
static void     x_push      ARGS((int nchars));
static int      x_yank      ARGS((int c));
static int      x_meta_yank ARGS((int c));
static int      x_abort     ARGS((int c));
static int      x_error     ARGS((int c));
static int      x_stuffreset ARGS((int c));
static int      x_stuff     ARGS((int c));
static void     x_mapin     ARGS((char *cp));
static char *   x_mapout    ARGS((int c));
static void     x_print     ARGS((int prefix, int key));
static int      x_set_mark  ARGS((int c));
static int      x_kill_region ARGS((int c));
static int      x_xchg_point_mark ARGS((int c));
#if 0
static int      x_copy_arg  ARGS((int c));
#endif
static int      x_noop      ARGS((int c));
#ifdef SILLY
static int      x_game_of_life ARGS((int c));
#endif
static void     add_stash   ARGS((char *dirnam, char *name));
static void     list_stash  ARGS((void));
static int      x_comp_comm ARGS((int c));
static int      x_list_comm ARGS((int c));
static int      x_complete  ARGS((int c));
static int      x_enumerate ARGS((int c));
static int      x_comp_file ARGS((int c));
static int      x_list_file ARGS((int c));
static int      x_comp_list ARGS((int c));
static void     compl_dec   ARGS((int type));
static void     compl_file  ARGS((int type));
static void     compl_command ARGS((int type));
static int      strmatch    ARGS((char *s1, char *s2));
static void	x_adjust    ARGS((void));
static void	x_e_putc    ARGS((int c));
#ifdef DEBUG
static int	x_debug_info ARGS((void));
#endif /* DEBUG */
static void	x_e_puts    ARGS((char *s));
static int      x_set_arg   ARGS((int c));
static int      x_prev_histword ARGS((void));
static int      x_fold_case ARGS((int c));
static char	*x_lastcp ARGS(());


static	struct x_ftab const x_ftab[] = {

#define xft_insert	&x_ftab[0]
#define xft_error	&x_ftab[1]
#define xft_erase	&x_ftab[2]
#define xft_werase	&x_ftab[3]
#define xft_kill	&x_ftab[4]
#define xft_intr	&x_ftab[5]
#define xft_quit	&x_ftab[6]
#define xft_ins_string	&x_ftab[7]

/*0*/ 	{x_insert,	"auto-insert",		0,	0,	0 },
/*1*/	{x_error,	"error",		0,	0,	0 },
/*2*/	{x_del_back,	"delete-char-backward",	0, CTRL('?'),	0 },
/*3*/	{x_del_bword,	"delete-word-backward",	1, CTRL('?'),	0 },
/*4*/	{x_del_line,	"kill-line",		0, 	0,	0 },
/*5*/	{x_abort,	"abort",		0,	0,	0 },
/*6*/	{x_noop,	"no-op",		0,	0,	0 },
/*7*/	{x_ins_string,	"macro-string",		0,	0,	XF_NOBIND|XF_ALLOC},

/* Do not move the above! */
	{x_del_char,	"delete-char-forward",	0,	0,	0 },
	{x_eot_del,	"eot-or-delete",	0, CTRL('D'),	0 },
	{x_del_back,	"delete-char-backward",	0, CTRL('H'),	0 },
	{x_del_bword,	"delete-word-backward",	1, CTRL('H'),	0 },
	{x_mv_bword,	"backward-word", 	1,	'b',	0 },
	{x_mv_fword,	"forward-word",		1,	'f',	0 },
	{x_del_fword,	"delete-word-forward", 	1,	'd',	0 },
	{x_mv_back,	"backward-char",	0, CTRL('B'),	0 },
	{x_mv_forw,	"forward-char",		0, CTRL('F'),	0 },
	{x_search_char,	"search-character",	0, CTRL(']'),	0 },
	{x_newline,	"newline",		0, CTRL('M'),	0 },
	{x_newline,	"newline",		0, CTRL('J'),	0 },
	{x_end_of_text,	"eot",			0, CTRL('_'),	0 },
	{x_abort,	"abort",		0, CTRL('G'),	0 },
	{x_prev_com,	"up-history",		0, CTRL('P'),	0},
	{x_next_com,	"down-history",		0, CTRL('N'),	0},
	{x_search_hist,	"search-history",	0, CTRL('R'),	0},
	{x_beg_hist,	"beginning-of-history",	1,	'<',	0},
	{x_end_hist,	"end-of-history",	1,	'>',	0},
	{x_mv_end,	"end-of-line",		0, CTRL('E'),	0 },
	{x_mv_begin,	"beginning-of-line",	0, CTRL('A'),	0 },
	{x_draw_line,	"redraw",		0, CTRL('L'),	0 },
	{x_meta1,	"prefix-1",		0, CTRL('['),	0 },
	{x_meta2,	"prefix-2",		0, CTRL('X'),	0 },
	{x_kill,	"kill-to-eol",		0, CTRL('K'),	0 },
	{x_yank,	"yank",			0, CTRL('Y'),	0 },
	{x_meta_yank,	"yank-pop", 		1,	'y',	0 },
	{x_literal,	"quote",		0, CTRL('^'),	0 },
	{x_stuffreset, 	"stuff-reset",		0,	 0,	0 },
#if defined(BRL) && defined(TIOCSTI)
	{x_stuff, 	"stuff",		0, CTRL('T'),	0 },
	{x_transpose,	"transpose-chars",	0,	 0,	0 },
#else
	{x_stuff, 	"stuff",		0,	 0,	0 },
	{x_transpose,	"transpose-chars",	0, CTRL('T'),	0 },
#endif
	{x_complete,	"complete",		1, CTRL('['),	0 },
        {x_comp_list,	"complete-list",	1,	'=',	0 },
	{x_enumerate,	"list",			1,	'?',	0 },
	{x_comp_file,	"complete-file",	1, CTRL('X'),	0 },
	{x_comp_comm,	"complete-command",	2, CTRL('['),	0 },
	{x_list_file,	"list-file",		0,	 0,	0 },
	{x_list_comm,	"list-command",		2,	'?',	0 },
	{x_nl_next_com,	"newline-and-next",	0, CTRL('O'),	0 },
	{x_set_mark,	"set-mark-command",	1,	' ',	0 },
	{x_kill_region,	"kill-region",		0, CTRL('W'),	0 },
	{x_xchg_point_mark, "exchange-point-and-mark", 2, CTRL('X'), 0 },
#if 0
	{x_copy_arg,	"copy-last-arg",	1,	'_',	0},
#endif
#ifdef SILLY
	{x_game_of_life, "play-game-of-life",	0,	0,	0 },
#endif 
#ifdef DEBUG
        {x_debug_info,	"debug-info",		1, CTRL('H'),	0 },
#endif
	{x_prev_histword, "prev-hist-word", 	1,	'.',	0 },
	{x_prev_histword, "prev-hist-word", 	1,	'_',	0 },
        {x_set_arg,	"",			1,	'0',	0 },
        {x_set_arg,	"",			1,	'1',	0 },
        {x_set_arg,	"",			1,	'2',	0 },
        {x_set_arg,	"",			1,	'3',	0 },
        {x_set_arg,	"",			1,	'4',	0 },
        {x_set_arg,	"",			1,	'5',	0 },
        {x_set_arg,	"",			1,	'6',	0 },
        {x_set_arg,	"",			1,	'7',	0 },
        {x_set_arg,	"",			1,	'8',	0 },
        {x_set_arg,	"",			1,	'9',	0 },
        {x_fold_case,	"upcase-word",		1,	'U',	0 },
        {x_fold_case,	"downcase-word",	1,	'L',	0 },
        {x_fold_case,	"capitalize-word",	1,	'C',	0 },
        {x_fold_case,	"upcase-word",		1,	'u',	0 },
        {x_fold_case,	"downcase-word",	1,	'l',	0 },
        {x_fold_case,	"capitalize-word",	1,	'c',	0 },
	{ 0 }
};

int
x_emacs(buf, len)
	char *buf;
	size_t len;
{
	char	c;
	int	i;
	int   (*func)();
	extern	x_insert();

	xbp = xbuf = buf; xend = buf + len;
	xlp = xcp = xep = buf;
	*xcp = 0;
	xlp_valid = TRUE;
	xmp = NULL;
	x_curprefix = 0;
	macroptr = null;
	x_histp = histptr + 1;

	if (x_nextcmdp != NULL) {
		x_load_hist(x_nextcmdp);
		x_nextcmdp = NULL;
	}

	x_col = promptlen(prompt);
	x_adj_ok = 1;
	x_displen = x_cols - 2 - x_col;
	x_adj_done = 0;

	while (1)  {
		x_flush();
		if (*macroptr)  {
			c = *macroptr++;
			if (*macroptr == 0)
				macroptr = null;
		}
		else {
			if ((c = x_getc()) < 0)
				return 0;
		}

		if (x_curprefix == -1)
			func = x_insert;
		else
			func = x_tab[x_curprefix][c&CMASK]->xf_func;
		if (func == NULL)
			func = x_error;
		i = c | (x_curprefix << 8);
		x_curprefix = 0;
		switch (i = (*func)(i))  {
		  case KSTD:
			x_last_command = func;
		  case KPREF:
		  case KNULL:
			break;
		  case KEOL:
			i = xep - xbuf;
			x_last_command = 0;
			return i;
		  case KINTR:	/* special case for interrupt */
			trapsig(SIGINT);
			runtraps(TRUE);
			break;
		}
	}
}

static int
x_insert(c)
	int c;
{
	char	str[2];

	/*
	 *  Should allow tab and control chars.
	 */
	if (c == 0)  {
		x_e_putc(BEL);
		return KSTD;
	}
	str[0] = c;
	str[1] = '\0';
	x_ins(str);
	return KSTD;
}

static int
x_ins_string(c)
	int c;
{
	if (*macroptr)   {
		x_e_putc(BEL);
		return KSTD;
	}
	macroptr = x_atab[c>>8][c & CMASK];
	return KSTD;
}

static void
x_ins(cp)
	char	*cp;
{
	int	count;
	register int	adj = x_adj_done;

	count = strlen(cp);
	if (xep+count >= xend) {
		x_e_putc(BEL);
		return;
	}

	if (xcp != xep)
		memmove(xcp+count, xcp, xep - xcp + 1);
	else
		xcp[count] = '\0';
	memmove(xcp, cp, count);
	/*
	 * x_zots() may result in a call to x_adjust()
	 * we want xcp to reflect the new position.
	 */
	cp = xcp;
	xcp += count;
	xep += count;
	xlp_valid = FALSE;
	x_lastcp();
	x_adj_ok = (xcp >= xlp);
	x_zots(cp);
	if (adj == x_adj_done)	/* has x_adjust() been called? */
	{
	  /* no */
	  for (cp = xlp; cp > xcp; )
	    x_bs(*--cp);
	}

	x_adj_ok = 1;
	return;
}

static int
x_del_back(c)
	int c;
{
	if (xcp == xbuf)  {
		x_e_putc(BEL);
		return KSTD;
	}
	x_goto(xcp - 1);
	x_delete(1);
	return KSTD;
}

static int
x_del_char(c)
	int c;
{
	if (xcp == xep)  {
		x_e_putc(BEL);
		return KSTD;
	}
	x_delete(1);
	return KSTD;
}

static void
x_delete(nc)
	int nc;
{
	int	i,j;
	char	*cp;
	
	if (nc == 0)
		return;
	if (xmp != NULL) {
		if (xcp + nc > xmp)
			xmp = xcp;
		else if (xmp > xcp)
			xmp -= nc;
	}
#ifdef PUSH_DELETE
	/*
	 * This lets us yank a word we have deleted.
	 */
	if (nc > 1)
	  x_push(nc);
#endif
	xep -= nc;
	cp = xcp;
	j = 0;
	i = nc;
	while (i--)  {
		j += x_size(*cp++);
	}
	memmove(xcp, xcp+nc, xep - xcp + 1);	/* Copies the null */
	x_adj_ok = 0;			/* don't redraw */
	x_zots(xcp);
	/*
	 * if we are already filling the line,
	 * there is no need to ' ','\b'.
	 * But if we must, make sure we do the minimum.
	 */
	if ((i = x_cols - 2 - x_col) > 0)
	{
	  j = (j < i) ? j : i;
	  i = j;
	  while (i--)
	    x_e_putc(' ');
	  i = j;
	  while (i--)
	    x_e_putc('\b');
	}
	/*x_goto(xcp);*/
	x_adj_ok = 1;
	xlp_valid = FALSE;
	for (cp = x_lastcp(); cp > xcp; )
	  x_bs(*--cp);

	return;	
}

static int
x_del_bword(c)
	int c;
{
	x_delete(x_bword());
	return KSTD;
}

static int
x_mv_bword(c)
	int c;
{
	(void)x_bword();
	return KSTD;
}

static int
x_mv_fword(c)
	int c;
{
	x_goto(xcp + x_fword());
	return KSTD;
}

static int
x_del_fword(c)
	int c;
{
	x_delete(x_fword());
	return KSTD;
}

static int
x_bword()
{
	int	nc = 0;
	register char *cp = xcp;

	if (cp == xbuf)  {
		x_e_putc(BEL);
		return 0;
	}
	if (x_last_command != x_set_arg)
	  x_arg = 1;
	while (x_arg--)
	{
	  while (cp != xbuf && ismfs(cp[-1]))
	  {
	    cp--;
	    nc++;
	  }
	  while (cp != xbuf && !ismfs(cp[-1]))
	  {
	    cp--;
	    nc++;
	  }
	}
	x_goto(cp);
	return nc;
}

static int
x_fword()
{
	int	nc = 0;
	register char	*cp = xcp;

	if (cp == xep)  {
		x_e_putc(BEL);
		return 0;
	}
	if (x_last_command != x_set_arg)
	  x_arg = 1;
	while (x_arg--)
	{
	  while (cp != xep && !ismfs(*cp))
	  {
	    cp++;
	    nc++;
	  }
	  while (cp != xep && ismfs(*cp))
	  {
	    cp++;
	    nc++;
	  }
	}
	return nc;
}

static void
x_goto(cp)
	register char *cp;
{
  if (cp < xbp || cp >= (xbp + x_displen))
  {
    /* we are heading off screen */
    xcp = cp;
    x_adjust();
  }
  else
  {
    if (cp < xcp)		/* move back */
    {
      while (cp < xcp)
	x_bs(*--xcp);
    }
    else
    {
      if (cp > xcp) 		/* move forward */
      {
	while (cp > xcp)
	  x_zotc(*xcp++);
      }
    }
  }
}

static void
x_bs(c)
	int c;
{
	register i;
	i = x_size(c);
	while (i--)
		x_e_putc('\b');
}

static int
x_size_str(cp)
	register char *cp;
{
	register size = 0;
	while (*cp)
		size += x_size(*cp++);
	return size;
}

static int
x_size(c)
	int c;
{
	if (c=='\t')
		return 4;	/* Kludge, tabs are always four spaces. */
	if (c < ' ' || c == 0x7F) /* ASCII control char */
		return 2;
	return 1;
}

static void
x_zots(str)
	register char *str;
{
  register int	adj = x_adj_done;

  x_lastcp();
  while (*str && str < xlp && adj == x_adj_done)
    x_zotc(*str++);
}

static void
x_zotc(c)
	int c;
{
	if (c == '\t')  {
		/*  Kludge, tabs are always four spaces.  */
		x_e_puts("    ");
	} else if (c < ' ' || c == 0x7F)  { /* ASCII */
		x_e_putc('^');
		x_e_putc(UNCTRL(c));
	} else
		x_e_putc(c);
}

static int
x_mv_back(c)
	int c;
{
	if (xcp == xbuf)  {
		x_e_putc(BEL);
		return KSTD;
	}
	x_goto(xcp-1);
	return KSTD;
}

static int
x_mv_forw(c)
	int c;
{
	if (xcp == xep)  {
		x_e_putc(BEL);
		return KSTD;
	}
	x_goto(xcp+1);
	return KSTD;
}

static int
x_search_char(c)
	int c;
{
	char *cp;

	*xep = '\0';
	if ((c = x_getc()) < 0 ||
	    /* we search forward, I don't know what Korn does */
	    ((cp = (xcp == xep) ? NULL : strchr(xcp+1, c)) == NULL &&
	    (cp = strchr(xbuf, c)) == NULL)) {
		x_e_putc(BEL);
		return KSTD;
	}
	x_goto(cp);
	return KSTD;
}

static int
x_newline(c)
	int c;
{
	x_e_putc('\r');
	x_e_putc('\n');
	x_flush();
	*xep++ = '\n';
	return KEOL;
}

static int
x_end_of_text(c)
	int c;
{
#if 0
	x_store_hist();
#endif
	return KEOL;
}

static int x_beg_hist(c) int c; {x_load_hist(history); return KSTD;}

static int x_end_hist(c) int c; {x_load_hist(histptr); return KSTD;}

static int x_prev_com(c) int c; {x_load_hist(x_histp-1); return KSTD;}

static int x_next_com(c) int c; {x_load_hist(x_histp+1); return KSTD;}

static void
x_load_hist(hp)
	register char **hp;
{
	int	oldsize;

	if (hp < history || hp > histptr) {
		x_e_putc(BEL);
		return;
	}
	x_histp = hp;
	oldsize = x_size_str(xbuf);
	(void)strcpy(xbuf, *hp);
	xbp = xbuf;
	xep = xcp = xbuf + strlen(*hp);
	xlp_valid = FALSE;
	if (xep > x_lastcp())
	  x_goto(xep);
	else
	  x_redraw(oldsize);
}

static int
x_nl_next_com(c)
	int	c;
{
	x_nextcmdp = x_histp + 1;
	return (x_newline(c));
}

static int
x_eot_del(c)
	int	c;
{
	if (xep == xbuf)
		return (x_end_of_text(c));
	else
		return (x_del_char(c));
}

/* reverse incremental history search */
static int
x_search_hist(c)
	int c;
{
	int offset = -1;	/* offset of match in xbuf, else -1 */
	char pat [256+1];	/* pattern buffer */
	register char *p = pat;
	int (*func)();

	*p = 0;
	while (1) {
		if (offset < 0) {
			x_e_puts("\nI-search: ");
			x_zots(pat);
		}
		x_flush();
		if ((c = x_getc()) < 0)
			return KSTD;
		func = x_tab[0][c&CMASK]->xf_func;
		if (c == CTRL('['))
			break;
		else if (func == x_search_hist)
			offset = x_search(pat, offset);
		else if (func == x_del_back)
			continue;	/* todo */
		else if (func == x_insert) {
			/* add char to pattern */
			*p++ = c, *p = 0;
			if (offset >= 0) {
				/* already have partial match */
				offset = x_match(xbuf, pat);
				if (offset >= 0) {
					x_goto(xbuf + offset + (p - pat) - (*pat == '^'));
					continue;
				}
			}
			offset = x_search(pat, offset);
		} else { /* other command */
			static char push[2];
			push[0] = c;
			macroptr = push; /* push command */
			break;
		}
	}
	if (offset < 0)
		x_redraw(-1);
	return KSTD;
}

/* search backward from current line */
static int
x_search(pat, offset)
	char *pat;
	int offset;
{
	register char **hp;
	int i;

	for (hp = x_histp; --hp >= history; ) {
		i = x_match(*hp, pat);
		if (i >= 0) {
			if (offset < 0)
				x_e_putc('\n');
			x_load_hist(hp);
			x_goto(xbuf + i + strlen(pat) - (*pat == '^'));
			return i;
		}
	}
	x_e_putc(BEL);
	x_histp = histptr;
	return -1;
}

/* return position of first match of pattern in string, else -1 */
static int
x_match(str, pat)
	char *str, *pat;
{
	if (*pat == '^') {
		return (strncmp(str, pat+1, strlen(pat+1)) == 0) ? 0 : -1;
	} else {
		char *q = strstr(str, pat);
		return (q == NULL) ? -1 : q - str;
	}
}

static int
x_del_line(c)
	int c;
{
	int	i, j;

	*xep = 0;
	i = xep- xbuf;
	j = x_size_str(xbuf);
	xcp = xbuf;
	x_push(i);
	xlp = xbp = xep = xbuf;
	xlp_valid = TRUE;
	*xcp = 0;
	xmp = NULL;
	x_redraw(j);
	return KSTD;
}

static int
x_mv_end(c)
	int c;
{
	x_goto(xep);
	return KSTD;
}

static int
x_mv_begin(c)
	int c;
{
	x_goto(xbuf);
	return KSTD;
}

static int
x_draw_line(c)
	int c;
{
	x_redraw(-1);
	return KSTD;

}

static void
x_redraw(limit)
  int limit;
{
	int	i, j;
	char	*cp;
	
	x_adj_ok = 0;
	if (limit == -1)
		x_e_putc('\n');
	else 
		x_e_putc('\r');
	x_flush();
	if (xbp == xbuf)
	{
	  pprompt(prompt);
	  x_col = promptlen(prompt);
	}
	x_displen = x_cols - 2 - x_col;
	xlp_valid = FALSE;
	cp = x_lastcp();
	x_zots(xbp);
	if (xbp != xbuf || xep > xlp)
	  limit = x_cols;
	if (limit >= 0)
	{
	  if (xep > xlp)
	    i = 0;			/* we fill the line */
	  else
	    i = limit - (xlp - xbp);

	  for (j = 0; j < i && x_col < (x_cols - 2); j++)
	    x_e_putc(' ');
	  i = ' ';
	  if (xep > xlp)		/* more off screen */
	  {
	    if (xbp > xbuf)
	      i = '*';
	    else
	      i = '>';
	  }
	  else
	    if (xbp > xbuf)
	      i = '<';
	  x_e_putc(i);
	  j++;
	  while (j--)
	    x_e_putc('\b');
	}
	for (cp = xlp; cp > xcp; )
	  x_bs(*--cp);
	x_adj_ok = 1;
	_D_(x_flush();)
	return;
}

static int
x_transpose(c)
	int c;
{
	char	tmp;
	if (xcp == xbuf) {
		x_e_putc(BEL);
		return KSTD;
	} else if (xcp == xep) {
		if (xcp - xbuf == 1) {
			x_e_putc(BEL);
			return KSTD;
		}
		x_bs(xcp[-1]);
		x_bs(xcp[-2]);
		x_zotc(xcp[-1]);
		x_zotc(xcp[-2]);
		tmp = xcp[-1];
		xcp[-1] = xcp[-2];
		xcp[-2] = tmp;
	} else {
		x_bs(xcp[-1]);
		x_zotc(xcp[0]);
		x_zotc(xcp[-1]);
		tmp = xcp[-1];
		xcp[-1] = xcp[0];
		xcp[0] = tmp;
		x_bs(xcp[0]);
		x_goto(xcp + 1);
	}
	return KSTD;
}

static int
x_literal(c)
	int c;
{
	x_curprefix = -1;
	return KSTD;
}

static int
x_meta1(c)
	int c;
{
	x_curprefix = 1;
	return KPREF;
}

static int
x_meta2(c)
	int c;
{
	x_curprefix = 2;
	return KPREF;
}

static int
x_kill(c)
	int c;
{
	int	i;

	i = xep - xcp;
	xlp = xcp;
	xlp_valid = TRUE;
	x_push(i);
	x_delete(i);
	return KSTD;
}

static void
x_push(nchars)
	int nchars;
{
	char	*cp = strnsave(xcp, nchars, AEDIT);
	if (killstack[killsp])
		afree((void *)killstack[killsp], AEDIT);
	killstack[killsp] = cp;
	killsp = (killsp + 1) % KILLSIZE;
}

static int
x_yank(c)
	int c;
{
	if (killsp == 0)
		killtp = KILLSIZE;
	else
		killtp = killsp;
	killtp --;
	if (killstack[killtp] == 0)  {
		x_e_puts("\nnothing to yank");
		x_redraw(-1);
		return KSTD;
	}
	xmp = xcp;
	x_ins(killstack[killtp]);
	return KSTD;
}

static int
x_meta_yank(c)
	int c;
{
	int	len;
	if (x_last_command != x_yank && x_last_command != x_meta_yank)  {
		x_e_puts("\nyank something first");
		x_redraw(-1);
		return KSTD;
	}
	len = strlen(killstack[killtp]);
	x_goto(xcp - len);
	x_delete(len);
	do  {
		if (killtp == 0)
			killtp = KILLSIZE - 1;
		else
			killtp--;
	}  while (killstack[killtp] == 0);
	x_ins(killstack[killtp]);
	return KSTD;
}

static int
x_abort(c)
	int c;
{
	/* x_zotc(c); */
	xlp = xep = xcp = xbp = xbuf;
	xlp_valid = TRUE;
	*xcp = 0;
	return KINTR;
}

static int
x_error(c)
	int c;
{
	x_e_putc(BEL);
	return KSTD;
}

static int
x_stuffreset(c)
	int c;
{
#ifdef TIOCSTI
	(void)x_stuff(c);
	return KINTR;
#else
	x_zotc(c);
	xlp = xcp = xep = xbp = xbuf;
	xlp_valid = TRUE;
	*xcp = 0;
	x_redraw(-1);
	return KSTD;
#endif
}

static int
x_stuff(c)
	int c;
{
#if 0 || defined TIOCSTI
	char	ch = c;
	bool_t	savmode = x_mode(FALSE);

	(void)ioctl(TTY, TIOCSTI, &ch);
	(void)x_mode(savmode);
	x_redraw(-1);
#endif
	return KSTD;
}

static void
x_mapin(cp)
	char	*cp;
{
	char	*op;

	op = cp;
	while (*cp)  {
		/* XXX -- should handle \^ escape? */
		if (*cp == '^')  {
			cp++;
			if (*cp >= '?')	/* includes '?'; ASCII */
				*op++ = CTRL(*cp);
			else  {
				*op++ = '^';
				cp--;
			}
		} else
			*op++ = *cp;
		cp++;
	}
	*op = 0;
}

static char *
x_mapout(c)
	int c;
{
	static char buf[8];
	register char *p = buf;

	if (c < ' ' || c == 0x7F)  { /* ASCII */
		*p++ = '^';
		*p++ = (c == 0x7F) ? '?' : (c | 0x40);
	} else
		*p++ = c;
	*p = 0;
	return buf;
}

static void
x_print(prefix, key)
	int prefix, key;
{
	if (prefix == 1)
		shprintf("%s", x_mapout(x_prefix1));
	if (prefix == 2)
		shprintf("%s", x_mapout(x_prefix2));
	shprintf("%s = ", x_mapout(key));
	if (x_tab[prefix][key]->xf_func != x_ins_string)
		shprintf("%s\n", x_tab[prefix][key]->xf_name);
	else
		shprintf("'%s'\n", x_atab[prefix][key]);
}

void
x_bind(a1, a2, macro)
	char *a1, *a2;
	int macro;		/* bind -m */
{
	struct x_ftab const *fp;
	int prefix, key;
	char *sp = NULL;

	if (x_tab == NULL)
		errorf("cannot bind, not a tty\n");

	if (a1 == NULL) {
		for (prefix = 0; prefix < 3; prefix++)
		    for (key = 0; key < 0x80; key++) {
			fp = x_tab[prefix][key];
			if (fp == NULL ||
			    fp->xf_func == x_insert || fp->xf_func == x_error)
				continue;
			x_print(prefix, key);
		    }
		return;
	}

	x_mapin(a1);
	prefix = key = 0;
	for (;; a1++) {
		key = *a1;
		if (x_tab[prefix][key]->xf_func == x_meta1)
			prefix = 1;
		else
		if (x_tab[prefix][key]->xf_func == x_meta2)
			prefix = 2;
		else
			break;
	}

	if (a2 == NULL) {
		x_print(prefix, key);
		return;
	}

	if (*a2 == 0)
		fp = xft_insert;
	else if (!macro) {
		for (fp = x_ftab; fp->xf_func; fp++)
			if (strcmp(fp->xf_name, a2) == 0)
				break;
		if (fp->xf_func == NULL || (fp->xf_flags & XF_NOBIND))
			errorf("%s: no such function\n", a2);
		if (fp->xf_func == x_meta1)
			x_prefix1 = key;
		if (fp->xf_func == x_meta2)
			x_prefix2 = key;
	} else {
		fp = xft_ins_string;
		x_mapin(a2);
		sp = strsave(a2, AEDIT);
	}

	if ((x_tab[prefix][key]->xf_flags & XF_ALLOC) && x_atab[prefix][key])
		afree((void *)x_atab[prefix][key], AEDIT);
	x_tab[prefix][key] = fp;
	x_atab[prefix][key] = sp;
}

void
x_init_emacs()
{
	register int i, j;
	struct x_ftab const *fp;

	ainit(AEDIT);

	x_tab = (struct x_ftab const *(*)[X_TABSZ]) alloc(sizeofN(*x_tab, 3), AEDIT);
	for (j = 0; j < 128; j++)
		x_tab[0][j] = xft_insert;
	for (i = 1; i < 3; i++)
		for (j = 0; j < 128; j++)
			x_tab[i][j] = xft_error;
	for (fp = x_ftab; fp->xf_func; fp++)
		if (fp->xf_db_char || fp->xf_db_tab)
			x_tab[fp->xf_db_tab][fp->xf_db_char] = fp;

	x_atab = (char *(*)[X_TABSZ]) alloc(sizeofN(*x_atab, 3), AEDIT);
	for (i = 1; i < 3; i++)
		for (j = 0; j < 128; j++)
			x_atab[i][j] = NULL;
}

void
x_emacs_keys(ec)
	X_chars *ec;
{
	x_tab[0][ec->erase] = xft_erase;
	x_tab[0][ec->kill] = xft_kill;
	x_tab[0][ec->werase] = xft_werase;
	x_tab[0][ec->intr] = xft_intr;
	x_tab[0][ec->quit] = xft_quit;
	x_tab[1][ec->erase] = xft_werase;
}

static int
x_set_mark(c)
	int c;
{
	xmp = xcp;
	return KSTD;
}

static int
x_kill_region(c)
	int c;
{
	int	rsize;
	char	*xr;

	if (xmp == NULL) {
		x_e_putc(BEL);
		return KSTD;
	}
	if (xmp > xcp) {
		rsize = xmp - xcp;
		xr = xcp;
	} else {
		rsize = xcp - xmp;
		xr = xmp;
	}
	x_goto(xr);
	x_push(rsize);
	x_delete(rsize);
	xmp = xr;
	return KSTD;
}

static int
x_xchg_point_mark(c)
	int c;
{
	char	*tmp;

	if (xmp == NULL) {
		x_e_putc(BEL);
		return KSTD;
	}
	tmp = xmp;
	xmp = xcp;
	x_goto( tmp );
	return KSTD;
}

#if 0
static int
x_copy_arg(c)
	int c;
{
	char *last;
	if ((last = strval(local("_"))) && *last)
		x_ins(last);
	return KSTD;
}
#endif

static int
x_noop(c)
	int c;
{
	return KNULL;
}

#ifdef SILLY
static int
x_game_of_life(c)
	int c;
{
	char	newbuf [256+1];
	register char *ip, *op;
	int	i, len;

	i = xep - xbuf;
	*xep = 0;
	len = x_size_str(xbuf);
	xcp = xbp = xbuf;
	memmove(newbuf+1, xbuf, i);
	newbuf[0] = 'A';
	newbuf[i] = 'A';
	for (ip = newbuf+1, op = xbuf; --i >= 0; ip++, op++)  {
		/*  Empty space  */
		if (*ip < '@' || *ip == '_' || *ip == 0x7F)  {
			/*  Two adults, make whoopee */
			if (ip[-1] < '_' && ip[1] < '_')  {
				/*  Make kid look like parents.  */
				*op = '`' + ((ip[-1] + ip[1])/2)%32;
				if (*op == 0x7F) /* Birth defect */
					*op = '`';
			}
			else
				*op = ' ';	/* nothing happens */
			continue;
		}
		/*  Child */
		if (*ip > '`')  {
			/*  All alone, dies  */
			if (ip[-1] == ' ' && ip[1] == ' ')
				*op = ' ';
			else	/*  Gets older */
				*op = *ip-'`'+'@';
			continue;
		}
		/*  Adult  */
		/*  Overcrowded, dies */
		if (ip[-1] >= '@' && ip[1] >= '@')  {
			*op = ' ';
			continue;
		}
		*op = *ip;
	}
	*op = 0;
	x_redraw(len);
	return KSTD;
}
#endif

/*
 *	File/command name completion routines
 */

/* type: 0 for list, 1 for completion */

static	XPtrV words;

static void
add_stash(dirnam, name)
	char *dirnam;	/* directory name, if file */
	char *name;
{
	char *cp;
	register int type = 0;	/* '*' if executable, '/' if directory, else 0 */
	register int len = strlen(name);

	/* determine file type */
	if (dirnam)  {
		struct stat statb;
		char *buf = alloc((size_t)(strlen(dirnam)+len+2), ATEMP);

		if (strcmp(dirnam, ".") == 0)
			*buf = '\0';
		else if (strcmp(dirnam, "/") == 0)
			(void)strcpy(buf, "/");
		else
			(void)strcat(strcpy(buf, dirnam), "/");
		(void)strcat(buf, name);
		if (stat(buf, &statb)==0)
			if (S_ISDIR(statb.st_mode))
				type = '/';
			else if (S_ISREG(statb.st_mode) && access(buf, X_OK)==0)
				type = '*';
		if (type)
			++len;
		afree((void *)buf, ATEMP);
	}

	if (len > x_maxlen)
		x_maxlen = len;

	/* stash name for later sorting */
	cp = strnsave(name, len, ATEMP);
	if (dirnam && type)  {	/* append file type indicator */
		cp[len-1] = type;
		cp[len] = '\0';
	}
	XPput(words, cp);
}

static void
list_stash()
{
	register char **array, **record;
	int items = 0, tabstop, loc, nrows, jump, offset;

	items = XPsize(words);
	array = (char**) XPptrv(words);
	if (items == 0)
		return;
	qsortp(XPptrv(words), (size_t)XPsize(words), xstrcmp);

	/* print names */
	x_maxlen = (x_maxlen/8 + 1) * 8;	/* column width */
	nrows = (items-1) / (x_cols/x_maxlen) + 1;
	for (offset = 0; offset < nrows; ++offset)  {
		tabstop = loc = 0;
		x_e_putc('\n');
		for (jump = 0; offset+jump < items; jump += nrows)  {
			if (jump)
				while (loc < tabstop)  {
					x_e_putc('\t');
					loc = (loc/8 + 1) * 8;
				}
			record = array + (offset + jump);
			x_e_puts(*record);
			loc += strlen(*record);
			tabstop += x_maxlen;	/* next tab stop */
			afree((void *)*record, ATEMP);
		}
	}

	afree((void*)array, ATEMP);
	x_redraw(-1);
}

static int
x_comp_comm(c)
	int c;
{
	compl_command(1);
	return KSTD;
}
static int
x_list_comm(c)
	int c;
{
	compl_command(0);
	return KSTD;
}
static int
x_complete(c)
	int c;
{
	compl_dec(1);
	return KSTD;
}
static int
x_enumerate(c)
	int c;
{
	compl_dec(0);
	return KSTD;
}
static int
x_comp_file(c)
	int c;
{
	compl_file(1);
	return KSTD;
}
static int
x_list_file(c)
	int c;
{
	compl_file(0);
	return KSTD;
}
static int
x_comp_list(c)
	int c;
{
	compl_dec(2);
	return KSTD;
}

static void
compl_dec(type)
	int type;
{
	char	*cp = xcp; 
	while (cp != xbuf && !iscfs(*cp))
		cp--;
	if (cp == xbuf && strchr(cp, '/') == NULL)
		compl_command(type);
	else
		compl_file(type);
}

static void
compl_file(type)
	int type;
{
	char	*str;
	register char *cp, *xp;
	char	*lastp;
	char	*dirnam;
	char	buf [256+1];
	char	bug [256+1];
	DIR    *dirp;
	struct dirent *dp;
	long	loc = -1;
	int	len;
	int	multi = 0;

	/* type == 0 for list, 1 for complete and 2 for complete-list */
	str = xcp;
	cp = buf;
	xp = str;
	while (xp != xbuf)  {
		--xp;
		if (iscfs(*xp))  {
			xp++;
			break;
		}
	}
	if (digit(*xp) && (xp[1] == '<' || xp[1] == '>'))
		xp++;
	while (*xp == '<' || *xp == '>')
		xp++;
	if (type) {			/* for complete */
		while (*xcp && !iscfs(*xcp))
			x_zotc(*xcp++);
	}
	if (type != 1) {		/* for list */
		x_maxlen = 0;
		XPinit(words, 16);
	}
	while (*xp && !iscfs(*xp))
		*cp++ = *xp++;

	*cp = 0;
	strcpy(buf, cp = substitute(buf, DOTILDE));
	afree((void*)cp, ATEMP);
	lastp = strrchr(buf, '/');
	if (lastp)
		*lastp = 0;

	dirnam = (lastp == NULL) ? "." : (lastp == buf) ? "/" : buf;
	dirp = ksh_opendir(dirnam);
	if (dirp == NULL) {
		x_e_putc(BEL);
		return;
	}

	if (lastp == NULL)
		lastp = buf;
	else
		lastp++;
	len = strlen(lastp);

	while ((dp = readdir(dirp)) != NULL)  {
		cp = dp->d_name;
		if (cp[0] == '.' &&
		    (cp[1] == '\0' || (cp[1] == '.' && cp[2] == '\0')))
			continue;	/* always ignore . and .. */
		if (strncmp(lastp, cp, len) == 0) {
			if (type	/* for complete */) {
				if (loc == -1)  {
					(void)strcpy(bug, cp);
					loc = strlen(cp);
				} else {
					multi = 1;
					loc = strmatch(bug, cp);
					bug[loc] = '\0';
				}
			}
			if (type != 1) { /* for list */
				add_stash(dirnam, cp);
			}
		}
	}
	(void)closedir(dirp);

	if (type) {			/* for complete */
		if (loc < 0 ||
		    (loc == 0 && type != 2))  {
			x_e_putc(BEL);
			return;
		}
		cp = bug + len;
		x_ins(cp);
		if (!multi)  {
			struct stat statb;
			if (lastp == buf)
				buf[0] = '\0';
			else if (lastp == buf + 1)  {
				buf[1] = '\0';
				buf[0] = '/';
			} else
				(void)strcat(buf, "/");
			(void)strcat(buf, bug);
			if (stat(buf, &statb) == 0 && S_ISDIR(statb.st_mode))
				x_ins("/");
			else
				x_ins(" ");
		}
	}
	if (type == 0 ||		/* if list */
	    (type == 2 && multi)) {	/* or complete-list and ambiguous */
		list_stash();
	}
}

static void
compl_command(type)
	int type;
{
	register struct tbl *tp;
	char	*str;
	char	buf [256+1];
	char	bug [256+1];
	char	*xp;
	char	*cp;
	int  len;
	int  multi;
	int  loc;

	/* type == 0 for list, 1 for complete and 2 for complete-list */
	str = xcp;
	cp = buf;
	xp = str;
	while (xp != xbuf)  {
		--xp;
		if (iscfs(*xp))  {
			xp++;
			break;
		}
	}
	if (type)			/* for complete */
		while (*xcp && !iscfs(*xcp))
			x_zotc(*xcp++);
	if (type != 1) {		/* for list */
		x_maxlen = 0;
		XPinit(words, 16);
	}
	while (*xp && !iscfs(*xp))
		*cp++ = *xp++;
	*cp = 0;

	len = strlen(buf);
	loc = -1;
	multi = 0;

	for (twalk(&taliases); (tp = tnext()) != NULL; ) {
		int	klen;

		if (!(tp->flag&ISSET))
			continue;
		klen = strlen(tp->name);
		if (klen < len)
			continue;
		if (strncmp(buf, tp->name, len) ==0) {
			if (type)  {	/* for complete */
				if (loc == -1)  {
					(void)strcpy(bug, tp->name);
					loc = klen;
				} else {
					multi = 1;
					loc = strmatch(bug, tp->name);
					bug[loc] = '\0';
				}
			}
			if (type != 1) { /* for list */
				add_stash((char *)0, tp->name);
			}
		}
	}

	if (type)  {			/* for complete */
		if (loc < 0 ||
		    (loc == 0 && type != 2))  {
			x_e_putc(BEL);
			return;
		}
		cp = bug + len;
		x_ins(cp);
		if (!multi)
			x_ins(" ");
		else if (type == 2)	/* complete and list rest */
			list_stash();
	}

	if (type == 0 ||		/* if list */
	    (type == 2 && multi)) {	/* or complete-list and ambiguous */
		list_stash();
	}
}

static int
strmatch(s1, s2)
	register char *s1, *s2;
{
	register char *p;

	for (p = s1; *p == *s2++ && *p != 0; p++)
		;
	return p - s1;
}

/* NAME:
 *      x_adjust - redraw the line adjusting starting point etc.
 *
 * DESCRIPTION:
 *      This function is called when we have exceeded the bounds 
 *      of the edit window.  It increments x_adj_done so that 
 *      functions like x_ins and x_delete know that we have been 
 *      called and can skip the x_bs() stuff which has already 
 *      been done by x_redraw.
 *
 * RETURN VALUE:
 *      None
 */

static void
x_adjust()
{
  x_adj_done++;			/* flag the fact that we were called. */
#ifdef EMACS
  /*
   * we had a promblem if the prompt length > x_cols / 2
   */
  if ((xbp = xcp - (x_displen / 2)) < xbuf)
    xbp = xbuf;
  xlp_valid = FALSE;
  x_redraw(x_cols);
#endif
  x_flush();
}

static void
x_e_putc(c)
	int c;
{
  if (c == '\r' || c == '\n')
    x_col = 0;
  if (x_col < x_cols)
  {
    x_putc(c);
    switch(c)
    {
    case BEL:
      break;
    case '\r':
    case '\n':
    break;
    case '\b':
      x_col--;
      break;
    default:
      x_col++;
      break;
    }
  }
  if (x_adj_ok && (x_col < 0 || x_col >= (x_cols - 2)))
  {
    x_adjust();
  }
}

#ifdef DEBUG
static int
x_debug_info()
{
  x_flush();
  printf("\nksh debug:\n");
  printf("\tx_col == %d,\t\tx_cols == %d,\tx_displen == %d\n",
	 x_col, x_cols, x_displen);
  printf("\txcp == 0x%lx,\txep == 0x%lx\n", (long) xcp, (long) xep);
  printf("\txbp == 0x%lx,\txbuf == 0x%lx\n", (long) xbp, (long) xbuf);
  printf("\txlp == 0x%lx\n", (long) xlp);
  printf("\txlp == 0x%lx\n", (long) x_lastcp());
  printf("\n");
  x_redraw(-1);
  return 0;
}
#endif

static void
x_e_puts(s)
	register char *s;
{
  register int	adj = x_adj_done;

  while (*s && adj == x_adj_done)
    x_e_putc(*s++);
}

/* NAME:
 *      x_set_arg - set an arg value for next function
 *
 * DESCRIPTION:
 *      This is a simple implementation of M-[0-9].
 *
 * RETURN VALUE:
 *      KSTD
 */

static int
x_set_arg(c)
  int c;
{
  if ((x_arg = (c &= CMASK) - '0') < 0 || x_arg > 9)
  {
    x_arg = 1;
    x_e_putc(BEL);
  }
  return KSTD;
}


/* NAME:
 *      x_prev_histword - recover word from prev command
 *
 * DESCRIPTION:
 *      This function recovers the last word from the previous 
 *      command and inserts it into the current edit line.  If a 
 *      numeric arg is supplied then the n'th word from the 
 *      start of the previous command is used.  
 *      
 *      Bound to M-.
 *
 * RETURN VALUE:
 *      KSTD
 */

static int
x_prev_histword()
{
  register char *rcp;
  char *cp;
  char **hp;

  hp = x_histp-1;
  if (hp < history || hp > histptr)
  {
    x_e_putc(BEL);
    return KSTD;
  }
  cp = *hp;
  if (x_last_command != x_set_arg)
  {
    rcp = &cp[strlen(cp) - 1];
    /*
     * ignore white-space after the last word
     */
    while (rcp > cp && iscfs(*rcp))
      rcp--;
    while (rcp > cp && !iscfs(*rcp))
      rcp--;
    if (iscfs(*rcp))
      rcp++;
    x_ins(rcp);
  }
  else
  {
    int c;
    
    rcp = cp;
    /*
     * ignore white-space at start of line
     */
    while (*rcp && iscfs(*rcp))
      rcp++;
    while (x_arg-- > 1)
    {
      while (*rcp && !iscfs(*rcp))
	rcp++;
      while (*rcp && iscfs(*rcp))
	rcp++;
    }
    cp = rcp;
    while (*rcp && !iscfs(*rcp))
      rcp++;
    c = *rcp;
    *rcp = '\0';
    x_ins(cp);
    *rcp = c;
  }
  return KSTD;
}

/* NAME:
 *      x_fold_case - convert word to UPPER/lower case
 *
 * DESCRIPTION:
 *      This function is used to implement M-u,M-l and M-c
 *      to upper case, lower case or Capitalize words.
 *
 * RETURN VALUE:
 *      None
 */

static int
x_fold_case(c)
  int c;
{
  register char	*cp = xcp;
  
  if (cp == xep)
  {
    x_e_putc(BEL);
    return 0;
  }
  c &= 0137;				/* strip prefixes and case */
  if (x_last_command != x_set_arg)
    x_arg = 1;
  while (x_arg--)
  {
    /*
     * fisrt skip over any white-space
     */
    while (cp != xep && ismfs(*cp))
    {
      cp++;
    }
    /*
     * do the first char on its own since it may be
     * a different action than for the rest.
     */
    if (cp != xep)
    {
      if (c == 'L')			/* M-l */
      {
	if (isupper(*cp))
	  *cp = tolower(*cp);
      }
      else				/* M-u or M-c */
      {
	if (islower(*cp))
	  *cp = toupper(*cp);
      }
      cp++;
    }
    /*
     * now for the rest of the word
     */
    while (cp != xep && !ismfs(*cp))
    {
      if (c == 'U')			/* M-u */
      {
	if (islower(*cp))
	  *cp = toupper(*cp);
      }
      else				/* M-l or M-c */
      {
	if (isupper(*cp))
	  *cp = tolower(*cp);
      }
      cp++;
    }
  }
  x_goto(cp);
  return 0;
}

/* NAME:
 *      x_lastcp - last visible char
 *
 * SYNOPSIS:
 *      x_lastcp()
 *
 * DESCRIPTION:
 *      This function returns a pointer to that  char in the 
 *      edit buffer that will be the last displayed on the 
 *      screen.  The sequence:
 *      
 *      for (cp = x_lastcp(); cp > xcp; cp)
 *        x_bs(*--cp);
 *      
 *      Will position the cursor correctly on the screen.
 *
 * RETURN VALUE:
 *      cp or NULL
 */

static char *
x_lastcp()
{
  register char *rcp;
  register int i;

  if (!xlp_valid)
  {
    for (i = 0, rcp = xbp; rcp < xep && i < x_displen; rcp++)
      i += x_size(*rcp);
    xlp = rcp;
  }
  xlp_valid = TRUE;
  return (xlp);
}

#endif /* EDIT */

