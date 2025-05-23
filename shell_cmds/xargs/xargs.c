/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * John B. Roll Jr.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $xMach: xargs.c,v 1.6 2002/02/23 05:27:47 tim Exp $
 */

#if 0
#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)xargs.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/usr.bin/xargs/xargs.c,v 1.57 2005/02/27 02:01:31 gad Exp $");

#include <sys/param.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <langinfo.h>
#include <locale.h>
#include <paths.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <TargetConditionals.h>
#if defined(TARGET_OS_IPHONE) || defined(TARGET_OS_WATCH) || defined(TARGET_OS_TV) || defined(TARGET_OS_MACCATALYST)
#include "ios_error.h"
#undef stderr
#define stderr thread_stderr
#endif
#include "pathnames.h"

#ifdef __APPLE__
// #include <get_compat.h>
// #else
#define COMPAT_MODE(a,b) (1)
#endif /* __APPLE__ */

static void	parse_input(int, char *[]);
static void	prerun(int, char *[]);
static int	prompt(void);
static void	run(char **);
static void	usage(void);
void		strnsubst(char **, const char *, const char *, size_t);
static void	waitchildren(const char *, int);

static __thread int last_was_newline = 1;
static __thread int last_was_blank = 0;

static char echo[] = _PATH_ECHO;
static __thread char **av, **bxp, **ep, **endxp, **xp;
static __thread char *argp, *bbp, *ebp, *inpline, *p, *replstr;
static const char *eofstr;
static __thread int count, insingle, indouble, oflag, pflag, tflag, Rflag, rval, zflag;
static __thread int cnt, Iflag, jfound, Lflag, wasquoted, xflag;
static __thread int curprocs, maxprocs;
static size_t pad9314053;

static __thread volatile int childerr;

extern char **environ;

