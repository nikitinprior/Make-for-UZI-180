/*
 *	cpp.c	- LSI C preprocessor ver 1.5 written by Mori Koichiro
 *
 *	23-Jun-87	include
 *	24-Jun-87	define, ifdef, ifndef, else, endif
 *	25-Jun-87	if, elif, undef, -d option, -s option, line, #, ##
 *	27-Jun-87	add __eval__
 *	14-Jul-87	debug lineadj
 *	 5-Sep-87	automatic directory search
 *	17-Sep-87	add -i
 *	29-Sep-87	long computation in #if
 *	30-Sep-87	add -w
 *	25-Dec-87	debug lineadj
 *	08-Jan-88	debug error
 *	18-Feb-88	debug recursive death
 *				add __TIME__, __DATE__
 *	22-Feb-88	edit error messages
 *	23-Feb-88	add Japanese messages
 *	02-Mar-88	bug fix: character value
 *	03-Nov-1988 02:36	add -c option (allows nested comment)
 *	22-Dec-1988 17:29	add -y option (select character set)
 *	06-Feb-1989 22:21	add code to check the validity of #endif
 *	07-Feb-1989 16:53	...
 *	07-Feb-1989 17:50	add code to remove trailing blanks
 *						from macro parameter
 *	07-Feb-1989 18:09	add code to remove comments in #pragma
 *	08-Feb-1989 17:23	tako
 *	18-Apr-1989 14:56	change value of FILENEST: 6 -> 8
 *  --------------------------------------------------------------
 *	15-Mar-2016	Adaptation for use with Hi-Tech C v3.09 for CP/M:
 *				  - deleted -y option (select character set)
 *				  - deleted Japanese messages
 *				  - LSI compiler extensions removed (nonrec, recursive, etc.)
 *				  - 2-byte character support removed 
 *				  - default allow C++ style comments (// ...),
 *					command line option changes this purpose
 *				  - command line format changed
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <time.h>
#include <errno.h>

extern void *	sbrk(size_t);

typedef	char		bool, tiny;
typedef	unsigned	nat;

#define	YES	1
#define	NO	0

#define	MAXBUFF		10000
#define	MAXPARM		200
#define	FILENEST	8			/* Nested include	*/
#define	IFNEST		8			/* Nested if/#ifdef */
#define	MACNEST		100

bool	Debug[26];
bool	NoSharp;
bool	NoWarn;

bool	CommNest;
bool	BCPL_comment = YES;		/* default allow C++ style comments */

#if 0
bool	Keep_comment;			/* not implemented */
#endif

#define	MACFLAG	0x7F

#undef	MAXDIR
#define	MAXDIR	10				/* maximum number search paths for include files */

char	*Incdir[MAXDIR + 1];

typedef	struct	{
	FILE	*fp;
	char	name[80];
	int		lineno;
} FILEST;

FILEST	FileStack[FILENEST];
FILEST	*FileSp = FileStack - 1;

FILE	*Ofp;
int		Lineno;
int		Errcode = 0;


FILEST	IfStack[IFNEST];
int		Ifnest = 0;

/*
 *	error messages
 */
char	MSG_cntopnX[]	=	"can't open: %s: %s";
char	MSG_cntwrt[]	=	"can't write";
char	MSG_incnst[]	=	"#include too nested";
char	MSG_outmem[]	=	"out of memory";
char	MSG_bigmac[]	=	"too big macro";
char	MSG_unxeof[]	=	"unexpected EOF";
char	MSG_opencomm[]	=	"missing */";
char	MSG_Xmisopn[]	=	"%s: missing '('";
char	MSG_Xmiscls[]	=	"%s: missing ')'";
char	MSG_Xnumpar[]	=	"%s: conflicting # of parameters";
char	MSG_macnst[]	=	"macro too nested";
char	MSG_incmisquo[]	=	"#include: missing \"";
char	MSG_incmisbra[]	=	"#include: missing >";
char	MSG_incmisfil[]	=	"#include: missing filename";
char	MSG_defmisnam[]	=	"#define: missing macro name";
char	MSG_defbadpar[]	=	"#define: bad macro parameter";
char	MSG_undmisnam[]	=	"#undef: missing macro name";
char	MSG_misif[]		=	"missing #if";
char	MSG_misendif[]	=	"missing #endif";
char	MSG_ifnst[]		=	"#if/#ifdef too nested";
char	MSG_ifsynX[]	=	"#if/#elif: syntax error near %s";
char	MSG_badnum[]	=	"bad number '%s'";
char	MSG_ifdmisnam[]	=	"#ifdef: missing identifier";
char	MSG_linmisnum[]	=	"#line: missing number";
char	MSG_badstmX[]	=	"undefined stmt: #%s";
char	MSG_undefmacX[]	=	"undefined macro: %s";
char	MSG_errordir[] 	=	"#error: %s";
char	MSG_Xredef[]	=	"Warning: macro '%s' redefined";
char	MSG_Unkn_flag[]	=	"unknown flag '-%c'\n";
char	usage[] 		=	"usage: cpp [-s] [-w] [-c] [-Dmacro] [-Iincdir] [input] [output]\n";

