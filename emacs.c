/*
 *  Emacs-like command line editing and history
 *
 *  created by Ron Natalie at BRL
 *  modified by Doug Kingston, Doug Gwyn, and Lou Salkind
 *  adapted to PD ksh by Eric Gisin
 */

#include "config.h"
#ifdef EMACS

#include "sh.h"
#include "ksh_stat.h"
#include "ksh_dir.h"
#include <ctype.h>
#include "edit.h"

static	Area	aedit;
#define	AEDIT	&aedit		/* area for kill ring and macro defns */

#undef CTRL			/* _BSD brain damage */
#define	CTRL(x)		((x) == '?' ? 0x7F : (x) & 0x1F)	/* ASCII */
#define	UNCTRL(x)	((x) == 0x7F ? '?' : (x) | 0x40)	/* ASCII */


/* values returned by keyboard functions */
#define	KSTD	0
#define	KEOL	1		/* ^M, ^J */
#define	KINTR	2		/* ^G, ^C */

struct	x_ftab  {
	int		(*xf_func)();
	char		*xf_name;
	short		xf_flags;
};

/* index into struct x_ftab x_ftab[] - small is good */
typedef unsigned char Findex;

struct x_defbindings {
	Findex		xdb_func;	/* XFUNC_* */
	char		xdb_tab;
	unsigned char	xdb_char;
};

#define XF_ARG		1	/* command takes number prefix */
#define	XF_NOBIND	2	/* not allowed to bind to function */
#define	XF_PREFIX	4	/* function sets prefix */

#define	iscfs(c)	(c == ' ' || c == '\t')	/* Separator for completion */
#define	ismfs(c)	(!(isalnum(c) || c == '_' || c == '$'))  /* Separator for motion */

#ifdef OS2
  /* Deal with 8 bit chars & an extra prefix for function key (these two
   * changes increase memory usage from 9,216 bytes to 24,416 bytes...)
   */
# define CHARMASK	0xFF		/* 8-bit ASCII character mask */
# define X_TABSZ	256		/* size of keydef tables etc */
# define X_NTABS	4		/* normal, meta1, meta2, meta3 */
static int	x_prefix3 = 0xE0;
#else /* OS2 */
# define CHARMASK	0x7F		/* 7-bit ASCII character mask */
# define X_TABSZ	128		/* size of keydef tables etc */
# define X_NTABS	3		/* normal, meta1, meta2 */
#endif /* OS2 */

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
static int	x_arg_defaulted;/* x_arg not explicitly set; defaulted to 1 */

static int	xlp_valid;
/* end from 4.9 edit.h } */

static	int	x_prefix1 = CTRL('['), x_prefix2 = CTRL('X');
static	char   **x_histp;	/* history position */
static	char   **x_nextcmdp;	/* for newline-and-next */
static	char	*xmp;		/* mark pointer */
static	Findex   x_last_command;
static	Findex (*x_tab)[X_TABSZ] = NULL; /* key definition */
static	char    *(*x_atab)[X_TABSZ] = NULL; /* macro definitions */
#define	KILLSIZE	20
static	char    *killstack[KILLSIZE];
static	int	killsp, killtp;
static	int	x_curprefix;
static	char    *macroptr;
static	int	x_maxlen;	/* to determine column width */

static void     x_ins       ARGS((char *cp));
static void     x_delete    ARGS((int nc, int force_push));
static int	x_bword     ARGS((void));
static int	x_fword     ARGS((void));
static void     x_goto      ARGS((char *cp));
static void     x_bs        ARGS((int c));
static int      x_size_str  ARGS((char *cp));
static int      x_size      ARGS((int c));
static void     x_zots      ARGS((char *str));
static void     x_zotc      ARGS((int c));
static void     x_load_hist ARGS((char **hp));
static int      x_search    ARGS((char *pat, int sameline, int offset));
static int      x_match     ARGS((char *str, char *pat));
static void	x_redraw    ARGS((int limit));
static void     x_push      ARGS((int nchars));
static void     x_mapin     ARGS((char *cp));
static char *   x_mapout    ARGS((int c));
static void     x_print     ARGS((int prefix, int key));
static void     add_stash   ARGS((char *dirnam, char *name));
static void     list_stash  ARGS((void));
static void     compl_dec   ARGS((int type));
static void     compl_file  ARGS((int type));
static void     compl_command ARGS((int type));
static int      strmatch    ARGS((char *s1, char *s2));
static void	x_adjust    ARGS((void));
static void	x_e_ungetc  ARGS((int c));
static int	x_e_getc    ARGS((void));
static void	x_e_putc    ARGS((int c));
#ifdef DEBUG
static int	x_debug_info ARGS((void));
#endif /* DEBUG */
static void	x_e_puts    ARGS((char *s));
static int	x_fold_case ARGS((int c));
static char	*x_lastcp ARGS(());