int
xargs_main(int argc, char *argv[])
{
	long arg_max;
	int ch, Jflag, nflag, nline;
	size_t nargs;
	size_t linelen;
	char *endptr;

	inpline = replstr = NULL;
#if !TARGET_OS_IPHONE && !TARGET_OS_WATCH && !TARGET_OS_TV && !TARGET_OS_MACCATALYST
	ep = environ;
#else
    ep = environmentVariables(ios_currentPid());
#endif
	eofstr = "";
	Jflag = nflag = 0;
    
    // Init all flags and variables:
    last_was_newline = 1;
    last_was_blank = 0;
    argp = NULL;
    bbp = NULL;
    ebp = NULL;
    inpline = NULL;
    p = NULL;
    replstr = NULL;
    count = 0;
    insingle = 0;
    indouble = 0;
    oflag = 0;
    pflag = 0;
    tflag = 0;
    Rflag = 0;
    rval = 0;
    zflag = 0;
    cnt = 0;
    Iflag = 0;
    jfound = 0;
    Lflag = 0;
    wasquoted = 0;
    xflag = 0;
    curprocs = 0;
    maxprocs = 0;
    childerr = 0;
    // end init
    
	(void)setlocale(LC_ALL, "");

	/*
	 * POSIX.2 limits the exec line length to ARG_MAX - 2K.  Running that
	 * caused some E2BIG errors, so it was changed to ARG_MAX - 4K.  Given
	 * that the smallest argument is 2 bytes in length, this means that
	 * the number of arguments is limited to:
	 *
	 *	 (ARG_MAX - 4K - LENGTH(utility + arguments)) / 2.
	 *
	 * We arbitrarily limit the number of arguments to 5000.  This is
	 * allowed by POSIX.2 as long as the resulting minimum exec line is
	 * at least LINE_MAX.  Realloc'ing as necessary is possible, but
	 * probably not worthwhile.
	 */
	nargs = 5000;
#if! TARGET_OS_IPHONE
    if ((arg_max = sysconf(_SC_ARG_MAX)) == -1) {
		errx(1, "sysconf(_SC_ARG_MAX) failed");
    }
#else
    arg_max = ARG_MAX;
#endif
	nline = arg_max - MAXPATHLEN; /* for argv[0] from execvp() */
	pad9314053 = sizeof(char *); /* reserve for string area rounding */
	while (*ep != NULL) {
		/* 1 byte for each '\0' */
		nline -= strlen(*ep++) + 1 + sizeof(*ep);
	}
	nline -= pad9314053;
	maxprocs = 1;
	while ((ch = getopt(argc, argv, "0E:I:J:L:n:oP:pR:s:tx")) != -1)
		switch(ch) {
		case 'E':
			eofstr = optarg;
			break;
		case 'I':
			Jflag = 0;
			Iflag = 1;
			Lflag = 1;
			replstr = optarg;
			break;
		case 'J':
			Iflag = 0;
			Jflag = 1;
			replstr = optarg;
			break;
		case 'L':
			Lflag = atoi(optarg);
			if (COMPAT_MODE("bin/xargs", "Unix2003")) {
				nflag = 0; /* Override */
				nargs = 5000;
			}
			break;
		case 'n':
			nflag = 1;
			if ((nargs = strtol(optarg, NULL, 10)) <= 0)
				errx(1, "illegal argument count");
			if (COMPAT_MODE("bin/xargs", "Unix2003")) {
				Lflag = 0; /* Override */
			}
			break;
		case 'o':
			oflag = 1;
			break;
		case 'P':
			if ((maxprocs = atoi(optarg)) <= 0)
				errx(1, "max. processes must be >0");
			break;
		case 'p':
			pflag = 1;
			break;
		case 'R':
			Rflag = strtol(optarg, &endptr, 10);
			if (*endptr != '\0')
				errx(1, "replacements must be a number");
			break;
		case 's':
			nline = atoi(optarg);
			pad9314053 = 0; /* assume the -s value is valid */
			break;
		case 't':
			tflag = 1;
			break;
		case 'x':
			xflag = 1;
			break;
		case '0':
			zflag = 1;
			break;
		case '?':
		default:
			usage();
	}
	argc -= optind;
	argv += optind;

	if (!Iflag && Rflag)
		usage();
	if (Iflag && !Rflag)
		Rflag = 5;
	if (xflag && !nflag)
		usage();
	if (Iflag || Lflag)
		xflag = 1;
	if (replstr != NULL && *replstr == '\0')
		errx(1, "replstr may not be empty");

	/*
	 * Allocate pointers for the utility name, the utility arguments,
	 * the maximum arguments to be read from stdin and the trailing
	 * NULL.
	 */
	linelen = 1 + argc + nargs + 1;
	if ((av = bxp = malloc(linelen * sizeof(char **))) == NULL)
		errx(1, "malloc failed");

	/*
	 * Use the user's name for the utility as argv[0], just like the
	 * shell.  Echo is the default.  Set up pointers for the user's
	 * arguments.
	 */
	if (*argv == NULL)
		cnt = strlen(*bxp++ = echo) + pad9314053;
	else {
		do {
			if (Jflag && strcmp(*argv, replstr) == 0) {
				char **avj;
				jfound = 1;
				argv++;
				for (avj = argv; *avj; avj++)
					cnt += strlen(*avj) + 1 + pad9314053;
				break;
			}
			cnt += strlen(*bxp++ = *argv) + 1 + pad9314053;
		} while (*++argv != NULL);
	}

	/*
	 * Set up begin/end/traversing pointers into the array.  The -n
	 * count doesn't include the trailing NULL pointer, so the malloc
	 * added in an extra slot.
	 */
	endxp = (xp = bxp) + nargs;

	/*
	 * Allocate buffer space for the arguments read from stdin and the
	 * trailing NULL.  Buffer space is defined as the default or specified
	 * space, minus the length of the utility name and arguments.  Set up
	 * begin/end/traversing pointers into the array.  The -s count does
	 * include the trailing NULL, so the malloc didn't add in an extra
	 * slot.
	 */
	nline -= cnt;
	if (nline <= 0)
		errx(1, "insufficient space for command");

	if ((bbp = malloc((size_t)(nline + 1))) == NULL)
		errx(1, "malloc failed");
	ebp = (argp = p = bbp) + nline - 1;
	for (;;)
		parse_input(argc, argv);
}