/****************************************************************
 *	warn
 ****************************************************************/
void	warn(char *msg, char *s) {

	if (NoWarn)
		return;
	if (FileSp >= FileStack)
		fprintf(stderr, "%s %d: ", FileSp->name, FileSp->lineno);
	fprintf(stderr, msg, s);
	putc('\n', stderr);
	if (Errcode != 1)
		Errcode = 2;
}

/****************************************************************
 *	error1
 ****************************************************************/
void	error1(char *msg, char *s, char *file, int lineno) {

	if (file != NULL)
#if CPM
		fprintf(stderr, "%s %d: ", file, lineno);
#else
		fprintf(stderr, msg, s, strerror(errno));
#endif
	fprintf(stderr, msg, s);
	putc('\n', stderr);
	Errcode = 1;
}

/****************************************************************
 *	error
 ****************************************************************/
void	error(char *msg, char *s) {

	if (FileSp >= FileStack)
		error1(msg, s, FileSp->name, FileSp->lineno);
	else if ((FileSp + 1)->lineno != 0)
		error1(msg, s, (FileSp + 1)->name, (FileSp + 1)->lineno);
	else
		error1(msg, s, NULL, 0);
}

/****************************************************************
 *	bug
 ****************************************************************/
void	bug(char *msg) {

	if (FileSp >= FileStack)
		fprintf(stderr, "%s %d: ", FileSp->name, FileSp->lineno);
	fprintf(stderr, "cpp error (%s)\n", msg);
	exit(1);
}

/****************************************************************
 *	lineadj
 ****************************************************************/
void	lineadj() {
	int	n;

	if ((n = FileSp->lineno) != Lineno) {
		if (Lineno < 0 || n < Lineno) {
			Lineno = FileSp->lineno;
			if (!NoSharp)
				fprintf(Ofp, "# %d \"%s\"\n",
						Lineno, FileSp->name);
		} else {
			while (Lineno < n)
				putc('\n', Ofp), Lineno++;
		}
	}
}

/****************************************************************
 *	put
 ****************************************************************/
void	put(char c) {

	if (c == MACFLAG)
		bug("put");
	putc(c, Ofp);
	if (c == '\n')
		Lineno++;
}

/****************************************************************
 *	fput
 ****************************************************************/
void	fput(char *s) {
	char	c;

	while ((c = *s++))
		put(c);
}

/****************************************************************
 *	efopen
 ****************************************************************/
FILE	*efopen(char *file, char *mode) {
	FILE	*fp;

	if ((fp = fopen(file, mode)) == NULL)
		error(MSG_cntopnX, file), exit(1);
	return (fp);
}

/****************************************************************
 *	efclose
 ****************************************************************/
void	efclose(FILE *fp) {

	if (ferror(fp) || fclose(fp))
		error(MSG_cntwrt, NULL), exit(1);
}

/****************************************************************
 *	directory
 ****************************************************************/
char	*directory(char *buf, char *s) {
	char	*p, *r, c;

	r = p = buf;
	while ((c = *s++)) {
		*p++ = c;
		if (c == ':' || c == '/' || c == '\\')
			r = p;
	}
	*r = '\0';
	return (buf);
}

/****************************************************************
 *	appendsl
 ****************************************************************/
char	*appendsl(char *buf, char *s) {
	char	c;

	strcpy(buf, s);
	if (buf[0] != '\0') {
		c = buf[strlen(buf) - 1];
		if (c != ':' && c != '/' && c != '\\')
			strcat(buf, "\\");
	}
	return (buf);
}

/****************************************************************
 *	pushFile
 ****************************************************************/
void	pushFile(char *file, char tag, FILE *fp) {
	char	fn[100];
	int	i;

	if (FileSp >= FileStack + FILENEST - 1) {
		error(MSG_incnst, NULL);		/* "#include too nested" */
		return;
	}
	if (fp != NULL)
		;
	else if (tag == '"' && FileSp >= FileStack &&
		(fp = fopen(strcat(directory(fn, FileSp->name), file), "r")) != NULL)
		file = fn;
	else {
		fp = NULL;
		if (tag != ' ')
			for (i = 0; Incdir[i] != NULL; i++)
				if ((fp = fopen(strcat(appendsl(fn, Incdir[i]), file), "r")) != NULL)
					break;
		if (fp == NULL && (fp = fopen(file, "r")) == NULL) {
			error(MSG_cntopnX, file);	/* "can't open: %s: %s" */
			return;
		}
	}
	FileSp++;
	FileSp->fp = fp;
	strcpy(FileSp->name, file);
	FileSp->lineno = 1;
	Lineno = -1;
	lineadj();
}

/****************************************************************
 *	rawnew
 ****************************************************************/
void	*rawnew(nat s) {
	void	*p;

	if ((p = sbrk(s)) == (void *)-1)
		error(MSG_outmem, NULL), exit(1);
	memset(p, 0, s);
	return (p);
}

#define	new(t, n)	((t *)rawnew(sizeof(t) * (n)))

/****************************************************************
 *	copy
 ****************************************************************/
char	*copy(char *s) {

	return (strcpy(new(char, strlen(s) + 1), s));
}



