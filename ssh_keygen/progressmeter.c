/* $OpenBSD: progressmeter.c,v 1.50 2020/01/23 07:10:22 dtucker Exp $ */
/*
 * Copyright (c) 2003 Nils Nordman.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "progressmeter.h"
#include "atomicio.h"
#include "misc.h"
#include "utf8.h"
#if TARGET_OS_IPHONE || TARGET_OS_WATCH || TARGET_OS_TV || TARGET_OS_MACCATALYST
#include <stdlib.h>
#endif

#define DEFAULT_WINSIZE 80
#define MAX_WINSIZE 512
#define PADDING 1		/* padding between the progress indicators */
#define UPDATE_INTERVAL 1	/* update the progress meter every second */
#define STALL_TIME 5		/* we're stalled after this many seconds */

/* determines whether we can output to the terminal */
static int can_output(void);

/* formats and inserts the specified size into the given buffer */
static void format_size(char *, int, off_t);
static void format_rate(char *, int, off_t);

/* window resizing */
static void sig_winch(int);
static void setscreensize(void);

/* signal handler for updating the progress meter */
static void sig_alarm(int);

static __thread double start;		/* start progress */
static __thread double last_update;	/* last progress update */
static __thread const char *file;	/* name of the file being transferred */
static __thread off_t start_pos;		/* initial position of transfer */
static __thread off_t end_pos;		/* ending position of transfer */
static __thread off_t cur_pos;		/* transfer position as of last refresh */
static __thread volatile off_t *counter;	/* progress counter */
static __thread long stalled;		/* how long we have been stalled */
static __thread int bytes_per_second;	/* current speed in bytes per second */
static __thread int win_size;		/* terminal window size */
static __thread volatile sig_atomic_t win_resized; /* for window resizing */
static __thread volatile sig_atomic_t alarm_fired;

/* units for format_size */
static const char unit[] = " KMGT";

static int
can_output(void)
{
#if !TARGET_OS_IPHONE && !TARGET_OS_WATCH && !TARGET_OS_TV && !TARGET_OS_MACCATALYST
    return (getpgrp() == tcgetpgrp(STDOUT_FILENO));
#else
    return (ios_isatty(STDOUT_FILENO));
#endif
}

static void
format_rate(char *buf, int size, off_t bytes)
{
	int i;

	bytes *= 100;
	for (i = 0; bytes >= 100*1000 && unit[i] != 'T'; i++)
		bytes = (bytes + 512) / 1024;
	if (i == 0) {
		i++;
		bytes = (bytes + 512) / 1024;
	}
	snprintf(buf, size, "%3lld.%1lld%c%s",
	    (long long) (bytes + 5) / 100,
	    (long long) (bytes + 5) / 10 % 10,
	    unit[i],
	    i ? "B" : " ");
}

static void
format_size(char *buf, int size, off_t bytes)
{
	int i;

	for (i = 0; bytes >= 10000 && unit[i] != 'T'; i++)
		bytes = (bytes + 512) / 1024;
	snprintf(buf, size, "%4lld%c%s",
	    (long long) bytes,
	    unit[i],
	    i ? "B" : " ");
}