static void
parse_input(int argc, char *argv[])
{
	int ch, foundeof;
	char **avj;
	int last_was_backslashed = 0;

	foundeof = 0;

	switch(ch = getchar()) {
	case EOF:
		/* No arguments since last exec. */
		if (p == bbp) {
			waitchildren(*argv, 1);
			exit(rval);
		}
		goto arg1;
	case ' ':
		last_was_blank = 1;
	case '\t':
		/* Quotes escape tabs and spaces. */
		if (insingle || indouble || zflag)
			goto addch;
		goto arg2;
	case '\0':
		if (zflag) {
			/*
			 * Increment 'count', so that nulls will be treated
			 * as end-of-line, as well as end-of-argument.  This
			 * is needed so -0 works properly with -I and -L.
			 */
			count++;
			goto arg2;
		}
		goto addch;
	case '\n':
		if (zflag)
			goto addch;
		if (COMPAT_MODE("bin/xargs", "Unix2003")) {
			if (last_was_newline) {
				/* don't count empty line */
				break;
			}
			if (!last_was_blank ) {
				/* only count if NOT continuation line */
				count++;
			} 
		} else {
			count++;
		}
		last_was_newline = 1;

		/* Quotes do not escape newlines. */
arg1:		if (insingle || indouble)
			errx(1, "unterminated quote");
arg2:
		foundeof = *eofstr != '\0' &&
		    strcmp(argp, eofstr) == 0;

#ifdef __APPLE__
		/* 6591323: -I specifies that it processes the entire line,
		 * so only recognize eofstr at the end of a line. */
		if (Iflag && !last_was_newline)
			foundeof = 0;

		/* 6591323: Essentially the same as the EOF handling above. */
		if (foundeof && (p - strlen(eofstr) == bbp)) {
			waitchildren(*argv, 1);
			exit(rval);
		}
#endif

		/* Do not make empty args unless they are quoted */
		if ((argp != p || wasquoted) && !foundeof) {
			*p++ = '\0';
			*xp++ = argp;
			if (Iflag) {
				size_t curlen;

				if (inpline == NULL)
					curlen = 0;
				else {
					/*
					 * If this string is not zero
					 * length, append a space for
					 * separation before the next
					 * argument.
					 */
					if ((curlen = strlen(inpline)))
						strcat(inpline, " ");
				}
				curlen++;
				/*
				 * Allocate enough to hold what we will
				 * be holding in a second, and to append
				 * a space next time through, if we have
				 * to.
				 */
				inpline = realloc(inpline, curlen + 2 +
				    strlen(argp));
				if (inpline == NULL)
					errx(1, "realloc failed");
				if (curlen == 1)
					strcpy(inpline, argp);
				else
					strcat(inpline, argp);
			}
		}

		/*
		 * If max'd out on args or buffer, or reached EOF,
		 * run the command.  If xflag and max'd out on buffer
		 * but not on args, object.  Having reached the limit
		 * of input lines, as specified by -L is the same as
		 * maxing out on arguments.
		 */
		if (xp == endxp || p + (count * pad9314053) > ebp || ch == EOF ||
		    (Lflag <= count && xflag) || foundeof) {
			if (xflag && xp != endxp && p + (count * pad9314053) > ebp)
				errx(1, "insufficient space for arguments");
			if (jfound) {
				for (avj = argv; *avj; avj++)
					*xp++ = *avj;
			}
			prerun(argc, av);
			if (ch == EOF || foundeof) {
				waitchildren(*argv, 1);
				exit(rval);
			}
			p = bbp;
			xp = bxp;
			count = 0;
		}
		argp = p;
		wasquoted = 0;
		break;
	case '\'':
		if (indouble || zflag)
			goto addch;
		insingle = !insingle;
		wasquoted = 1;
		break;
	case '"':
		if (insingle || zflag)
			goto addch;
		indouble = !indouble;
		wasquoted = 1;
		break;
	case '\\':
		last_was_backslashed = 1;
		if (zflag)
			goto addch;
		/* Backslash escapes anything, is escaped by quotes. */
		if (!insingle && !indouble && (ch = getchar()) == EOF)
			errx(1, "backslash at EOF");
		/* FALLTHROUGH */
	default:
addch:		if (p < ebp) {
			*p++ = ch;
			break;
		}

		/* If only one argument, not enough buffer space. */
		if (bxp == xp)
			errx(1, "insufficient space for argument");
		/* Didn't hit argument limit, so if xflag object. */
		if (xflag)
			errx(1, "insufficient space for arguments");

		if (jfound) {
			for (avj = argv; *avj; avj++)
				*xp++ = *avj;
		}
		prerun(argc, av);
		xp = bxp;
		cnt = ebp - argp;
		memcpy(bbp, argp, (size_t)cnt);
		p = (argp = bbp) + cnt;
		*p++ = ch;
		break;
	}
	if (ch != ' ')
		last_was_blank = 0;
	if (ch != '\n' || last_was_backslashed)
		last_was_newline = 0;
}