typedef	struct macro	{
	struct macro	*next;
	char		*name;
	union	{
		char	*body;
		void	(*func)();
	}		u;
	char		flags;
#define	DEFINED	001
#define	ISFUNC	002
	int		nparms;
} MACRO;

#define	HASHSIZE	1024

MACRO	*HashTbl[HASHSIZE];



char	Pool[MAXBUFF];
MACRO	*SleepStack[MACNEST];
MACRO	**Sleep = SleepStack;

char	*Ungetp = Pool + MAXBUFF;
char	*Token = Pool;
char	*Tail;
char	Lex, EvalLex;


bool	DirectiveMode = NO;

bool	Undeferr;



/****************************************************************
 *	checkPool
 ****************************************************************/
void	checkPool(char *p) {

	if (p + 1 >= Ungetp)
		error(MSG_bigmac, NULL), exit(1);	/* "too big macro" */
}

/****************************************************************
 *	unget
 ****************************************************************/
void	unget(char c) {

	if (c != 0) {
		checkPool(Token);
		if ((*--Ungetp = c) == '\n')
			FileSp->lineno--;
	}
}

/****************************************************************
 *	ungets
 ****************************************************************/
void	ungets(char *s) {
	char *q;

	for (q = s + strlen(s); --q >= s; )
		unget(*q);
}

/****************************************************************
 *	get1
 ****************************************************************/
char	get1() {
	int		c;
	bool	fc;

	if (Ungetp < Pool + MAXBUFF)
		c = *Ungetp++;
	else {
		fc = NO;
		for (;;) {
			if (FileSp < FileStack)
				return (0);
			while ((c = getc(FileSp->fp)) == MACFLAG)
				;
			if (c != EOF)
				break;
			efclose(FileSp->fp);
			Lineno = -2;
			fc = YES;
			FileSp--;
		}
		if (fc)
			lineadj();
	}
	if (c == '\n')
		FileSp->lineno++;
	return (c);
}

/****************************************************************
 *	geteof
 ****************************************************************/
char	geteof() {
	char	c;

	if (DirectiveMode) {
		while ((c = get1()) == '\\')
			if ((c = get1()) != '\n') {
				unget(c);
				c = '\\';
				break;
			}
	} else
		c = get1();
	return (c);
}

/****************************************************************
 *	get
 ****************************************************************/
char	get() {
	char	c;

	if ((c = geteof()) == 0)
		error(MSG_unxeof, NULL), exit(1);
	return (c);
}


#define	NAME	'a'
#define	NUMBER	'0'

#define	SHR	'b'
#define	SHL	'c'
#define	LE	'd'
#define	GE	'e'
#define	NE	'f'
#define	EQ	'g'
#define	AND	'h'
#define	OR	'i'



#define	isletter(c)	(isalpha(c) || (c) == '_')
#define	iswhite(c)	((c) == ' ' || (c) == '\t' || (c) == '\f' || (c) == '\v')


/****************************************************************
 *	gettok_eof
 ****************************************************************/
char	gettok_eof() {
	char	c, d;
	char	*p;

	if ((c = geteof()) == '/') {
		c = geteof();
		if (c == '*') {
			/* skip comment & change it to ' ' */
			FILEST	*fsave = FileSp;
			int	cc, lsave = fsave->lineno;

			c = 0;
			cc = 1;
			for (;;) {
				d = c;
				c = geteof();
				if (c == 0) {
					error1(MSG_opencomm, NULL, fsave->name, lsave);
					exit(1);
				}
				if (d == '*' && c == '/') {
					if (--cc == 0)
						break;
					c = 0;
				}
				if (CommNest && d == '/' && c == '*') {
					cc++;
					c = 0;
				}
			}
			c = ' ';
		} else if (BCPL_comment && c == '/') {
			/* skip C++ comment (originally BCPL style) */
			while ((c = geteof()) != '\n' && c != 0)
				;
		} else {
			unget(c);
			c = '/';
		}
	}
	d = c;
	p = Token;
	if (isletter(c) || isdigit(c)) {
		d = isdigit(c) ? NUMBER : NAME;
		while (isletter(c) || isdigit(c)) {
			*p++ = c;
			c = geteof();
		}
		unget(c);
	} else if (c == '"' || c == '\'') {
		*p++ = c;
		while (c = get(), c != d && c != '\n' && c != '\0') {
			*p++ = c;
			if (c == '\\')
				*p++ = get();
		}
		if (c == '\n')
			unget(c);
		else
			*p++ = c;
	} else
		*p++ = c;
	*p = '\0';
	checkPool(p);
	Tail = p;
	return (Lex = d);
}


/****************************************************************
 *	gettok
 ****************************************************************/
char	gettok() {
	char	c;

	if ((c = gettok_eof()) == 0)
		error(MSG_unxeof, NULL), exit(1);
	return (c);
}


/****************************************************************
 *	skip_gettok
 ****************************************************************/
char	skip_gettok() {
	char	t;

	while (t = gettok(), iswhite(t))
		;
	return (t);
}


/****************************************************************
 *	hash
 ****************************************************************/
