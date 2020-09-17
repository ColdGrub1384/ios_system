//
//  libc_replacement.c
//  ios_system
//
//  Created by Nicolas Holzschuch on 30/04/2018.
//  Copyright © 2018 Nicolas Holzschuch. All rights reserved.
//
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>

#include "ios_error.h"
#undef write
#undef fwrite
#undef puts
#undef fputs
#undef fputc
#undef putw
#undef fflush

int printf (const char *format, ...) {
    va_list arg;
    int done;
    
    va_start (arg, format);
    done = vfprintf (thread_stdout, format, arg);
    va_end (arg);
    
    return done;
}
int fprintf(FILE * restrict stream, const char * restrict format, ...) {
    va_list arg;
    int done;
    if (thread_stderr == NULL) thread_stderr = stderr;
    if (thread_stdout == NULL) thread_stdout = stdout;

    va_start (arg, format);
    if (fileno(stream) == STDOUT_FILENO) done = vfprintf (thread_stdout, format, arg);
    else if (fileno(stream) == STDERR_FILENO) done = vfprintf (thread_stderr, format, arg);
    // iOS, debug:
    // else if (fileno(stream) == STDERR_FILENO) done = vfprintf (stderr, format, arg);
    else done = vfprintf (stream, format, arg);
    va_end (arg);
    
    return done;
}
int scanf (const char *format, ...) {
    int             count;
    va_list ap;
    
    if (thread_stderr == NULL) thread_stderr = stderr;
    if (thread_stdout == NULL) thread_stdout = stdout;
    if (thread_stdin == NULL) thread_stdin = stdin;

    fflush(thread_stdout);
    fflush(thread_stderr);
    va_start (ap, format);
    count = vfscanf (thread_stdin, format, ap);
    va_end (ap);
    return (count);
}
int ios_fflush(FILE *stream) {
    if (thread_stdout == NULL) thread_stdout = stdout;
    if (thread_stderr == NULL) thread_stderr = stderr;

    if (fileno(stream) == STDOUT_FILENO) return fflush(thread_stdout);
    if (fileno(stream) == STDERR_FILENO) return fflush(thread_stderr);
    return fflush(stream);
}
ssize_t ios_write(int fildes, const void *buf, size_t nbyte) {
    if (thread_stdout == NULL) thread_stdout = stdout;
    if (thread_stderr == NULL) thread_stderr = stderr;
    if (fildes == STDOUT_FILENO) return write(fileno(thread_stdout), buf, nbyte);
    if (fildes == STDERR_FILENO) return write(fileno(thread_stderr), buf, nbyte);
    return write(fildes, buf, nbyte);
}
size_t ios_fwrite(const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream) {
    if (thread_stdout == NULL) thread_stdout = stdout;
    if (thread_stderr == NULL) thread_stderr = stderr;
    if (fileno(stream) == STDOUT_FILENO) return fwrite(ptr, size, nitems, thread_stdout);
    if (fileno(stream) == STDERR_FILENO) return fwrite(ptr, size, nitems, thread_stderr);
    return fwrite(ptr, size, nitems, stream);
}
int ios_puts(const char *s) {
    if (thread_stdout == NULL) thread_stdout = stdout;
    // puts adds a newline at the end.
    int returnValue = fputs(s, thread_stdout);
    fputc('\n', thread_stdout);
    return returnValue;
}
int ios_fputs(const char* s, FILE *stream) {
    if (thread_stdout == NULL) thread_stdout = stdout;
    if (thread_stderr == NULL) thread_stderr = stderr;
    if (fileno(stream) == STDOUT_FILENO) return fputs(s, thread_stdout);
    if (fileno(stream) == STDERR_FILENO) return fputs(s, thread_stderr);
    return fputs(s, stream);
}
int ios_fputc(int c, FILE *stream) {
    if (thread_stdout == NULL) thread_stdout = stdout;
    if (thread_stderr == NULL) thread_stderr = stderr;
    if (fileno(stream) == STDOUT_FILENO) return fputc(c, thread_stdout);
    if (fileno(stream) == STDERR_FILENO) return fputc(c, thread_stderr);
    return fputc(c, stream);
}

#include <assert.h>

int ios_putw(int w, FILE *stream) {
    if (thread_stdout == NULL) thread_stdout = stdout;
    if (thread_stderr == NULL) thread_stderr = stderr;
    if (fileno(stream) == STDOUT_FILENO) return putw(w, thread_stdout);
    if (fileno(stream) == STDERR_FILENO) return putw(w, thread_stderr);
    return putw(w, stream);
}

// Fake process IDs to go with fake forking:
// You will still need to edit your code to make sure you go through both branches.
#define IOS_MAX_THREADS 128
static pthread_t thread_ids[IOS_MAX_THREADS];
static char** environment[IOS_MAX_THREADS];
static int pid_overflow = 0;
static pid_t current_pid = 0;

