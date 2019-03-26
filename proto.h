/*
 * prototypes for PD-KSH
 * originally generated using "cproto.c 3.5 92/04/11 19:28:01 cthuang "
 * $Id: proto.h,v 1.3 1994/05/19 18:32:40 michael Exp michael $
 */

/* alloc.c */
Area *	ainit		ARGS((Area *ap));
void 	afreeall	ARGS((Area *ap));
void *	alloc		ARGS((size_t size, Area *ap));
void *	aresize		ARGS((void *ptr, size_t size, Area *ap));
void 	afree		ARGS((void *ptr, Area *ap));
/* c_ksh.c */
int 	c_hash		ARGS((char **wp));
int 	c_cd		ARGS((char **wp));
int 	c_print		ARGS((char **wp));
int 	c_whence	ARGS((char **wp));
int 	c_typeset	ARGS((char **wp));
int 	c_alias		ARGS((char **wp));
int 	c_unalias	ARGS((char **wp));
int 	c_let		ARGS((char **wp));
int 	c_jobs		ARGS((char **wp));
int 	c_fgbg		ARGS((char **wp));
int 	c_kill		ARGS((char **wp));
void	getopts_reset	ARGS((int val));
int	c_getopts	ARGS((char **wp));
int 	c_bind		ARGS((char **wp));
/* c_sh.c */
int 	c_label		ARGS((char **wp));
int 	c_shift		ARGS((char **wp));
int 	c_umask		ARGS((char **wp));
int 	c_dot		ARGS((char **wp));
int 	c_wait		ARGS((char **wp));
int 	c_read		ARGS((char **wp));
int 	c_eval		ARGS((char **wp));
int 	c_trap		ARGS((char **wp));
int 	c_brkcont	ARGS((char **wp));
int 	c_exitreturn	ARGS((char **wp));
int 	c_set		ARGS((char **wp));
int 	c_unset		ARGS((char **wp));
int 	c_ulimit	ARGS((char **wp));
int 	c_times		ARGS((char **wp));
int 	timex		ARGS((struct op *t, int f));
int 	c_exec		ARGS((char **wp));
int 	c_builtin	ARGS((char **wp));
/* c_test.c */
int 	c_test		ARGS((char **wp));
int	is_db_unop	ARGS((char *s));
int	is_db_binop	ARGS((char *s));
int	is_db_patop	ARGS((int op));
/* edit.c */
void 	x_init		ARGS((void));
int 	x_read		ARGS((char *buf, size_t len));
int 	x_getc		ARGS((void));
void 	x_flush		ARGS((void));
void 	x_putc		ARGS((int c));
void 	x_puts		ARGS((char *s));
bool_t 	x_mode		ARGS((bool_t onoff));
int 	promptlen	ARGS((char *cp));
void	set_editmode	ARGS((char *ed));
/* emacs.c: most prototypes in edit.h */
void 	x_bind		ARGS((char *a1, char *a2, int macro));
/* eval.c */
char *	substitute	ARGS((const char *cp, int f));
char **	eval		ARGS((char **ap, int f));
char *	evalstr		ARGS((char *cp, int f));
char *	evalonestr	ARGS((char *cp, int f));
/* XPtrV not defined at this point... */
void	expand		ARGS((/*char *cp, XPtrV *wp, int f*/));
/* exec.c */
int	fd_clexec	ARGS((int fd));
int 	execute		ARGS((struct op * volatile t, volatile int flags));
int 	shcomexec	ARGS((char **wp));
struct tbl * findfunc	ARGS((char *name, int h, int create));
int 	define		ARGS((char *name, struct op *t));
void 	builtin		ARGS((char *name, int (*func)()));
struct tbl *	findcom	ARGS((char *name, int insert, int autoload,
			      int justsearch));
void 	flushcom	ARGS((int all));
char *	search		ARGS((char *name, char *path, int mode));
/* expr.c */
long 	evaluate	ARGS((const char *expr));
void	v_evaluate	ARGS((struct tbl *vp, const char *expr));
/* history.c */
int 	c_fc	 	ARGS((register char **wp));
void 	histbackup	ARGS((void));
void	sethistsize	ARGS((int n));
void	sethistfile	ARGS((char *name));
void	init_histvec	ARGS((void));
#ifdef EASY_HISTORY
void 	histsave	ARGS((char *cmd));
void 	histappend	ARGS((char *cmd, int nl_seperate));
#else
void	histsave	ARGS((int lno, char *cmd, int dowrite));
#endif
char **	histget	 	ARGS((char *str));
void 	hist_init	ARGS((Source *s));
void 	hist_finish	ARGS((void));
char **	histpos	 	ARGS((void));
int 	histN	 	ARGS((void));
int 	histnum	 	ARGS((int n));
int	findhist	ARGS((int start, int fwd, char *str));
/* io.c */
void 	errorf		ARGS((const char *fmt, ...)) GCC_FA_NORETURN;
void 	shellf		ARGS((const char *fmt, ...));
void 	shprintf	ARGS((const char *fmt, ...));
int	can_seek	ARGS((int fd));
void	initio		ARGS((void));
int 	savefd		ARGS((int fd));
void 	restfd		ARGS((int fd, int ofd));
void 	openpipe	ARGS((int *pv));
void 	closepipe	ARGS((int *pv));
struct temp *maketemp	ARGS((Area *ap));
/* jobs.c */
void 	j_init		ARGS((int mflagset));
void 	j_exit		ARGS((void));
void 	j_change	ARGS((void));
int 	exchild		ARGS((struct op *t, int flags, int close_fd));
void 	startlast	ARGS((void));
int 	waitlast	ARGS((void));
int 	waitfor		ARGS((char *cp));
int 	j_kill		ARGS((char *cp, int sig));
int 	j_resume	ARGS((char *cp, int bg));
void 	j_jobs		ARGS((char *cp, int slp, int nflag));
void 	j_notify	ARGS((void));
pid_t	j_async		ARGS((void));
int 	j_stopped	ARGS((void));
/* lex.c */
int 	yylex		ARGS((int cf));
void 	yyerror		ARGS((const char *fmt, ...)) GCC_FA_NORETURN;
Source * pushs		ARGS((int type));
void	set_prompt	ARGS((int to));
void 	pprompt		ARGS((char *cp));
/* mail.c */
void 	mcheck		ARGS((void));
void 	mbset		ARGS((char *p));
void 	mpset		ARGS((char *mptoparse));
void 	mprint		ARGS((void));
/* main.c */
int 	main		ARGS((int argc, char **argv, char **envp));
int 	include		ARGS((char *name, int argc, char **argv));
int 	command		ARGS((char *comm));
int 	shell		ARGS((Source *volatile s, int volatile exit_atend));
void 	unwind		ARGS((int i)) GCC_FA_NORETURN;
void 	newenv		ARGS((int type));
void 	quitenv		ARGS((void));
void 	aerror		ARGS((Area *ap, const char *msg)) GCC_FA_NORETURN;
/* misc.c */
void 	setctypes	ARGS((const char *s, int t));
void 	initctypes	ARGS((void));
char *	ulton		ARGS((unsigned long n, int base));
char *	strsave		ARGS((const char *s, Area *ap));
char *	strnsave	ARGS((const char *s, int n, Area *ap));
int	option		ARGS((const char *n));
char *	getoptions	ARGS((void));
void	change_flag	ARGS((enum sh_flag f, int what, int newval));
int	parse_args	ARGS((char **argv, int what, char **cargp,
				int *setargsp));