nat	hash(char *s) {
	nat	v;
	char	c;

	v = 0;
	while ((c = *s++))
		v = v * 257 + c;
	return (v % HASHSIZE);
}

/****************************************************************
 *	roomfor
 ****************************************************************/
MACRO	*roomfor(char *name) {
	MACRO	*p, **r;

	r = &HashTbl[hash(name)];
	for (p = *r; p != NULL; p = p->next)
		if (strcmp(p->name, name) == 0)
			break;
	if (p == NULL) {
		p = new(MACRO, 1);
		p->name = copy(name);
		p->next = *r;
		*r = p;
	}
	return (p);
}

/****************************************************************
 *	lookup
 ****************************************************************/
MACRO	*lookup(char *name) {
	MACRO	*p;

	for (p = HashTbl[hash(name)]; p != NULL; p = p->next)
		if (strcmp(p->name, name) == 0)
			return (p->flags & DEFINED ? p : (MACRO	*)NULL);
	return (NULL);
}

/****************************************************************
 *	installf
 ****************************************************************/
void	installf(char *name, void (*func)()) {
	MACRO	*p;

	p = roomfor(name);
	p->u.func = func;
	p->flags = DEFINED | ISFUNC;
	p->nparms = -1;
}

/****************************************************************
 *	install
 ****************************************************************/
void	install(char *name, char *body, int nparms) {
	MACRO	*p;

	p = roomfor(name);
	if (p->flags & DEFINED &&
		(p->flags & ISFUNC || strcmp(p->u.body, body) != 0))
		warn(MSG_Xredef, name);
	p->u.body = copy(body);
	p->flags = DEFINED;
	p->nparms = nparms;
	if (Debug['d'-'a'])
		fprintf(stderr, "[debug] %s(%d) = {%s}\n",
					p->name, p->nparms, p->u.body);
}



char	skip_egettok(void);
char	egettok(void);
long	eval0(tiny);

/****************************************************************
 *	macFile
 ****************************************************************/
void	macFile() {
	/*
	 * replaced by current filename
	 */
	unget('"');
	ungets(FileSp->name);
	unget('"');
}

/****************************************************************
 *	macLine
 ****************************************************************/
void	macLine() {
	/*
	 * replaced by current line#
	 */
	char	buff[10];

	sprintf(buff, "%d", FileSp->lineno);
	ungets(buff);
}


struct	tm	*Timep;

/****************************************************************
 *	macTime
 ****************************************************************/
void	macTime() {
	/*
	 * replaced by current time
	 */
	char	buff[12];

	sprintf(buff, "\"%02d:%02d:%02d\"",
			Timep->tm_hour, Timep->tm_min, Timep->tm_sec);
	ungets(buff);
}

/****************************************************************
 *	macDate
 ****************************************************************/