void
refresh_progress_meter(int force_update)
{
	char buf[MAX_WINSIZE + 1];
	off_t transferred;
	double elapsed, now;
	int percent;
	off_t bytes_left;
	int cur_speed;
	int hours, minutes, seconds;
	int file_len;

#if TARGET_OS_IPHONE || TARGET_OS_WATCH || TARGET_OS_TV || TARGET_OS_MACCATALYST
    // Not initialized:
    if ((counter == NULL) || (file == NULL))
        return;
    // alarm() is not working. We emulate it by looking at the time:
    now = monotime_double();
    if (now - last_update > UPDATE_INTERVAL)
        alarm_fired = 1;
#endif
	if ((!force_update && !alarm_fired && !win_resized) || !can_output())
		return;
    
	alarm_fired = 0;

	if (win_resized) {
		setscreensize();
		win_resized = 0;
	}

	transferred = *counter - (cur_pos ? cur_pos : start_pos);
	cur_pos = *counter;
#if !TARGET_OS_IPHONE && !TARGET_OS_WATCH && !TARGET_OS_TV && !TARGET_OS_MACCATALYST
    now = monotime_double();
#endif
	bytes_left = end_pos - cur_pos;

	if (bytes_left > 0)
		elapsed = now - last_update;
	else {
		elapsed = now - start;
		/* Calculate true total speed when done */
		transferred = end_pos - start_pos;
		bytes_per_second = 0;
	}

	/* calculate speed */
	if (elapsed != 0)
		cur_speed = (transferred / elapsed);
	else
		cur_speed = transferred;

#define AGE_FACTOR 0.9
	if (bytes_per_second != 0) {
		bytes_per_second = (bytes_per_second * AGE_FACTOR) +
		    (cur_speed * (1.0 - AGE_FACTOR));
	} else
		bytes_per_second = cur_speed;

	/* filename */
	buf[0] = '\0';
	file_len = win_size - 36;
	if (file_len > 0) {
		buf[0] = '\r';
		snmprintf(buf+1, sizeof(buf)-1, &file_len, "%-*s",
		    file_len, file);
	}

	/* percent of transfer done */
	if (end_pos == 0 || cur_pos == end_pos)
		percent = 100;
	else
		percent = ((float)cur_pos / end_pos) * 100;
	snprintf(buf + strlen(buf), win_size - strlen(buf),
	    " %3d%% ", percent);

	/* amount transferred */
	format_size(buf + strlen(buf), win_size - strlen(buf),
	    cur_pos);
	strlcat(buf, " ", win_size);

	/* bandwidth usage */
	format_rate(buf + strlen(buf), win_size - strlen(buf),
	    (off_t)bytes_per_second);
	strlcat(buf, "/s ", win_size);

	/* ETA */
	if (!transferred)
		stalled += elapsed;
	else
		stalled = 0;

	if (stalled >= STALL_TIME)
		strlcat(buf, "- stalled -", win_size);
	else if (bytes_per_second == 0 && bytes_left)
		strlcat(buf, "  --:-- ETA", win_size);
	else {
		if (bytes_left > 0)
			seconds = bytes_left / bytes_per_second;
		else
			seconds = elapsed;

		hours = seconds / 3600;
		seconds -= hours * 3600;
		minutes = seconds / 60;
		seconds -= minutes * 60;

		if (hours != 0)
			snprintf(buf + strlen(buf), win_size - strlen(buf),
			    "%d:%02d:%02d", hours, minutes, seconds);
		else
			snprintf(buf + strlen(buf), win_size - strlen(buf),
			    "  %02d:%02d", minutes, seconds);

		if (bytes_left > 0)
			strlcat(buf, " ETA", win_size);
		else
			strlcat(buf, "    ", win_size);
	}

	atomicio(vwrite, STDOUT_FILENO, buf, win_size - 1);
    last_update = now;
}

/*ARGSUSED*/
static void
sig_alarm(int ignore)
{
	alarm_fired = 1;
	alarm(UPDATE_INTERVAL);
}

void
start_progress_meter(const char *f, off_t filesize, off_t *ctr)
{
	start = last_update = monotime_double();
	file = f;
	start_pos = *ctr;
	end_pos = filesize;
	cur_pos = 0;
	counter = ctr;
	stalled = 0;
	bytes_per_second = 0;

	setscreensize();
	refresh_progress_meter(1);

	ssh_signal(SIGALRM, sig_alarm);
	ssh_signal(SIGWINCH, sig_winch);
	alarm(UPDATE_INTERVAL);
}

void
stop_progress_meter(void)
{
	alarm(0);

	if (!can_output())
		return;

	/* Ensure we complete the progress */
	if (cur_pos != end_pos)
		refresh_progress_meter(1);

	atomicio(vwrite, STDOUT_FILENO, "\n", 1);
    counter = NULL;
    file = NULL;
}

/*ARGSUSED*/
static void
sig_winch(int sig)
{
	win_resized = 1;
}

static void
setscreensize(void)
{
	struct winsize winsize;

#if !TARGET_OS_IPHONE && !TARGET_OS_WATCH && !TARGET_OS_TV && !TARGET_OS_MACCATALYST
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsize) != -1 &&
        winsize.ws_col != 0) {
        if (winsize.ws_col > MAX_WINSIZE)
            win_size = MAX_WINSIZE;
        else
            win_size = winsize.ws_col;
    } else
        win_size = DEFAULT_WINSIZE;
#else
    // Since we don't get window size from ioctl, we set it ourselves:
    win_size = atoi(getenv("COLUMNS"));
#endif
	win_size += 1;					/* trailing \0 */
}