int 	getn		ARGS((char *as));
int 	getn_		ARGS((char *as, char *who));
char *	strerror	ARGS((int i));
int 	gmatch		ARGS((char *s, char *p));
void 	qsortp		ARGS((void **base, size_t n, int (*f)()));
int 	xstrcmp		ARGS((void *p1, void *p2));
void	ksh_getopt_reset ARGS((Getopt *go, int));
int	ksh_getopt	ARGS((char **argv, Getopt *go, char *options));
void	print_value_quoted ARGS((char *s));
/* path.c */
int	make_path	ARGS((char *curpath, char *file, char **pathlist,
				char *result, int len));
void	simplify_path	ARGS((char *path));
/* syn.c */
void 	initkeywords	ARGS((void));
struct op * compile	ARGS((Source *s));
/* table.c */
unsigned int 	hash	ARGS((char *n));
void 	tinit		ARGS((struct table *tp, Area *ap));
struct tbl *	tsearch	ARGS((struct table *tp, char *n, unsigned int h));
struct tbl *	tenter	ARGS((struct table *tp, char *n, unsigned int h));
void 	tdelete		ARGS((struct tbl *p));
void 	twalk		ARGS((struct table *tp));
struct tbl *	tnext	ARGS((void));
struct tbl **	tsort	ARGS((struct table *tp));
/* trace.c */
/* trap.c */
void	inittraps	ARGS((void));
void	alarm_init	ARGS((void));
Trap *	gettrap		ARGS((char *name));
RETSIGTYPE trapsig	ARGS((int i));
void	intrcheck	ARGS((void));
void 	runtraps	ARGS((int intr));
void 	runtrap		ARGS((Trap *p));
void 	cleartraps	ARGS((void));
void 	restoresigs	ARGS((void));
void	settrap		ARGS((Trap *p, char *s));
int	setsig		ARGS((Trap *p, RETSIGTYPE (*f)(), int flags));
void	setexecsig	ARGS((Trap *p, int restore));
/* tree.c */
void 	ptree		ARGS((struct op *t, struct shf *f));
void 	pioact		ARGS((struct shf *f, struct ioword *iop));
int 	fptreef		ARGS((struct shf *f, char *fmt, ...));
char *	snptreef	ARGS((char *s, int n, char *fmt, ...));
struct op *	tcopy	ARGS((struct op *t, Area *ap));
char *	wdcopy		ARGS((const char *wp, Area *ap));
const char *wdscan	ARGS((const char *wp, int c));
void 	tfree		ARGS((struct op *t, Area *ap));
/* var.c */
void 	newblock	ARGS((void));
void 	popblock	ARGS((void));
struct tbl *	global	ARGS((char *n));
struct tbl *	local	ARGS((char *n));
char *	strval		ARGS((struct tbl *vp));
long 	intval		ARGS((struct tbl *vp));
void 	setstr		ARGS((struct tbl *vq, char *s));
struct tbl *	strint	ARGS((struct tbl *vq, struct tbl *vp));
void 	setint		ARGS((struct tbl *vq, long n));
int	getint		ARGS((struct tbl *vp, long *nump));
int 	import		ARGS((char *thing));
struct tbl *	typeset	ARGS((char *var, int set, int clr, int field, int base));
void 	unset		ARGS((struct tbl *vp));
char 	*skip_varname	ARGS((char *s, int aok));
char	*skip_wdvarname ARGS((char *s, int aok));
int	is_wdvarname	ARGS((char *s, int aok));
int	is_wdvarassign	ARGS((char *s));
char **	makenv		ARGS((void));
int	array_ref_len	ARGS((const char *cp));
char *  basename	ARGS((char *str));
void    set_array	ARGS((char *var, int reset, char **vals));
/* version.c */
/* vi.c: prototypes in edit.h */