/* The lines between START-FUNC-TAB .. END-FUNC-TAB are run through a
 * script (emacs-gen.sh) that generates emacs.out which contains:
 *	- function declarations for x_* functions
 *	- defines of the form XFUNC_<name> where <name> is function
 *	  name, sans leading x_.
 * Note that the script treats #ifdef and { 0, 0, 0} specially - use with
 * caution.
 */
#include "emacs.out"
static const struct x_ftab x_ftab[] = {
/* @START-FUNC-TAB@ */
	{ x_abort,		"abort",			0 },
	{ x_beg_hist,		"beginning-of-history",		0 },
	{ x_comp_comm,		"complete-command",		0 },
	{ x_comp_file,		"complete-file",		0 },
	{ x_complete,		"complete",			0 },
	{ x_del_back,		"delete-char-backward",		XF_ARG },
	{ x_del_bword,		"delete-word-backward",		XF_ARG },
	{ x_del_char,		"delete-char-forward",		XF_ARG },
	{ x_del_fword,		"delete-word-forward",		XF_ARG },
	{ x_del_line,		"kill-line",			0 },
	{ x_draw_line,		"redraw",			0 },
	{ x_end_hist,		"end-of-history",		0 },
	{ x_end_of_text,	"eot",				0 },
	{ x_enumerate,		"list",				0 },
	{ x_eot_del,		"eot-or-delete",		XF_ARG },
	{ x_error,		"error",			0 },
	{ x_goto_hist,		"goto-history",			XF_ARG },
	{ x_ins_string,		"macro-string",			XF_NOBIND },
	{ x_insert,		"auto-insert",			XF_ARG },
	{ x_kill,		"kill-to-eol",			XF_ARG },
	{ x_kill_region,	"kill-region",			0 },
	{ x_list_comm,		"list-command",			0 },
	{ x_list_file,		"list-file",			0 },
	{ x_literal,		"quote",			0 },
	{ x_meta1,		"prefix-1",			XF_PREFIX },
	{ x_meta2,		"prefix-2",			XF_PREFIX },
	{ x_meta_yank,		"yank-pop",			0 },
	{ x_mv_back,		"backward-char",		XF_ARG },
	{ x_mv_begin,		"beginning-of-line",		0 },
	{ x_mv_bword,		"backward-word",		XF_ARG },
	{ x_mv_end,		"end-of-line",			0 },
	{ x_mv_forw,		"forward-char",			XF_ARG },
	{ x_mv_fword,		"forward-word",			XF_ARG },
	{ x_newline,		"newline",			0 },
	{ x_next_com,		"down-history",			XF_ARG },
	{ x_nl_next_com,	"newline-and-next",		0 },
	{ x_noop,		"no-op",			0 },
	{ x_prev_com,		"up-history",			XF_ARG },
	{ x_prev_histword,	"prev-hist-word",		XF_ARG },
	{ x_search_char_forw,	"search-character-forward",	XF_ARG },
	{ x_search_char_back,	"search-character-backward",	XF_ARG },
	{ x_search_hist,	"search-history",		0 },
	{ x_set_mark,		"set-mark-command",		0 },
	{ x_stuff,		"stuff",			0 },
	{ x_stuffreset,		"stuff-reset",			0 },
	{ x_transpose,		"transpose-chars",		0 },
	{ x_version,		"version",			0 },
	{ x_xchg_point_mark,	"exchange-point-and-mark",	0 },
	{ x_yank,		"yank",				0 },
        { x_comp_list,		"complete-list",		0 },
        { x_fold_capitialize,	"capitalize-word",		XF_ARG },
        { x_fold_lower,		"downcase-word",		XF_ARG },
        { x_fold_upper,		"upcase-word",			XF_ARG },
        { x_set_arg,		"set-arg",			XF_NOBIND },
#ifdef SILLY
	{ x_game_of_life,	"play-game-of-life",		0 },
#else
	{ 0, 0, 0 },
#endif
#ifdef DEBUG
        { x_debug_info,		"debug-info",			0 },
#else
	{ 0, 0, 0 },
#endif
#ifdef OS2
	{ x_meta3,		"prefix-3",			XF_PREFIX },
#else
	{ 0, 0, 0 },
#endif
/* @END-FUNC-TAB@ */
    };