void	macDate() {
	/*
	 * replaced by current date
	 */
	static	char	*moname[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	char	buff[14];

	sprintf(buff, "\"%s %02d %04d\"",
		moname[Timep->tm_mon], Timep->tm_mday, 1900 + Timep->tm_year);
	ungets(buff);
}

/****************************************************************
 *	macEval - recursive
 ****************************************************************/
void	macEval() {

	/*
	 * replaced by evaluated value
	 */
	char	buff[12];
	bool 	undeferr;

	if (skip_egettok() != '(') {
		error(MSG_Xmisopn, "__eval__");
		ungets(Token);
		return;
	}
	undeferr = Undeferr;
	Undeferr = YES;
	sprintf(buff, "%ld", eval0(0));
	Undeferr = undeferr;
	if (EvalLex != ')')
		error(MSG_Xmiscls, "__eval__");
	ungets(buff);
}

/****************************************************************
 *	egettok_eof - recursive
 ****************************************************************/
char	egettok_eof() {
	char	t;
	MACRO	*p;
	int		i, nest;
	char	*save, *q, *s;
	FILEST	*fsave;
	int		lsave;
	char	*Parms[MAXPARM];

	for (;;) {
		if ((t = gettok_eof()) == MACFLAG) {
			/* wake up sleeping macro */
			if (Sleep <= SleepStack)
				bug("egettok");
			(*--Sleep)->flags |= DEFINED;
			continue;
		}
		if (t != NAME || (p = lookup(Token)) == NULL)
			break;
		if (p->flags & ISFUNC) {
			/* built-in macro */
			(*p->u.func)();
			continue;
		}
		/* replaced by macro body */
		save = Token;
		fsave = FileSp;
		lsave = fsave->lineno;
		if (p->nparms >= 0) {
			/* expecting parameters */
			if ((t = skip_gettok()) != '(') {
#if	0
/* delete 24-Jul-1989 */
				error(MSG_Xmisopn, p->name);
				return (t);
#endif
				ungets(Token);
				unget(' ');
				ungets(p->name);
				return (gettok());
			}
			t = skip_egettok();
			for (i = 0; t != ')'; i++) {
				/* get a parameter */
				if (t == 0) {
					FileSp = fsave;
					fsave->lineno = lsave;
					error(MSG_Xmiscls, p->name);
					exit(1);
				}
				if (i >= MAXPARM) {
					error(MSG_Xmiscls, p->name);
					Token = save;
					return (t);
				}
				while (iswhite(t) || t == '\n')
					t = egettok();
				Parms[i] = Token;
				nest = 0;
/*
				(t != 0 && (t != ',' && t != ')' || nest != 0))
				(          (                                 ))
*/

				while (t != 0 && (t != ',' && t != ')' || nest != 0)) {
					if (t == '(')
						nest++;
					else if (t == ')')
						nest--;
					else if (t == '\n')
						*Token = ' ';
					Token = Tail;
					t = egettok();
				}
				while (Token > Parms[i] && iswhite(Token[-1]))
					Token--;
				*Token++ = '\0';
				if (t == ',')
					t = skip_egettok();
			}
			if (i != p->nparms)
				error(MSG_Xnumpar, p->name);
			for (; i < p->nparms; i++)
				Parms[i] = "";
		}
		/* let macro p sleep */
		p->flags &= ~DEFINED;
		if (Sleep >= SleepStack + MACNEST)
			error(MSG_macnst, NULL), exit(1);
		*Sleep++ = p;
		unget(MACFLAG);
		/* expand body */
		s = p->u.body;
		for (q = s + strlen(s); --q >= s; )
			if (*q == MACFLAG)
				ungets(Parms[get() - '0']);
			else
				unget(*q);
		Token = save;
	}
	return (t);
}

/****************************************************************
 *	egettok - recursive
 ****************************************************************/
char	egettok() {
	char	c;

	if ((c = egettok_eof()) == 0)
		error(MSG_unxeof, NULL), exit(1);
	return (c);
}

/****************************************************************
 *	skip_egettok - recursive
 ****************************************************************/
char	skip_egettok() {
	char	t;

	while (t = egettok(), iswhite(t))
		;
	return (t);
}

/****************************************************************
 *	skip_eol
 ****************************************************************/
void	skip_eol() {

	while (Lex != '\n')
		gettok();
	put('\n');
}

/****************************************************************
 *	do_include
 ****************************************************************/
void	do_include() {
	char	t, c, buff[100], *p;

	t = skip_egettok();
	if (t == '"') {
		strcpy(buff, Token + 1);
		if (buff[strlen(buff) - 1] != '"')
			error(MSG_incmisquo, NULL);		/* "#include: missing \"" */
		buff[strlen(buff) - 1] = '\0';
	}
	else if (t == '<') {
		p = buff;
		while ((c = get()) != '>') {
			if (c == '\n') {
				error(MSG_incmisbra, NULL);	/* "#include: missing >" */
				return;
			}
			*p++ = c;
		}
		*p = '\0';
	}
	else {
		error(MSG_incmisfil, NULL);			/* "#include: missing filename" */
		return;
	}
#if	0
	skip_eol();
#endif
	pushFile(buff, t, NULL);
}

/****************************************************************
 *	do_define
 ****************************************************************/
void	do_define() {
	char	*name, *body, *p;
	char	t;
	int		i, np;
	bool	quote;
	char	*Parms[MAXPARM];

	/* get macro name */
	if (skip_gettok() != NAME) {
		error(MSG_defmisnam, NULL);		/* "#define: missing macro name" */
		skip_eol();
		return;
	}
	name = Token;
	Token = Tail + 1;
	np = -1;
	if ((t = gettok()) == '(') {
		/* parameter macro */
		np = 0;
		while ((t = skip_gettok()) == NAME) {
			Parms[np++] = Token;
			Token = Tail + 1;
			if ((t = skip_gettok()) != ',')
				break;
		}
		if (t != ')') {
			error(MSG_defbadpar, NULL);	/* "#define: bad macro parameter" */
			skip_eol();
			Token = Pool;
			return;
		}
		t = ' ';
	}
	/* get macro body */
	while (iswhite(t))
		t = gettok();
	body = p = Token;
	quote = NO;
	for (; t != '\n'; t = gettok()) {
		if (t == '#') {
			if ((t = get()) == '#') {
				/* ##: name concatenation */
				Token = p;
				t = get();
			} else {
				/* #: surround by double quote */
				quote = YES;
				Token = Tail;
			}
			while (iswhite(t))
				t = get();
			unget(t);
			continue;
		}
		if (quote)
			Token[-1] = '"';	/* change # to " */
		if (t == NAME) {
			for (i = 0; i < np; i++)
				if (strcmp(Parms[i], Token) == 0) {
					Token[0] = MACFLAG;
					Token[1] = '0' + i;
					Tail = Token + 2;
					break;
				}
		}
		if (quote)
			*Tail++ = '"';
		Token = Tail;
		if (! iswhite(t))
			p = Token;
		quote = NO;
	}
	*p = '\0';
	unget(t);
	/* register */
	install(name, body, np);
	Token = Pool;
}

/****************************************************************
 *	do_undef
 ****************************************************************/
void	do_undef() {
	char	t;
	MACRO	*p;

	if ((t = skip_gettok()) != NAME)
		error(MSG_undmisnam, NULL);		/* "#undef: missing macro name" */
	else {
		if ((p = lookup(Token)) != NULL)
			p->flags &= ~DEFINED;
	}
	skip_eol();
}

/*	
 *	Conditional Compilation
 */

/****************************************************************
 *	search_else_endif
 ****************************************************************/
int	search_else_endif() {
	int	nest;
	char	t;
	FILEST	*fsave;
	int	lsave;

	fsave = FileSp;
	lsave = fsave->lineno;
	t = 0;
	nest = 1;
	for (;;) {
		skip_eol();
		if ((t = skip_gettok()) == '#') {
			t = skip_egettok();
			if (strcmp(Token, "if") == 0 ||
				strcmp(Token, "ifdef") == 0 ||
				strcmp(Token, "ifndef") == 0)
				nest++;
			else if (strcmp(Token, "endif") == 0) {
				if (--nest == 0) {
					Ifnest--;
					return (0);
				}
			} else if (nest == 1 &&
				(strcmp(Token, "else") == 0 ||
					strcmp(Token, "elif") == 0))
				return (1);
		} else if (t == 0) {
			error1(MSG_misendif, NULL, fsave->name, lsave);
			exit(1);
		}
	}
}


/*
 * 		Special lex for #if
 */

/****************************************************************
 *	twinlex
 ****************************************************************/
char	twinlex(char c1, char c2, char l2, char c3, char l3) {
	char	c;

	if ((c = get()) == c2)
		return (l2);
	if (c == c3)
		return (l3);
	unget(c);
	return (c1);
}

/****************************************************************
 *	getlex1
 ****************************************************************/
char	getlex1(char lex) {

	switch (lex) {
	case '<':
		return (twinlex('<', '<', SHR, '=', LE));
	case '>':
		return (twinlex('>', '>', SHL, '=', GE));
	case '=':
		return (twinlex('=', '=', EQ, 0, 0));
	case '!':
		return (twinlex('!', '=', NE, 0, 0));
	case '&':
		return (twinlex('&', '&', AND, 0, 0));
	case '|':
		return (twinlex('|', '|', OR, 0, 0));
	default:
		return (lex);
	}
}

/****************************************************************
 *	ugetlex
 ****************************************************************/
char	ugetlex() {

	return (EvalLex = getlex1(skip_gettok()));
}

/****************************************************************
 *	getlex - recursive
 ****************************************************************/
char	getlex() {

	return (EvalLex = getlex1(skip_egettok()));
}

/****************************************************************
 *	levelof
 ****************************************************************/
tiny	levelof(char lex) {

	switch (lex) {
	case '?':	return (0);
	case OR:	return (1);
	case AND:	return (2);
	case '|':	return (3);
	case '^':	return (4);
	case '&':	return (5);
	case EQ:
	case NE:	return (6);
	case '<':
	case LE:
	case '>':
	case GE:	return (7);
	case SHR:
	case SHL:	return (8);
	case '+':
	case '-':	return (9);
	case '*':
	case '/':
	case '%':	return (10);
	default:	return (11);
	}
}

#define	PRIMLEV	11

jmp_buf	Env;

/****************************************************************
 *	synerror
 ****************************************************************/
void	synerror() {

	error(MSG_ifsynX, Token);
	skip_eol();
	longjmp(Env, 1);
}

/****************************************************************
 *	checkLex
 ****************************************************************/
void	checkLex(char lex) {

	if (EvalLex != lex)
		synerror();
}

/****************************************************************
 *	digval
 ****************************************************************/
char	digval(char c) {

	c = (isupper(c) ? c + ('a' - 'A') : c);	 /*	c = tolower(c);	 */
	if (isdigit(c)) {
		return (c - '0');
	}
	if (islower(c)) {
		return (c - 'a' + 10);
	}
	return (100);
}


/****************************************************************
 *
 ****************************************************************/
long	valof(char *s) {
	long	v, radix;
	char	c, *p;

	p = s;
	c = *p;
	if (c != '0')
		radix = 10;						/* decimal */
	else if (*++p == 'x' || *p == 'X')
		p++, radix = 16;				/* hexadecimal */
	else
		radix = 8;						/* octal */
	v = 0;
	while ((c = digval(*p)) < radix) {
		v = v * radix + c;
		p++;
	}
	/* skip suffixes */
	c = (isupper(c) ? c + ('a' - 'A') : c);	 /*	c = tolower(c);	 */
	while (c  == 'u' || c == 'l')
		p++;
	if (*p)
		error(MSG_badnum, s);
	return (v);
}

/****************************************************************
 *
 ****************************************************************/
long	chaval(char *s) {
	char	c, d;
	int		n;
	long	v;

	v = 0;
	while ((c = *s++) != '\0' && c != '\'') {
		if (*s) {
			v = (v << 16) | (c << 8) | *s++;
			continue;
		}
		if (c == '\\')
			switch (c = *s++) {
			case 'a':	c = '\a';	break;
			case 'b':	c = '\b';	break;
			case 'f':	c = '\f';	break;
			case 'n':	c = '\n';	break;
			case 'r':	c = '\r';	break;
			case 't':	c = '\t';	break;
			case 'v':	c = '\v';	break;
			case 'x':
				c = 0;
				for (n = 0; n < 3; n++) {
					if ((d = digval(*s)) >= 16)
						break;
					c = c << 4 | d;
					s++;
				}
				break;
			default:
				if ('0' <= c && c <= '7') {
					s--;
					c = 0;
					for (n = 0; n < 3; n++) {
						if ((d = digval(*s)) >= 8)
							break;
						c = c << 3 | d;
						s++;
					}
				}
			}
		v = v << 8 | c;
	}
	return (v);
}

/****************************************************************
 *	eval1 - recursive
 ****************************************************************/
long	eval1(tiny lev) {
	long	v, w, u;
	char	op;

	if (lev == PRIMLEV) {
		if (EvalLex == '(') {
			v = eval0(0);
			checkLex(')');
			getlex();
			return (v);
		}
		if (EvalLex == '-')
			return (-eval0(PRIMLEV));
		if (EvalLex == '+')
			return (eval0(PRIMLEV));
		if (EvalLex == '~')
			return (~eval0(PRIMLEV));
		if (EvalLex == '!')
			return (!eval0(PRIMLEV));
		if (EvalLex == NUMBER) {
			v = valof(Token);
			getlex();
			return (v);
		}
		if (EvalLex == '\'') {
			v = chaval(Token + 1);
			getlex();
			return (v);
		}
		if (EvalLex == NAME && strcmp(Token, "defined") == 0) {
			if (ugetlex() == '(') {
				ugetlex(), checkLex(NAME);
				v = lookup(Token) != NULL;
				getlex(), checkLex(')');
			} else
				v = lookup(Token) != NULL;
			getlex();
			return (v);
		}
		if (EvalLex == NAME) {	/* undefined macro name */
			if (Undeferr)
				error(MSG_undefmacX, Token);
			getlex();
			return (0);
		}
		synerror();
	}
	v = eval1(lev + 1);
	while (levelof(EvalLex) == lev) {
		op = EvalLex;
		if (op == '?') {
			w = eval0(lev);
			checkLex(':');
			u = eval0(lev);
			return (v ? w : u);
		}
		w = eval0(lev + 1);		
		switch (op) {
		case '+':	v += w;		break;
		case '-':	v -= w;		break;
		case '*':	v *= w;		break;
		case '/':	v /= w;		break;
		case '%':	v %= w;		break;
		case '&':	v &= w;		break;
		case '|':	v |= w;		break;
		case '^':	v ^= w;		break;
		case SHR:	v <<= w;	break;
		case SHL:	v >>= w;	break;
		case EQ:	v = v == w;	break;
		case NE:	v = v != w;	break;
		case '>':	v = v > w;	break;
		case '<':	v = v < w;	break;
		case GE:	v = v >= w;	break;
		case LE:	v = v <= w;	break;
		case AND:	v = v && w;	break;
		case OR:	v = v || w;	break;
		default:	bug("eval1");
		}
	}
	return (v);
}

/****************************************************************
 *	eval0 - recursive
 ****************************************************************/
long	eval0(tiny lev) {

	getlex();
	return (eval1(lev));
}

/****************************************************************
 *	eval
 ****************************************************************/
long	eval() {
	long	v;

	if (setjmp(Env) == 0) {
		v = eval0(0);
		checkLex('\n');
		return (v);
	} else
		return (0);
}

/****************************************************************
 *	do_if
 ****************************************************************/
void	do_if() {

	if (Ifnest >= IFNEST)
		error(MSG_ifnst, NULL);
	else
		memcpy(&IfStack[Ifnest++], FileSp, sizeof(*FileSp));
	if (eval() == 0) {
		do {
			search_else_endif();
			if (strcmp(Token, "elif") != 0) {
				skip_eol();
				return;
			}
		} while (eval() == 0);
	}
}

/****************************************************************
 *	do_ifdef
 ****************************************************************/
void	do_ifdef(bool whendef) {

	if (Ifnest >= IFNEST)
		error(MSG_ifnst, NULL);
	else
		memcpy(&IfStack[Ifnest++], FileSp, sizeof(*FileSp));
	if (skip_gettok() != NAME)
		error(MSG_ifdmisnam, NULL);
	else if ((lookup(Token) != NULL) != whendef) {
		do {
			search_else_endif();
			if (strcmp(Token, "elif") != 0) {
				skip_eol();
				return;
			}
		} while (eval() == 0);
	}
}

/****************************************************************
 *	do_else_elif
 ****************************************************************/
void	do_else_elif() {

	if (Ifnest == 0)
		error(MSG_misif, NULL);
	/* skip until #endif */
	while (search_else_endif() != 0)
		;
	skip_eol();
}

/****************************************************************
 *	do_endif
 ****************************************************************/
void	do_endif() {

	if (Ifnest == 0)
		error(MSG_misif, NULL);
	else
		Ifnest--;
	skip_eol();
}

/****************************************************************
 *	do_line
 ****************************************************************/
void	do_line() {

	if (skip_egettok() != NUMBER) {
		error(MSG_linmisnum, NULL);
		skip_eol();
		return;
	}
	FileSp->lineno = atoi(Token) - 1;
	if (skip_egettok() == '"') {
		Token[strlen(Token) - 1] = '\0';
		strcpy(FileSp->name, Token + 1);
	}
	while (Lex != '\n')
		gettok();
	Lineno = -1;
}

/****************************************************************
 *	do_pragma
 ****************************************************************/
void	do_pragma() {
	char	c;

	if (NoSharp)
		skip_eol();
	else {
		fput("#p");
		do {
			c = gettok();
			fput(Token);
		} while (c != '\n');
	}
}

/****************************************************************
 *	do_error
 ****************************************************************/
void	do_error() {

	char	msg[100];
	char	*p, c;

	p = msg;
	while ((c = get()) == ' ' || c == '\t')
		;
	while (p < msg + sizeof(msg) - 1 && c != '\n' && c != 0) {
		*p++ = c;
		c = get();
	}
	*p = '\0';
	while (c != '\n' && c != 0)
		c = get();
	unget(c);
	error(MSG_errordir, msg);
	get();
}

/****************************************************************
 *	do_cpp
 ****************************************************************/
void	do_cpp() {
	bool	tol;
	char	t;

	/* main loop */
	tol = YES;
	t = 0;
	while ((t = egettok_eof()) != 0) {
		if (t == '#' && tol) {
			DirectiveMode = YES;
			t = skip_gettok();
			if (t == '\n')
				;
			else if (strcmp(Token, "include") == 0)
				do_include();
			else if (strcmp(Token, "define") == 0)
				do_define();
			else if (strcmp(Token, "undef") == 0)
				do_undef();
			else if (strcmp(Token, "if") == 0)
				do_if();
			else if (strcmp(Token, "ifdef") == 0)
				do_ifdef(YES);
			else if (strcmp(Token, "ifndef") == 0)
				do_ifdef(NO);
			else if (strcmp(Token, "elif") == 0)
				do_else_elif();
			else if (strcmp(Token, "else") == 0)
				do_else_elif();
			else if (strcmp(Token, "endif") == 0)
				do_endif();
			else if (strcmp(Token, "line") == 0)
				do_line();
			else if (strcmp(Token, "pragma") == 0)
				do_pragma();
			else if (strcmp(Token, "error") == 0)
				do_error();
			else {
				error(MSG_badstmX, Token);	/* undefined stmt */
				skip_eol();
			}
			DirectiveMode = NO;
			lineadj();
		} else {
			fput(Token);
			if (t == '\n') {
				lineadj();
				tol = YES;
			}
			else if (!isspace(t))
				tol = NO;
		}
	}
	if (Ifnest != 0) {
		Ifnest--;
		error1(MSG_misendif, NULL, IfStack[Ifnest].name, IfStack[Ifnest].lineno);
	}
}

/****************************************************************
 *	main
 ****************************************************************/
int main(int argc, char *argv[]) {

	char	c, *p, *s, *outf;
	int		inc;
	char	name[100];
	time_t	t;

	time(&t);
	Timep = localtime(&t);
	inc = 0;
	installf("__FILE__", macFile);
	installf("__LINE__", macLine);
	installf("__TIME__", macTime);
	installf("__DATE__", macDate);
	installf("__eval__", macEval);
	outf = NULL;

	while (++argv, --argc != 0 && argv[0][0] == '-') {
		p = argv[0] + 1;
		switch (*p++) {
		case 'x':				/* Debug mode */
			while ((c = *p++))
				if (isalpha(c))
					Debug[(	(isupper(c) ? c + ('a' - 'A') : c) ) - 'a'] = YES;
				else if (c == '#')
					memset(Debug, YES, 26);
			break;
		case 'w':				/* Disables displaying warning messages */
			NoWarn = YES;
			break;
		case 'B':				/* Disable C++ style comments */
			BCPL_comment = NO;
			break;
#if 0
		case 'C':				/* Do not discard comments */
			Keep_comment = YES;
			break;
#endif
		case 'D':				/* get macro definition */
			s = name;
			while ((c = *p++) != '\0' && c != '=') {
				*s++ = c;
			}
			*s = '\0';
			if (c == '\0')
				p = "1";
			install(name, p, -1);
			break;
		case 'I':				/* include directory */
			if (inc >= MAXDIR)
				fprintf(stderr, "too many -i\n"), exit(1);
			Incdir[inc++] = p;
			break;
		case 's':			/* Do not send information about line number to output. */
			NoSharp = YES;
			break;
#if 0
		case 'o':			/* Assign a name to output file */
			if (*p != '\0')
				outf = p;
			else if (++argv, --argc != 0)
				outf = *argv;
			else
				fprintf(stderr, usage), exit(1);
			break;
#endif
		case 'c':			/* Allow nested comments */
			CommNest = YES;
			break;
		default:
			fprintf(stderr, MSG_Unkn_flag, *(p-1));
			fprintf(stderr, "%s", usage), exit(1);
		}
	}
	Incdir[inc] = NULL;
	if (argc == 2) {				/* options + inpup + output files */
		outf = *(argv+1);
	}
	if (outf == NULL)
		Ofp = stdout;
	else {
		Ofp = efopen(outf, "w");
	}
	if (argc == 0)
		pushFile("", ' ', stdin);
	else
		pushFile(argv[0], ' ', NULL);
	do_cpp();
	efclose(Ofp);
	exit(Errcode);
}

