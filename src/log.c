/*
 * Copyright (c) 2017 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>

#include "log.h"
#include "core.h"

static struct {
    void *udata;
    log_LockFn lock;
    FILE *fp;
    int level;
    int quiet;
} L;

static int year;
static int month;
static int day;
static int hour;
static int minutes;
static int seconds;
static int usec;

static struct timeval tv;
static struct tm *tm;

static const char *level_names[] = {
    "FATAL",
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG",
    "TRACE"
};

static const char *level_colors[] = {
    "\x1b[35m",
    "\x1b[31m",
    "\x1b[33m",
    "\x1b[32m",
    "\x1b[36m",
    "\x1b[94m"
};

static inline void time_to_str(char *buf) {
    gettimeofday(&tv, NULL);
    tm = localtime(&tv.tv_sec);
    /* Add 1900 to get the right year value read the manual page for localtime() */
    year    = tm->tm_year + 1900;
    /* Months are 0 indexed in struct tm */
    month   = tm->tm_mon + 1;
    day     = tm->tm_mday;
    hour    = tm->tm_hour;
    minutes = tm->tm_min;
    seconds = tm->tm_sec;
    usec    = tv.tv_usec;
    // buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt)] = '\0';
    int len = sprintf(buf, 
            "%04d-%02d-%02d %02d:%02d:%02d.%06d ",
            year,
            month,
            day,
            hour,
            minutes,
            seconds,
            usec);
    buf[len] = '\0';
}

static inline void lock(void) {
    if ( likely(L.lock) ) {
        L.lock(L.udata, 1);
    }
}

static inline void unlock(void) {
    if ( likely(L.lock) ) {
        L.lock(L.udata, 0);
    }
}

void log_set_udata(void *udata) {
    L.udata = udata;
}

void log_set_lock(log_LockFn fn) {
    L.lock = fn;
}

void log_set_fp(FILE *fp) {
    L.fp = fp;
}

void log_set_level(int level) {
    L.level = level;
}

void log_set_quiet(int enable) {
    L.quiet = enable ? 1 : 0;
}

void log_log(int level, const char *file, int line, const char *fmt, ...) {
    if ( likely(level > L.level) ) {
        return;
    } else if ( unlikely(L.quiet && !L.fp) ) {
        return;
    }

    char time_string[100];

    /* Acquire lock */
    lock();

    time_to_str(time_string);

    /* Log to stderr */
    if ( unlikely(!L.quiet) ) {
        va_list args;

        fprintf(
                stderr, "[%lu] %s %s %-5s\x1b[0m \x1b[90m%s:%d:\x1b[0m ",
                pthread_self(), time_string, level_colors[level], level_names[level], file, line);
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
        fprintf(stderr, "\n");
        fflush(stderr);
    }

    /* Log to file */
    if ( likely(L.fp) ) {
        va_list args;
        fprintf(L.fp, "[%lu] %s %-5s %s:%d: ", pthread_self(), time_string, level_names[level], file, line);
        va_start(args, fmt);
        vfprintf(L.fp, fmt, args);
        va_end(args);
        fprintf(L.fp, "\n");
        fflush(L.fp);
    }

    /* Release lock */
    unlock();
}
