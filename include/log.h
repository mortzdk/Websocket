#pragma once

/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>

#define LOG_VERSION "0.1.0"

typedef void (*log_LockFn)(void *udata, int lock);

enum {
    WSS_LOG_FATAL,
    WSS_LOG_ERROR,
    WSS_LOG_WARN,
    WSS_LOG_INFO,
    WSS_LOG_DEBUG,
    WSS_LOG_TRACE
};

#define WSS_log_trace(...) log_log(WSS_LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define WSS_log_debug(...) log_log(WSS_LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define WSS_log_info(...)  log_log(WSS_LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define WSS_log_warn(...)  log_log(WSS_LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define WSS_log_error(...) log_log(WSS_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define WSS_log_fatal(...) log_log(WSS_LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

void log_set_udata(void *udata);
void log_set_lock(log_LockFn fn);
void log_set_fp(FILE *fp);
void log_set_level(int level);
void log_set_quiet(int enable);

void log_log(int level, const char *file, int line, const char *fmt, ...);

#endif
