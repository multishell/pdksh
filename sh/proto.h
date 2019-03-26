/*
 * prototypes for PD-KSH
 * originally generated using "cproto.c 3.5 92/04/11 19:28:01 cthuang "
 * $Id: proto.h,v 1.1 1992/04/25 08:29:02 sjg Exp $
 */
#ifndef ARGS
#if defined(__STDC__) || defined(__cplusplus)
#define ARGS(s) s
#else
#define ARGS(s) ()
#endif
#endif

/* alloc.c */
Area *	ainit		 ARGS((Area *ap));
void 	afreeall	 ARGS((Area *ap));
void *	alloc		 ARGS((size_t size, Area *ap));
void *	aresize		 ARGS((void *ptr, size_t size, Area *ap));
void 	afree		 ARGS((void *ptr, Area *ap));
/* c_ksh.c */
int 	c_hash		 ARGS((char **wp));
int 	c_cd		 ARGS((char **wp));
int 	c_print		 ARGS((char **wp));
int 	c_whence	 ARGS((char **wp));
int 	c_typeset	 ARGS((char **wp));
int 	c_alias		 ARGS((char **wp));
int 	c_unalias	 ARGS((char **wp));
int 	c_let		 ARGS((char **wp));
int 	c_jobs		 ARGS((char **wp));
int 	c_fgbg		 ARGS((char **wp));
int 	c_kill		 ARGS((char **wp));
int 	c_bind		 ARGS((char **wp));
/* c_sh.c */
int 	c_label		 ARGS((char **wp));
int 	c_shift		 ARGS((char **wp));
int 	c_umask		 ARGS((char **wp));
int 	c_dot		 ARGS((char **wp));
int 	c_wait		 ARGS((char **wp));
int 	c_read		 ARGS((char **wp));
int 	c_eval		 ARGS((char **wp));
int 	c_trap		 ARGS((char **wp));
void 	setsig		 ARGS((struct trap *p, void (*f)()));
int 	c_return	 ARGS((char **wp));
int 	c_brkcont	 ARGS((char **wp));
int 	c_exit		 ARGS((char **wp));
int 	c_set		 ARGS((char **wp));
int 	c_unset		 ARGS((char **wp));
int 	c_ulimit	 ARGS((char **wp));
int 	c_times		 ARGS((char **wp));
int 	timex		 ARGS((struct op *t, int f));
int 	c_exec		 ARGS((char **wp));
int 	c_builtin	 ARGS((char **wp));
/* c_test.c */
int 	c_test		 ARGS((char **wp));
int 	oexpr		 ARGS((int n));
int 	aexpr		 ARGS((int n));
int 	nexpr		 ARGS((int n));
int 	primary		 ARGS((int n));
int 	filstat		 ARGS((char *nm, int mode));
int 	t_lex		 ARGS((char *s));
int 	newerf		 ARGS((char *f1, char *f2));
int 	olderf		 ARGS((char *f1, char *f2));
int 	equalf		 ARGS((char *f1, char *f2));
/* do_ulimit.c */
int 	do_ulimit	 ARGS((char *a1, char *a2));
/* edit.c */
int 	x_read		 ARGS((int fd, char *buf, size_t len));
int 	x_getc		 ARGS((void));
void 	x_flush		 ARGS((void));
void 	x_adjust	 ARGS((void));
void 	x_putc		 ARGS((int c));
void 	x_puts		 ARGS((char *s));
void 	x_init		 ARGS((void));
bool_t 	x_mode		 ARGS((bool_t onoff));
int 	promptlen	 ARGS((char *cp));
int 	init_editmode	 ARGS((void));
/* emacs.c */
int 	x_emacs		 ARGS((char *buf, size_t len));
void 	x_redraw	 ARGS((int limit));
void 	x_bind		 ARGS((char *a1, char *a2, int macro));
void 	x_init_emacs	 ARGS((void));
void 	x_emacs_keys	 ARGS((int erase, int kill, int werase, int intr, int quit));
char *	x_lastcp	 ARGS((void));
/* eval.c */
char *	substitute	 ARGS((char const *cp, int f));
char **	eval		 ARGS((char **ap, int f));
char *	evalstr		 ARGS((char *cp, int f));
/* exec.c */
int 	execute		 ARGS((struct op *t, volatile int flags));
int 	shcomexec	 ARGS((char **wp));
int 	define		 ARGS((char *name, struct op *t));
int 	builtin		 ARGS((char *name, int (*func)()));
struct tbl *	findcom	 ARGS((char *name, int insert));
int 	flushcom	 ARGS((int all));
char *	search		 ARGS((char *name, char *path, int mode));
/* expr.c */
void 	evalerr		 ARGS((char *err));
long 	evaluate	 ARGS((const char *expr));
/* getopts.c */
void 	resetopts	 ARGS((void));
int 	c_getopts	 ARGS((char **wp));
/* history.c */
int 	c_fc	 	 ARGS((register char **wp));
void 	histbackup	 ARGS((void));
void 	histsave	 ARGS((char *cmd));
char **	histget	 	 ARGS((char *str));
char *	histrpl	 	 ARGS((char *s, char *pat, char *rep, int global));
void 	hist_init	 ARGS((Source *s));
void 	hist_finish	 ARGS((void));
char **	histpos	 	 ARGS((void));
int 	histN	 	 ARGS((void));
int 	histnum	 	 ARGS((int n));
char *	findhist	 ARGS((int start, int fwd, char *str));
/* io.c */
int 	errorf		 ARGS((const char *fmt, ...));
int 	shellf		 ARGS((const char *fmt, ...));
void 	fopenshf	 ARGS((int fd));
void 	flushshf	 ARGS((int fd));
int 	savefd		 ARGS((int fd));
void 	restfd		 ARGS((int fd, int ofd));
void 	openpipe	 ARGS((int *pv));
void 	closepipe	 ARGS((int *pv));
struct temp *	maketemp ARGS((Area *ap));
/* jobs.c */
void 	j_init		 ARGS((void));
void 	j_exit		 ARGS((void));
void 	j_change	 ARGS((void));
int 	exchild		 ARGS((struct op *t, int flags));
int 	waitlast	 ARGS((void));
int 	j_reapchld	 ARGS((void));
int 	j_reap		 ARGS((void));
int 	waitfor		 ARGS((int job));
void 	j_kill		 ARGS((int job, int sig));
int 	j_resume	 ARGS((int job, int bg));
void 	j_jobs		 ARGS((void));
void 	j_notify	 ARGS((void));
int 	j_lookup	 ARGS((char *cp));
int 	j_stopped	 ARGS((void));
/* lex.c */
int 	yylex		 ARGS((int cf));
int 	gethere		 ARGS((void));
void 	yyerror		 ARGS((const char *msg));
Source * pushs		 ARGS((int type));
int 	pprompt		 ARGS((char *cp));
/* mail.c */
void 	mcheck		 ARGS((void));
void 	mbset		 ARGS((char *p));
void 	mpset		 ARGS((char *mptoparse));
void 	mprint		 ARGS((void));
/* main.c */
int 	main		 ARGS((int argc, char **argv, char **envp));
int 	include		 ARGS((char *name));
#if 0
int 	command		 ARGS((char *comm));
#endif
int 	shell		 ARGS((Source *s));
void 	leave		 ARGS((int rv));
int 	error		 ARGS((void));
int 	unwind		 ARGS((void));
int 	newenv		 ARGS((int type));
int 	quitenv		 ARGS((void));
void 	aerror		 ARGS((Area *ap, const char *msg));
/* misc.c */
void 	setctypes	 ARGS((/* const */ char *s, int t));
void 	initctypes	 ARGS((void));
char *	ulton		 ARGS((unsigned long n, int base));
char *	strsave		 ARGS((char *s, Area *ap));
int 	option		 ARGS((const char *n));
char *	getoptions	 ARGS((void));
void 	printoptions	 ARGS((void));
int 	getn		 ARGS((char *as));
char *	strerror	 ARGS((int i));
int 	gmatch		 ARGS((char *s, char *p));
void 	qsortp		 ARGS((void **base, size_t n, int (*f)()));
int 	qsort1		 ARGS((void **base, void **lim, int (*f)()));
int 	xstrcmp		 ARGS((void *p1, void *p2));
void 	cleanpath	 ARGS((char *pwd, char *dir, char *clean));
/* syn.c */
int 	yyparse		 ARGS((void));
int 	keywords	 ARGS((void));
struct op * compile	 ARGS((Source *s));
/* table.c */
unsigned int 	hash	 ARGS((char *n));
void 	tinit		 ARGS((struct table *tp, Area *ap));
struct tbl *	tsearch	 ARGS((struct table *tp, char *n, unsigned int h));
struct tbl *	tenter	 ARGS((struct table *tp, char *n, unsigned int h));
void 	tdelete		 ARGS((struct tbl *p));
void 	twalk		 ARGS((struct table *tp));
struct tbl *	tnext	 ARGS((void));
struct tbl **	tsort	 ARGS((struct table *tp));
/* trace.c */
/* trap.c */
Trap *	gettrap		 ARGS((char *name));
void 	trapsig		 ARGS((int i));
int 	runtraps	 ARGS((void));
int 	runtrap		 ARGS((Trap *p));
int 	cleartraps	 ARGS((void));
int 	ignoresig	 ARGS((int i));
int 	restoresigs	 ARGS((void));
/* tree.c */
void 	ptree		 ARGS((struct op *t, FILE *f));
int 	pioact		 ARGS((FILE *f, struct ioword *iop));
int 	fptreef		 ARGS((FILE *f, char *fmt, ...));
int 	snptreef	 ARGS((char *s, int n, char *fmt, ...));
struct op *	tcopy	 ARGS((struct op *t, Area *ap));
char *	wdcopy		 ARGS((char *wp, Area *ap));
char *	wdscan		 ARGS((char *wp, int c));
void 	tfree		 ARGS((struct op *t, Area *ap));
/* var.c */
void 	newblock	 ARGS((void));
void 	popblock	 ARGS((void));
struct tbl *	global	 ARGS((char *n));
struct tbl *	local	 ARGS((char *n));
char *	strval		 ARGS((struct tbl *vp));
long 	intval		 ARGS((struct tbl *vp));
void 	setstr		 ARGS((struct tbl *vq, char *s));
struct tbl *	strint	 ARGS((struct tbl *vq, struct tbl *vp));
void 	setint		 ARGS((struct tbl *vq, long n));
int 	import		 ARGS((char *thing));
struct tbl *	typeset	 ARGS((char *var, int set, int clr));
void 	unset		 ARGS((struct tbl *vp));
int 	isassign	 ARGS((char *s));
char **	makenv		 ARGS((void));
/* version.c */
/* vi.c */
void 	vi_reset	 ARGS((char *buf, int len));
int 	vi_hook		 ARGS((int ch));
int 	save_cbuf	 ARGS((void));
int 	restore_cbuf	 ARGS((void));
int 	x_vi		 ARGS((char *buf, size_t len));
int 	getch		 ARGS((void));
char **	globstr		 ARGS((char *stuff));