inline pthread_t ios_getThreadId(pid_t pid) {
    // return ios_getLastThreadId(); // previous behaviour
    return thread_ids[pid];
}

// We do not recycle process ids too quickly to avoid collisions.

static inline const pid_t ios_nextAvailablePid() {
    if (!pid_overflow && (current_pid < IOS_MAX_THREADS - 1)) {
        current_pid += 1;
        thread_ids[current_pid] = -1; // Not yet started
        return current_pid;
    }
    // We've already started more than IOS_MAX_THREADS threads.
    if (!pid_overflow) current_pid = 0; // first time over the limit
    pid_overflow = 1;
    while (1) {
        current_pid += 1;
        if (current_pid >= IOS_MAX_THREADS) current_pid = 1;
        pthread_t thread_pid = ios_getThreadId(current_pid);
        if (thread_pid == 0) {
            return current_pid; // already released
        }
        // Dangerous: if the process is already killed, this wil crash
        /*
        if (pthread_kill(thread_pid, 0) != 0) {
            thread_ids[current_pid] = 0;
            return current_pid; // not running anymore
        }
        */
    }
}

inline void ios_storeThreadId(pthread_t thread) {
    // To avoid issues when a command starts a command without forking,
    // we only store thread IDs for the first thread of the "process".
    // fprintf(stderr, "Storing thread %x to pid %d current value: %x\n", thread, current_pid, thread_ids[current_pid]);
    if (thread_ids[current_pid] == -1) {
        thread_ids[current_pid] = thread;
        return;
    }
    // The fuck is this line doing here?
    // if (pthread_kill(ios_getThreadId(current_pid), 0) != 0) thread_ids[current_pid] = thread;
}


// store extra environment variables (called from execve)
// Only store the variables that are not already in the environment.
char* libc_getenv(char* variableName) {
    if (environment[current_pid] != NULL) {
        char** envp = environment[current_pid];
        int i = 0;
        while (envp[i] != NULL) {
            char* position = strstr(envp[i],"=");
            if (strncmp(variableName, envp[i], position - envp[i]) == 0) {
                char* value = position + 1;
                return value;
            }
            i++;
        }
    }
    return getenv(variableName);
}

extern char** environ;
void storeEnvironment(char* envp[]) {
    environment[current_pid] = envp;
    /*
    int i = 0;
    while ((envp[i] != NULL) && (environ[i] != NULL)) {
        if (strcmp(envp[i], environ[i]) != 0) {
            break;
        }
        i++;
    }
    // We can have new values for old variables, new variables, etc
    // Need to store variables that are going to be set and their prior values.
    int newVariableStart = i;
    int numVariablesSet = 0;
    char** previousValues = NULL;
    char** variableNames = NULL;
    // Sometimes, envp == environ. In that case, no need to to anything specific:
    if (envp[newVariableStart] != NULL) {
        while (envp[newVariableStart + numVariablesSet] != NULL) {
            numVariablesSet++;
        }
        previousValues = malloc(numVariablesSet * sizeof(char*));
        variableNames = malloc(numVariablesSet * sizeof(char*));
        // Set up the new environment variables, store previous value for restore:
        for (int i = 0; i < numVariablesSet; i++) {
            variableNames[i] = strdup(envp[newVariableStart + i]);
            // fprintf(stderr, "Setting new variable: %s\n", variableNames[i]); fflush(stderr);
            char* position = strstr(variableNames[i],"=");
            char* value = position + 1;
            *position = 0;
            char* previousValue = getenv(variableNames[i]);
            if (previousValue == NULL)
                previousValues[i] = NULL;
            else
                previousValues[i] = strdup(previousValue);
            // fprintf(stderr, "Setting %s to %s\n", variableNames[i], value); fflush(stderr);
            int failure = setenv(variableNames[i], value, 1);
            if (failure) {
                fprintf(stderr, "Could not set variable %s: %s\n", variableNames[i], strerror(errno));
            }
        }
    }
     */
}

// when the command is terminated, release the environment variables that were added.
void resetEnvironment(pid_t pid) {
    environment[pid] = NULL;
/*
    if (numVariablesSet > 0) {
        for (int i = 0; i < numVariablesSet; i++) {
            if (strlen(variableNames[i]) == 0)
                continue;
            if (previousValues[i] == NULL)
                unsetenv(variableNames[i]);
            else
                setenv(variableNames[i], previousValues[i], 1);
        }
        // Free the variables allocated:
        for (int i = 0; i < numVariablesSet; i++) {
            if (previousValues[i] != NULL)
                free(previousValues[i]);
            free(variableNames[i]);
        }
        free(previousValues);
        free(variableNames);
    }
*/
}