/*
 * Do things necessary before run()'ing, such as -I substitution,
 * and then call run().
 */
static void
prerun(int argc, char *argv[])
{
	char **tmp, **tmp2, **avj;
	int repls;

	repls = Rflag;

	if (argc == 0 || repls == 0) {
		*xp = NULL;
		run(argv);
		return;
	}

	avj = argv;

	/*
	 * Allocate memory to hold the argument list, and
	 * a NULL at the tail.
	 */
	tmp = malloc((argc + 1) * sizeof(char**));
	if (tmp == NULL)
		errx(1, "malloc failed");
	tmp2 = tmp;

	/*
	 * Save the first argument and iterate over it, we
	 * cannot do strnsubst() to it.
	 */
	if ((*tmp++ = strdup(*avj++)) == NULL)
		errx(1, "strdup failed");

	/*
	 * For each argument to utility, if we have not used up
	 * the number of replacements we are allowed to do, and
	 * if the argument contains at least one occurrence of
	 * replstr, call strnsubst(), else just save the string.
	 * Iterations over elements of avj and tmp are done
	 * where appropriate.
	 */
	while (--argc) {
		*tmp = *avj++;
		if (repls && strstr(*tmp, replstr) != NULL) {
			strnsubst(tmp++, replstr, inpline, (size_t)255);
			if (repls > 0)
				repls--;
		} else {
			if ((*tmp = strdup(*tmp)) == NULL)
				errx(1, "strdup failed");
			tmp++;
		}
	}

	/*
	 * Run it.
	 */
	*tmp = NULL;
	run(tmp2);

	/*
	 * Walk from the tail to the head, free along the way.
	 */
	for (; tmp2 != tmp; tmp--)
		free(*tmp);
	/*
	 * Now free the list itself.
	 */
	free(tmp2);

	/*
	 * Free the input line buffer, if we have one.
	 */
	if (inpline != NULL) {
		free(inpline);
		inpline = NULL;
	}
}