static	struct x_defbindings const x_defbindings[] = {
	{ XFUNC_del_back,		0, CTRL('?') },
	{ XFUNC_del_bword,		1, CTRL('?') },
	{ XFUNC_eot_del,		0, CTRL('D') },
	{ XFUNC_del_back,		0, CTRL('H') },
	{ XFUNC_del_bword,		1, CTRL('H') },
	{ XFUNC_del_bword,		1,      'h'  },
	{ XFUNC_mv_bword,		1,      'b'  },
	{ XFUNC_mv_fword,		1,      'f'  },
	{ XFUNC_del_fword,		1,      'd'  },
	{ XFUNC_mv_back,		0, CTRL('B') },
	{ XFUNC_mv_forw,		0, CTRL('F') },
	{ XFUNC_search_char_forw,	0, CTRL(']') },
	{ XFUNC_search_char_back,	1, CTRL(']') },
	{ XFUNC_newline,		0, CTRL('M') },
	{ XFUNC_newline,		0, CTRL('J') },
	{ XFUNC_end_of_text,		0, CTRL('_') },
	{ XFUNC_abort,			0, CTRL('G') },
	{ XFUNC_prev_com,		0, CTRL('P') },
	{ XFUNC_next_com,		0, CTRL('N') },
	{ XFUNC_nl_next_com,		0, CTRL('O') },
	{ XFUNC_search_hist,		0, CTRL('R') },
	{ XFUNC_beg_hist,		1,      '<'  },
	{ XFUNC_end_hist,		1,      '>'  },
	{ XFUNC_goto_hist,		1,      'g'  },
	{ XFUNC_mv_end,			0, CTRL('E') },
	{ XFUNC_mv_begin,		0, CTRL('A') },
	{ XFUNC_draw_line,		0, CTRL('L') },
	{ XFUNC_meta1,			0, CTRL('[') },
	{ XFUNC_meta2,			0, CTRL('X') },
	{ XFUNC_kill,			0, CTRL('K') },
	{ XFUNC_yank,			0, CTRL('Y') },
	{ XFUNC_meta_yank,		1,      'y'  },
	{ XFUNC_literal,		0, CTRL('^') },
#if defined(BRL) && defined(TIOCSTI)
	{ XFUNC_stuff,			0, CTRL('T') },
#else
	{ XFUNC_transpose,		0, CTRL('T') },
#endif
	{ XFUNC_complete,		1, CTRL('[') },
        { XFUNC_comp_list,		1,	'='  },
	{ XFUNC_enumerate,		1,	'?'  },
	{ XFUNC_comp_file,		1, CTRL('X') },
	{ XFUNC_comp_comm,		2, CTRL('[') },
	{ XFUNC_list_comm,		2,	'?'  },
	{ XFUNC_list_file,		2, CTRL('Y') },
	{ XFUNC_nl_next_com,		0, CTRL('O') },
	{ XFUNC_set_mark,		1,	' '  },
	{ XFUNC_kill_region,		0, CTRL('W') },
	{ XFUNC_xchg_point_mark,	2, CTRL('X') },
	{ XFUNC_version,		0, CTRL('V') },
#ifdef DEBUG
        { XFUNC_debug_info,		1, CTRL('H') },
#endif
	{ XFUNC_prev_histword,		1,	'.'  },
	{ XFUNC_prev_histword,		1,	'_'  },
        { XFUNC_set_arg,		1,	'0'  },
        { XFUNC_set_arg,		1,	'1'  },
        { XFUNC_set_arg,		1,	'2'  },
        { XFUNC_set_arg,		1,	'3'  },
        { XFUNC_set_arg,		1,	'4'  },
        { XFUNC_set_arg,		1,	'5'  },
        { XFUNC_set_arg,		1,	'6'  },
        { XFUNC_set_arg,		1,	'7'  },
        { XFUNC_set_arg,		1,	'8'  },
        { XFUNC_set_arg,		1,	'9'  },
        { XFUNC_fold_upper,		1,	'U'  },
        { XFUNC_fold_upper,		1,	'u'  },
        { XFUNC_fold_lower,		1,	'L'  },
        { XFUNC_fold_lower,		1,	'l'  },
        { XFUNC_fold_capitialize,	1,	'C'  },
        { XFUNC_fold_capitialize,	1,	'c'  },
#ifdef OS2
	{ XFUNC_meta3,			0,	0xE0 },
	{ XFUNC_mv_back,		3,	'K'  },
	{ XFUNC_mv_forw,		3,	'M'  },
	{ XFUNC_next_com,		3,	'P'  },
	{ XFUNC_prev_com,		3,	'H'  },
#else /* OS2 */
	/* These for ansi arrow keys: arguablely shouldn't be here by
	 * default, but its simpler/faster/smaller than using termcap
	 * entries.
	 */
        { XFUNC_meta2,			1,	'['  },
	{ XFUNC_prev_com,		2,	'A'  },
	{ XFUNC_next_com,		2,	'B'  },
	{ XFUNC_mv_forw,		2,	'C'  },
	{ XFUNC_mv_back,		2,	'D'  },
#endif /* OS2 */
};

