/*-
 * Copyright (c) 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1987, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#if 0
#ifndef lint
static char sccsid[] = "@(#)printenv.c	8.2 (Berkeley) 5/4/95";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <TargetConditionals.h>
#include "ios_error.h"

static void	usage(void);
extern char **environ;

/*
 * printenv
 *
 * Bill Joy, UCB
 * February, 1979
 */
int
printenv_main(int argc, char *argv[])
{
	char *cp, **ep;
	size_t len;
	int ch;
    optind = 1; opterr = 1; optreset = 1;

	while ((ch = getopt(argc, argv, "")) != -1)
		switch(ch) {
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
#if !TARGET_OS_IPHONE && !TARGET_OS_WATCH && !TARGET_OS_TV && !TARGET_OS_MACCATALYST
        for (ep = environ; *ep != NULL; ep++)
#else
        for (ep = environmentVariables(ios_currentPid()); *ep != NULL; ep++)
#endif
			(void)fprintf(thread_stdout, "%s\n", *ep);
		exit(0);
	}
	len = strlen(*argv);
#if !TARGET_OS_IPHONE && !TARGET_OS_WATCH && !TARGET_OS_TV && !TARGET_OS_MACCATALYST
    for (ep = environ; *ep != NULL; ep++)
#else
    for (ep = environmentVariables(ios_currentPid()); *ep != NULL; ep++)
#endif
		if (!memcmp(*ep, *argv, len)) {
			cp = *ep + len;
			if (!*cp || *cp == '=') {
				(void)fprintf(thread_stdout, "%s\n", *cp ? cp + 1 : cp);
				exit(0);
			}
		}
	exit(1);
}

static void
usage(void)
{
	(void)fprintf(thread_stderr, "usage: printenv [name]\n");
	exit(1);
}
