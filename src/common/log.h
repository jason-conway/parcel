#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <pthread.h>

typedef enum loglvl_t {
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
} loglvl_t;

typedef struct logctx_t {
    va_list ap;
    const char *fmt;
    const char *file;
    struct tm *time;
    void *udata;
    int32_t line;
    loglvl_t level;
} logctx_t;

// Initialize the logger mutex and set starting loglevel
void log_init(loglvl_t lvl);

// Change the current loglevel
// Called after `log_init(...)`
void log_set_loglvl(loglvl_t lvl);

void _log(loglvl_t level, const char *file, int32_t line, const char *fmt, ...);

/**
 * @brief Log a message with TRACE level
 * @param ... Printf-style format string followed by arguments
 */
#define log_trace(...) _log(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)

/**
 * @brief Log a message with DEBUG level
 * @param ... Printf-style format string followed by arguments
 */
#define log_debug(...) _log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)

/**
 * @brief Log a message with INFO level
 * @param ... Printf-style format string followed by arguments
 */
#define log_info(...)  _log(LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)

/**
 * @brief Log a message with WARNING level
 * @param ... Printf-style format string followed by arguments
 */
#define log_warn(...)  _log(LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)

/**
 * @brief Log a message with ERROR level
 * @param ... Printf-style format string followed by arguments
 */
#define log_error(...) _log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

/**
 * @brief Log a message with FATAL level
 * @param ... Printf-style format string followed by arguments
 */
#define log_fatal(...) _log(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)