static void
run(char **argv)
{
	pid_t pid;
	int fd;
	char **avec;

	/*
	 * If the user wants to be notified of each command before it is
	 * executed, notify them.  If they want the notification to be
	 * followed by a prompt, then prompt them.
	 */
	if (tflag || pflag) {
		(void)fprintf(thread_stderr, "%s", *argv);
		for (avec = argv + 1; *avec != NULL; ++avec)
			(void)fprintf(thread_stderr, " %s", *avec);
		/*
		 * If the user has asked to be prompted, do so.
		 */
		if (pflag)
			/*
			 * If they asked not to exec, return without execution
			 * but if they asked to, go to the execution.  If we
			 * could not open their tty, break the switch and drop
			 * back to -t behaviour.
			 */
			switch (prompt()) {
			case 0:
				return;
			case 1:
				goto exec;
			case 2:
				break;
			}
		(void)fprintf(thread_stderr, "\n");
		(void)fflush(stderr);
	}
exec:
	childerr = 0;
	switch(pid = ios_fork()) {
	case -1:
		err(1, "vfork");
    default:   // ios_system: go through both branches
	// case 0:
		if (oflag) {
#if !TARGET_OS_IPHONE && !TARGET_OS_WATCH && !TARGET_OS_TV && !TARGET_OS_MACCATALYST
			if ((fd = open(_PATH_TTY, O_RDONLY)) == -1)
#else
            if ((fd = ios_getstdin()) == -1) 
#endif
            err(1, "can't open /dev/tty");
		} else {
			fd = open(_PATH_DEVNULL, O_RDONLY);
		}
		if (fd > STDIN_FILENO) {
			if (dup2(fd, STDIN_FILENO) != 0)
				err(1, "can't dup2 to stdin");
#if !TARGET_OS_IPHONE && !TARGET_OS_WATCH && !TARGET_OS_TV && !TARGET_OS_MACCATALYST
            close(fd);
#endif
		}
#if !TARGET_OS_IPHONE && !TARGET_OS_WATCH && !TARGET_OS_TV && !TARGET_OS_MACCATALYST
		execvp(argv[0], argv);
		childerr = errno;
		// _exit(1);
#else
        childerr = execvp(argv[0], argv);
#endif
	}
	curprocs++;
    // Now, we need to wait until the child process has finished. 
#if !TARGET_OS_IPHONE && !TARGET_OS_WATCH && !TARGET_OS_TV && !TARGET_OS_MACCATALYST
	waitchildren(*argv, 0);
#else
    // iOS specifics: wait for each process in turn.
    int status;
    waitpid(pid, &status, 0); // wait for this process to finish (iOS)
    curprocs--;
    if (childerr != 0) {
        errno = childerr;
        err(errno == ENOENT ? 127 : 126, "%s", *argv);
    }
    if (WIFSIGNALED(status) || WEXITSTATUS(status) == 255)
        exit(1);
    if (WEXITSTATUS(status))
        rval = 1;
#endif
}

// wait for all children of current process (Unix default).
// Won't work with the current version of ios_system.
static void
waitchildren(const char *name, int waitall)
{
	pid_t pid;
	int status;

#if TARGET_OS_IPHONE || TARGET_OS_WATCH || TARGET_OS_TV || TARGET_OS_MACCATALYST
    return;
#endif
	while ((pid = waitpid(-1, &status, !waitall && curprocs < maxprocs ?
	    WNOHANG : 0)) > 0) {
		curprocs--;
		/* If we couldn't invoke the utility, exit. */
		if (childerr != 0) {
			errno = childerr;
			err(errno == ENOENT ? 127 : 126, "%s", name);
		}
		/*
		 * If utility signaled or exited with a value of 255,
		 * exit 1-125.
		 */
		if (WIFSIGNALED(status) || WEXITSTATUS(status) == 255)
			exit(1);
		if (WEXITSTATUS(status))
			rval = 1;
	}
#if !defined(TARGET_OS_IPHONE) && !defined(TARGET_OS_WATCH) && !defined(TARGET_OS_TV) && !defined(TARGET_OS_MACCATALYST)
	if (pid == -1 && errno != ECHILD)
		err(1, "wait3");
#endif
}

/*
 * Prompt the user about running a command.
 */
// iOS: this flag won't work on iOS system for now.
// We need two different sources of input (stdin and ttyfp)
// and we only have one. Same problem as "more" and "less".
static int
prompt(void)
{
	regex_t cre;
	size_t rsize;
	int match;
	char *response;
	FILE *ttyfp;

	if ((ttyfp = fopen(_PATH_TTY, "r")) == NULL)
		return (2);	/* Indicate that the TTY failed to open. */
	(void)fprintf(thread_stderr, "?...");
	(void)fflush(stderr);
	if ((response = fgetln(ttyfp, &rsize)) == NULL ||
	    regcomp(&cre, nl_langinfo(YESEXPR), REG_BASIC) != 0) {
		(void)fclose(ttyfp);
		return (0);
	}
	match = regexec(&cre, response, 0, NULL, 0);
	(void)fclose(ttyfp);
	regfree(&cre);
	return (match == 0);
}

static void
usage(void)
{
	fprintf(thread_stderr,
"usage: xargs [-0opt] [-E eofstr] [-I replstr [-R replacements]] [-J replstr]\n"
"             [-L number] [-n number [-x]] [-P maxprocs] [-s size]\n"
"             [utility [argument ...]]\n");
	exit(1);
}
