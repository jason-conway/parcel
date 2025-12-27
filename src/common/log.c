#include "log.h"

typedef struct log_t {
    void *udata;
    loglvl_t level;
    bool quiet;
    pthread_mutex_t lock;
} log_t;

static log_t log;

static const char *lvls[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

static const char *level_colors[] = {
    "\033[94m", "\033[36m", "\033[32m", "\033[33m", "\033[31m", "\033[35m"
};

static void stdout_callback(logctx_t *ev)
{
    char pre[16];
    pre[strftime(pre, sizeof(pre), "%H:%M:%S", ev->time)] = '\0';
    fprintf(ev->udata, "%s %s%-5s\033[0m \033[90m%s:%d:\033[0m ", pre, level_colors[ev->level], lvls[ev->level], ev->file, ev->line);
    vfprintf(ev->udata, ev->fmt, ev->ap);
    fprintf(ev->udata, "\n");
    fflush(ev->udata);
}

static void init_event(logctx_t *ev, void *udata)
{
    if (!ev->time) {
        time_t t = time(NULL);
        ev->time = localtime(&t);
    }
    ev->udata = udata;
}

static const char *xbasename(const char *path)
{
    const char *s = path;
    for (; *s; s++);
    for (; s > path && s[-1] != '/' && s[-1] != '\\'; s--);
    return s;
}

void log_init(loglvl_t lvl)
{
    pthread_mutex_init(&log.lock, NULL);
    log.level = lvl;
}

void _log(loglvl_t level, const char *file, int32_t line, const char *fmt, ...)
{
    logctx_t ev = {
        .fmt = fmt,
        .file = xbasename(file),
        .line = line,
        .level = level,
    };
    pthread_mutex_lock(&log.lock);
    if (!log.quiet && (int32_t)level >= (int32_t)log.level) {
        init_event(&ev, stderr);
        va_start(ev.ap, fmt);
        stdout_callback(&ev);
        va_end(ev.ap);
    }
    pthread_mutex_unlock(&log.lock);
}