int
x_emacs(buf, len)
	char *buf;
	size_t len;
{
	int	c;
	int	i;
	Findex	f;

	xbp = xbuf = buf; xend = buf + len;
	xlp = xcp = xep = buf;
	*xcp = 0;
	xlp_valid = TRUE;
	xmp = NULL;
	x_curprefix = 0;
	macroptr = (char *) 0;
	x_histp = histptr + 1;
	x_last_command = XFUNC_error;

	x_col = promptlen(prompt);
	x_adj_ok = 1;
	x_displen = x_cols - 2 - x_col;
	x_adj_done = 0;

	pprompt(prompt, 0);

	if (x_nextcmdp != NULL) {
		x_load_hist(x_nextcmdp);
		x_nextcmdp = NULL;
	}

	while (1) {
		x_flush();
		if ((c = x_e_getc()) < 0)
			return 0;

		f = x_curprefix == -1 ? XFUNC_insert
			: x_tab[x_curprefix][c&CHARMASK]; 

		if (!(x_ftab[f].xf_flags & XF_PREFIX)
		    && x_last_command != XFUNC_set_arg)
		{
			x_arg = 1;
			x_arg_defaulted = 1;
		}
		i = c | (x_curprefix << 8);
		x_curprefix = 0;
		switch (i = (*x_ftab[f].xf_func)(i))  {
		  case KSTD:
			if (!(x_ftab[f].xf_flags & XF_PREFIX))
				x_last_command = f;
			break;
		  case KEOL:
			i = xep - xbuf;
			return i;
		  case KINTR:	/* special case for interrupt */
			trapsig(SIGINT);
			x_mode(FALSE);
			unwind(LSHELL);
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
	while (x_arg--)
		x_ins(str);
	return KSTD;
}

static int
x_ins_string(c)
	int c;
{
	if (macroptr)   {
		x_e_putc(BEL);
		return KSTD;
	}
	macroptr = x_atab[c>>8][c & CHARMASK];
	if (macroptr && !*macroptr) {
		/* XXX bell? */
		macroptr = (char *) 0;
	}
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
	int col = xcp - xbuf;

	if (col == 0)  {
		x_e_putc(BEL);
		return KSTD;
	}
	if (x_arg > col)
		x_arg = col;
	x_goto(xcp - x_arg);
	x_delete(x_arg, FALSE);
	return KSTD;
}

static int
x_del_char(c)
	int c;
{
	int nleft = xep - xcp;

	if (!nleft) {
		x_e_putc(BEL);
		return KSTD;
	}
	if (x_arg > nleft)
		x_arg = nleft;
	x_delete(x_arg, FALSE);
	return KSTD;
}

/* Delete nc chars to the right of the cursor (including cursor position) */
static void
x_delete(nc, force_push)
	int nc;
	int force_push;
{
	int	i,j;
	char	*cp;
	
	if (nc == 0)
		return;
	if (xmp != NULL && xmp > xcp) {
		if (xcp + nc > xmp)
			xmp = xcp;
		else
			xmp -= nc;
	}

	/*
	 * This lets us yank a word we have deleted.
	 */
	if (nc > 1 || force_push)
		x_push(nc);

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
	x_delete(x_bword(), FALSE);
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
	x_delete(x_fword(), FALSE);
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
	while (x_arg--)
	{
	  while (cp != xep && ismfs(*cp))
	  {
	    cp++;
	    nc++;
	  }
	  while (cp != xep && !ismfs(*cp))
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
      if (cp > xcp)		/* move forward */
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
	int col = xcp - xbuf;

	if (col == 0)  {
		x_e_putc(BEL);
		return KSTD;
	}
	if (x_arg > col)
		x_arg = col;
	x_goto(xcp - x_arg);
	return KSTD;
}

static int
x_mv_forw(c)
	int c;
{
	int nleft = xep - xcp;

	if (!nleft) {
		x_e_putc(BEL);
		return KSTD;
	}
	if (x_arg > nleft)
		x_arg = nleft;
	x_goto(xcp + x_arg);
	return KSTD;
}

static int
x_search_char_forw(c)
	int c;
{
	char *cp = xcp;

	*xep = '\0';
	c = x_e_getc();
	while (x_arg--) {
	    if (c < 0
	       || ((cp = (cp == xep) ? NULL : strchr(cp + 1, c)) == NULL
		   && (cp = strchr(xbuf, c)) == NULL))
	    {
		    x_e_putc(BEL);
		    return KSTD;
	    }
	}
	x_goto(cp);
	return KSTD;
}

static int
x_search_char_back(c)
	int c;
{
	char *cp = xcp, *p;

	c = x_e_getc();
	for (; x_arg--; cp = p)
		for (p = cp; ; ) {
			if (p-- == xbuf)
				p = xep;
			if (c < 0 || p == cp) {
				x_e_putc(BEL);
				return KSTD;
			}
			if (*p == c)
				break;
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
	return KEOL;
}

static int x_beg_hist(c) int c; { x_load_hist(history); return KSTD;}

static int x_end_hist(c) int c; { x_load_hist(histptr); return KSTD;}

static int x_prev_com(c) int c; { x_load_hist(x_histp - x_arg); return KSTD;}

static int x_next_com(c) int c; { x_load_hist(x_histp + x_arg); return KSTD;}
  
/* Goto a particular history number obtained from argument.
 * If no argument is given history 1 is probably not what you
 * want so we'll simply go to the oldest one.
 */
static int
x_goto_hist(c)
	int c;
{
	if (x_arg_defaulted)
		x_load_hist(history);
	else
		x_load_hist(histptr + x_arg - source->line);
	return KSTD;
}

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
	if (xep == xbuf && x_arg_defaulted)
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
	Findex f;

	*p = '\0';
	while (1) {
		if (offset < 0) {
			x_e_puts("\nI-search: ");
			x_e_puts(pat);
		}
		x_flush();
		if ((c = x_e_getc()) < 0)
			return KSTD;
		f = x_tab[0][c&CHARMASK];
		if (c == CTRL('['))
			break;
		else if (f == XFUNC_search_hist)
			offset = x_search(pat, 0, offset);
		else if (f == XFUNC_del_back) {
			if (p == pat) {
				offset = -1;
				break;
			}
			if (p > pat)
				*--p = '\0';
			if (p == pat)
				offset = -1;
			else
				offset = x_search(pat, 1, offset);
			continue;
		} else if (f == XFUNC_insert) {
			/* add char to pattern */
			/* overflow check... */
			if (p >= &pat[sizeof(pat) - 1]) {
				x_e_putc(BEL);
				continue;
			}
			*p++ = c, *p = '\0';
			if (offset >= 0) {
				/* already have partial match */
				offset = x_match(xbuf, pat);
				if (offset >= 0) {
					x_goto(xbuf + offset + (p - pat) - (*pat == '^'));
					continue;
				}
			}
			offset = x_search(pat, 0, offset);
		} else { /* other command */
			x_e_ungetc(c);
			break;
		}
	}
	if (offset < 0)
		x_redraw(-1);
	return KSTD;
}

/* search backward from current line */
static int
x_search(pat, sameline, offset)
	char *pat;
	int sameline;
	int offset;
{
	register char **hp;
	int i;

	for (hp = x_histp - (sameline ? 0 : 1) ; hp >= history; --hp) {
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
	  pprompt(prompt, 0);
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

	/* What transpose is meant to do seems to be up for debate. This
	 * is a general summary of the options; the text is abcd with the
	 * upper case character or underscore indicating the cursor positiion:
	 *     Who			Before	After  Before	After
	 *     at&t ksh in emacs mode:	abCd	abdC   abcd_	(bell)
	 *     at&t ksh in gmacs mode:	abCd	baCd   abcd_	abdc_
	 *     gnu emacs:		abCd	acbD   abcd_	abdc_
	 * Pdksh currently goes with GNU behavior since I believe this is the
	 * most common version of emacs, unless in gmacs mode, in which case
	 * it does the at&t ksh gmacs mdoe.
	 * This should really be broken up into 3 functions so users can bind
	 * to the one they want.
	 */
	if (xcp == xbuf) {
		x_e_putc(BEL);
		return KSTD;
	} else if (xcp == xep || Flag(FGMACS)) {
		if (xcp - xbuf == 1) {
			x_e_putc(BEL);
			return KSTD;
		}
		/* Gosling/Unipress emacs style: Swap two characters before the
		 * cursor, do not change cursor position
		 */
		x_bs(xcp[-1]);
		x_bs(xcp[-2]);
		x_zotc(xcp[-1]);
		x_zotc(xcp[-2]);
		tmp = xcp[-1];
		xcp[-1] = xcp[-2];
		xcp[-2] = tmp;
	} else {
		/* GNU emacs style: Swap the characters before and under the
		 * cursor, move cursor position along one.
		 */
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
	return KSTD;
}

static int
x_meta2(c)
	int c;
{
	x_curprefix = 2;
	return KSTD;
}

#ifdef OS2
static int
x_meta3(c)
	int c;
{
	x_curprefix = 3;
	return KSTD;
}
#endif /* OS2 */

static int
x_kill(c)
	int c;
{
	int col = xcp - xbuf;
	int lastcol = xep - xbuf;
	int ndel;

	if (x_arg_defaulted)
		x_arg = lastcol;
	else if (x_arg > lastcol)
		x_arg = lastcol;
	ndel = x_arg - col;
	if (ndel < 0) {
		x_goto(xbuf + x_arg);
		ndel = -ndel;
	}
	x_delete(ndel, TRUE);
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
	if (x_last_command != XFUNC_yank && x_last_command != XFUNC_meta_yank) {
		x_e_puts("\nyank something first");
		x_redraw(-1);
		return KSTD;
	}
	len = strlen(killstack[killtp]);
	x_goto(xcp - len);
	x_delete(len, FALSE);
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
#ifdef OS2
			if (*cp == '0')	/* To define function keys */
				*op++ = 0xE0;
			else
#endif /* OS2 */
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
	*op = '\0';
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
#ifdef OS2
	} else if (c == 0xE0) {
		*p++ = '^';
		*p++ = '0';
#endif /* OS2 */
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
#ifdef OS2
	if (prefix == 3)
		shprintf("%s", x_mapout(x_prefix3));
#endif /* OS2 */
	shprintf("%s = ", x_mapout(key));
	if (x_tab[prefix][key] != XFUNC_ins_string)
		shprintf("%s\n", x_ftab[x_tab[prefix][key]].xf_name);
	else
		shprintf("'%s'\n", x_atab[prefix][key]);
}

int
x_bind(a1, a2, macro, list)
	char *a1, *a2;
	int macro;		/* bind -m */
	int list;		/* bind -l */
{
	Findex f;
	int prefix, key;
	char *sp = NULL;

	if (x_tab == NULL) {
		bi_errorf("cannot bind, not a tty");
		return 1;
	}

	/* List function names */
	if (list) {
		for (f = 0; f < NELEM(x_ftab); f++)
			if (x_ftab[f].xf_name
			    && !(x_ftab[f].xf_flags & XF_NOBIND))
				shprintf("%s\n", x_ftab[f].xf_name);
		return 0;
	}

	if (a1 == NULL) {
		for (prefix = 0; prefix < X_NTABS; prefix++)
			for (key = 0; key < X_TABSZ; key++) {
				f = x_tab[prefix][key];
				if (f == XFUNC_insert || f == XFUNC_error
				    || (macro && f != XFUNC_ins_string))
					continue;
				x_print(prefix, key);
			}
		return 0;
	}

	x_mapin(a1);
	prefix = key = 0;
	for (;; a1++) {
		key = *a1 & CHARMASK;
		if (x_tab[prefix][key] == XFUNC_meta1)
			prefix = 1;
		else if (x_tab[prefix][key] == XFUNC_meta2)
			prefix = 2;
#ifdef OS2
		else if (x_tab[prefix][key] == XFUNC_meta3)
			prefix = 3;
#endif /* OS2 */
		else
			break;
	}

	if (a2 == NULL) {
		x_print(prefix, key);
		return 0;
	}

	if (*a2 == 0)
		f = XFUNC_insert;
	else if (!macro) {
		for (f = 0; f < NELEM(x_ftab); f++)
			if (x_ftab[f].xf_name
			    && strcmp(x_ftab[f].xf_name, a2) == 0)
				break;
		if (f == NELEM(x_ftab) || x_ftab[f].xf_flags & XF_NOBIND) {
			bi_errorf("%s: no such function", a2);
			return 1;
		}
#if 0		/* This breaks the bind commands that map arrow keys */
		if (f == XFUNC_meta1)
			x_prefix1 = key;
		if (f == XFUNC_meta2)
			x_prefix2 = key;
#endif /* 0 */
	} else {
		f = XFUNC_ins_string;
		x_mapin(a2);
		sp = strsave(a2, AEDIT);
	}

	if (x_tab[prefix][key] == XFUNC_ins_string && x_atab[prefix][key])
		afree((void *)x_atab[prefix][key], AEDIT);
	x_tab[prefix][key] = f;
	x_atab[prefix][key] = sp;

	return 0;
}

void
x_init_emacs()
{
	register int i, j;

	ainit(AEDIT);

	x_tab = (Findex (*)[X_TABSZ]) alloc(sizeofN(*x_tab, X_NTABS), AEDIT);
	for (j = 0; j < X_TABSZ; j++)
		x_tab[0][j] = XFUNC_insert;
	for (i = 1; i < X_NTABS; i++)
		for (j = 0; j < X_TABSZ; j++)
			x_tab[i][j] = XFUNC_error;
	for (i = 0; i < NELEM(x_defbindings); i++)
		x_tab[x_defbindings[i].xdb_tab][x_defbindings[i].xdb_char]
			= x_defbindings[i].xdb_func;

	x_atab = (char *(*)[X_TABSZ]) alloc(sizeofN(*x_atab, X_NTABS), AEDIT);
	for (i = 1; i < X_NTABS; i++)
		for (j = 0; j < X_TABSZ; j++)
			x_atab[i][j] = NULL;
}

void
x_emacs_keys(ec)
	X_chars *ec;
{
	x_tab[0][ec->erase] = XFUNC_del_back;
	x_tab[0][ec->kill] = XFUNC_del_line;
	x_tab[0][ec->werase] = XFUNC_del_bword;
	x_tab[0][ec->intr] = XFUNC_abort;
	x_tab[0][ec->quit] = XFUNC_noop;
	x_tab[1][ec->erase] = XFUNC_del_bword;
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
	x_delete(rsize, TRUE);
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

static int
x_version(c)
	int c;
{
	char *o_xbuf = xbuf, *o_xend = xend;
	char *o_xbp = xbp, *o_xep = xep, *o_xcp = xcp;
	int lim = x_lastcp() - xbp;

	xbuf = xbp = xcp = ksh_version + 4;
	xend = xep = ksh_version + 4 + strlen(ksh_version + 4);
	x_redraw(lim);
	x_flush();

	c = x_e_getc();
	xbuf = o_xbuf;
	xend = o_xend;
	xbp = o_xbp;
	xep = o_xep;
	xcp = o_xcp;
	x_redraw(strlen(ksh_version));

	if (c < 0)
		return KSTD;
	/* This is what at&t ksh seems to do...  Very bizarre */
	if (c != ' ')
		x_e_ungetc(c);

	return KSTD;
}

static int
x_noop(c)
	int c;
{
	return KSTD;
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
		else if (strcmp(dirnam, slash) == 0)
			(void)strcpy(buf, slash);
		else
			(void)strcat(strcpy(buf, dirnam), slash);
		(void)strcat(buf, name);
		if (stat(buf, &statb)==0)
			if (S_ISDIR(statb.st_mode))
				type = DIRSEP;
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

/* table for type normal binding
 * 0 = enumerate  M-= complete as much as possible and then list
 * 1 = complete   M-Esc
 * 2 = list       M-?
 */
static void
compl_dec(type)
	int type;
{
	char	*cp = xcp; 
	int	have_word;

	/* find start of word */
	while (cp != xbuf && !iscfs(cp[-1]))
		--cp;
	     
	have_word = cp < xep && !iscfs(*cp);

	/* skip any space before word */
	while (cp != xbuf && iscfs(cp[-1]))
		--cp;

	/* XXX strchr is not good here - will go past the first word */
	if (have_word && cp == xbuf && strchr_dirsep(cp) == NULL)
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
	char	*buf;
	char	*bug;
	int	buglen;
	DIR    *dirp;
	struct dirent *dp;
	long	loc = -1;
	int	len;
	int	multi = 0;

	/* type == 0 for list, 1 for complete and 2 for complete-list */
	str = xcp;
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
	cp = xp;
	while (*xp && !iscfs(*xp))
		xp++;
	cp = strnsave(cp, xp - cp, ATEMP);

	buf = substitute(cp, DOTILDE);
	afree((void*)cp, ATEMP);
	lastp = strrchr_dirsep(buf);
	if (lastp)
		*lastp = '\0';

	dirnam = (lastp == NULL) ? "." : (lastp == buf) ? slash : buf;

#ifdef OS2
	if (dirnam[0] && dirnam[1] == ':' && dirnam[2] == '\0' ) {
	    char buf[4];
	    strcpy(buf, dirnam);
	    strcat(buf, DIRSEPSTR);
	    dirp = ksh_opendir(dirnam);
	} else
#endif /* OS2 */
		dirp = ksh_opendir(dirnam);

	if (dirp == NULL) {
		x_e_putc(BEL);
		afree((void *) buf, ATEMP);
		return;
	}

	if (lastp == NULL)
		lastp = buf;
	else
		lastp++;
	len = strlen(lastp);

	bug = alloc(buglen = 64, ATEMP);
	while ((dp = readdir(dirp)) != NULL)  {
		cp = dp->d_name;
		if (cp[0] == '.' &&
		    (cp[1] == '\0' || (cp[1] == '.' && cp[2] == '\0')))
			continue;	/* always ignore . and .. */
		if (FILENCMP(lastp, cp, len) == 0) {
			if (type) {	/* for complete */
				if (loc == -1)  {
					loc = NLENGTH(dp);
					if (loc >= buglen) {
						while (loc >= buglen)
							buglen *= 2;
						bug = aresize(bug, buglen,
								ATEMP);
					}
					(void)strcpy(bug, cp);
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
			afree((void *) buf, ATEMP);
			afree((void *) bug, ATEMP);
			return;
		}
		x_ins(bug + len);
		if (!multi)  {
			struct stat statb;
			int ret;

			if (lastp == buf)
				ret = stat(bug, &statb);
			else {
				char *tbuf = strnsave(dirnam, strlen(dirnam)
						+ 1 + strlen(bug) + 1, ATEMP);
				if (lastp != buf + 1)
					strcat(tbuf, slash);
				strcat(tbuf, bug);
				ret = stat(tbuf, &statb);
				afree((void *) tbuf, ATEMP);
			}
			if (ret >= 0 && S_ISDIR(statb.st_mode))
				x_ins(slash);
			else
				x_ins(space);
		}
	}
	afree((void *) buf, ATEMP);
	afree((void *) bug, ATEMP);
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
	char	*buf;
	char	*bug;
	int	buglen;
	char	*xp;
	char	*cp;
	int  len;
	int  multi;
	int  loc;

	/* type == 0 for list, 1 for complete and 2 for complete-list */
	str = xcp;
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
	cp = xp;
	while (*xp && !iscfs(*xp))
		xp++;
	buf = strnsave(cp, xp - cp, ATEMP);

	len = strlen(buf);
	loc = -1;
	multi = 0;

	bug = alloc(buglen = 64, ATEMP);
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
					loc = klen;
					if (loc >= buglen) {
						while (loc >= buglen)
							buglen *= 2;
						bug = aresize(bug, buglen,
								ATEMP);
					}
					(void)strcpy(bug, tp->name);
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
	afree((void *) buf, ATEMP);

	if (type)  {			/* for complete */
		if (loc < 0 ||
		    (loc == 0 && type != 2))  {
			x_e_putc(BEL);
			afree((void *) bug, ATEMP);
			return;
		}
		x_ins(bug + len);
		if (!multi)
			x_ins(space);
	}
	afree((void *) bug, ATEMP);

	if (type == 0			/* if list */
	    || (type == 2 && multi))	/* or complete-list and ambiguous */
		list_stash();
}

static int
strmatch(s1, s2)
	register char *s1, *s2;
{
	register char *p;

	for (p = s1; FILECHCONV(*p) == FILECHCONV(*s2) && *p != 0; p++, s2++)
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
  /*
   * we had a problem if the prompt length > x_cols / 2
   */
  if ((xbp = xcp - (x_displen / 2)) < xbuf)
    xbp = xbuf;
  xlp_valid = FALSE;
  x_redraw(x_cols);
  x_flush();
}

static int unget_char = -1;

static void
x_e_ungetc(c)
	int c;
{
	unget_char = c;
}

static int
x_e_getc()
{
	int c;
	
	if (unget_char >= 0) {
		c = unget_char;
		unget_char = -1;
	} else {
		if (macroptr)  {
			c = *macroptr++;
			if (!*macroptr)
				macroptr = (char *) 0;
		} else
			c = x_getc();
	}

	return c <= CHARMASK ? c : (c & CHARMASK);
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
  printf(newline);
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
	int n = 0;
	int first = 1;

	c &= CHARMASK;	/* strip command prefix */
	for (; c >= 0 && isdigit(c); c = x_e_getc(), first = 0)
		n = n * 10 + (c - '0');
	if (c < 0 || first) {
		x_e_putc(BEL);
		x_arg = 1;
		x_arg_defaulted = 1;
	} else {
		x_e_ungetc(c);
		x_arg = n;
		x_arg_defaulted = 0;
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
x_prev_histword(c)
	int c;
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
  if (x_arg_defaulted) {
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
  } else {
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

/* Uppercase N(1) words */
static int
x_fold_upper(c)
  int c;
{
	return x_fold_case('U');
}

/* Lowercase N(1) words */
static int
x_fold_lower(c)
  int c;
{
	return x_fold_case('L');
}

/* Lowercase N(1) words */
static int
x_fold_capitialize(c)
  int c;
{
	return x_fold_case('C');
}

/* NAME:
 *      x_fold_case - convert word to UPPER/lower/Capitial case
 *
 * DESCRIPTION:
 *      This function is used to implement M-U,M-u,M-L,M-l,M-C and M-c
 *      to UPPER case, lower case or Capitalize words.
 *
 * RETURN VALUE:
 *      None
 */

static int
x_fold_case(c)
	int c;
{
	char *cp = xcp;
	
	if (cp == xep) {
		x_e_putc(BEL);
		return KSTD;
	}
	while (x_arg--) {
		/*
		 * fisrt skip over any white-space
		 */
		while (cp != xep && ismfs(*cp))
			cp++;
		/*
		 * do the first char on its own since it may be
		 * a different action than for the rest.
		 */
		if (cp != xep) {
			if (c == 'L') {		/* lowercase */
				if (isupper(*cp))
					*cp = tolower(*cp);
			} else {		/* uppercase, capitialize */
				if (islower(*cp))
					*cp = toupper(*cp);
			}
			cp++;
		}
		/*
		 * now for the rest of the word
		 */
		while (cp != xep && !ismfs(*cp)) {
			if (c == 'U') {		/* uppercase */
				if (islower(*cp))
					*cp = toupper(*cp);
			} else {		/* lowercase, capitialize */
				if (isupper(*cp))
					*cp = tolower(*cp);
			}
			cp++;
		}
	}
	x_goto(cp);
	return KSTD;
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