void ios_releaseThread(pthread_t thread) {
    // TODO: this is inefficient. Replace with NSMutableArray?
    // fprintf(stderr, "Releasing thread %x\n", thread);
    for (int p = 0; p < IOS_MAX_THREADS; p++) {
        if (thread_ids[p] == thread) {
            // fprintf(stderr, "Found Id %x\n", p);
            resetEnvironment(p);
            thread_ids[p] = 0;
            return;
        }
    }
    // fprintf(stderr, "Not found\n");
}


void ios_releaseThreadId(pid_t pid) {
    resetEnvironment(pid);
    thread_ids[pid] = 0;
}

pid_t ios_currentPid() {
    return current_pid;
}

pid_t fork(void) { return ios_nextAvailablePid(); } // increases current_pid by 1.
pid_t ios_fork(void) { return ios_nextAvailablePid(); } // increases current_pid by 1.
pid_t vfork(void) { return ios_nextAvailablePid(); }

// simple replacement of waitpid for swift programs
// We use "optnone" to prevent optimization, otherwise the while loops never end.
__attribute__ ((optnone)) void ios_waitpid(pid_t pid) {
    pthread_t threadToWaitFor;
    // Old system: no explicit pid, just store last thread Id.
    if ((pid == -1) || (pid == 0)) {
        threadToWaitFor = ios_getLastThreadId();
        while (threadToWaitFor != 0) {
            threadToWaitFor = ios_getLastThreadId();
        }
        return;
    }
    // New system: thread Id is store with pid:
    threadToWaitFor = ios_getThreadId(pid);
    while (threadToWaitFor != 0) {
        // -1: not started, >0 started, not finished, 0: finished
        threadToWaitFor = ios_getThreadId(pid);
    }
    return;
}

__attribute__ ((optnone)) pid_t waitpid(pid_t pid, int *stat_loc, int options) {
    // pthread_join won't work,  because the thread might have been detached.
    // (and you can't re-join a detached thread).
    // -1 = the call waits for any child process (not good yet)
    //  0 = the call waits for any child process in the process group of the caller
    
    if (options && WNOHANG) {
        // WNOHANG: just check that the process is still running:
        pthread_t threadToWaitFor;
        if ((pid == -1) || (pid == 0)) threadToWaitFor = ios_getLastThreadId();
        else threadToWaitFor = ios_getThreadId(pid);
        if (threadToWaitFor != 0) // the process is still running
            return 0;
        else {
            if (stat_loc) *stat_loc = W_EXITCODE(ios_getCommandStatus(), 0);
            fflush(thread_stdout);
            fflush(thread_stderr);
            return pid; // was "-1". See man page and https://github.com/holzschu/ios_system/issues/89
        }
    } else {
        // Wait until the process is terminated:
        ios_waitpid(pid);
        if (stat_loc) *stat_loc = W_EXITCODE(ios_getCommandStatus(), 0);
        return pid;
    }
}



//
void vwarn(const char *fmt, va_list args)
{
    if (thread_stderr == NULL) thread_stderr = stderr;
    fputs(ios_progname(), thread_stderr);
    if (fmt != NULL)
    {
        fputs(": ", thread_stderr);
        vfprintf(thread_stderr, fmt, args);
    }
    fputs(": ", thread_stderr);
    fputs(strerror(errno), thread_stderr);
    putc('\n', thread_stderr);
}

void vwarnx(const char *fmt, va_list args)
{
    if (thread_stderr == NULL) thread_stderr = stderr;
    fputs(ios_progname(), thread_stderr);
    fputs(": ", thread_stderr);
    if (fmt != NULL)
        vfprintf(thread_stderr, fmt, args);
    putc('\n', thread_stderr);
}
// void err(int eval, const char *fmt, ...);
void err(int eval, const char *fmt, ...) {
    va_list argptr;
    va_start(argptr, fmt);
    vwarn(fmt, argptr);
    va_end(argptr);
    ios_exit(eval);
}
//     void errx(int eval, const char *fmt, ...);
void errx(int eval, const char *fmt, ...) {
    va_list argptr;
    va_start(argptr, fmt);
    vwarnx(fmt, argptr);
    va_end(argptr);
    ios_exit(eval);
}
//   void warn(const char *fmt, ...);
void warn(const char *fmt, ...) {
    va_list argptr;
    va_start(argptr, fmt);
    vwarn(fmt, argptr);
    va_end(argptr);
}
//   void warnx(const char *fmt, ...);
void warnx(const char *fmt, ...) {
    va_list argptr;
    va_start(argptr, fmt);
    vwarnx(fmt, argptr);
    va_end(argptr);
}
