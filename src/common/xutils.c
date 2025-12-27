/**
 * @file xutils.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Shared utility functions
 * @version 0.9.2
 * @date 2022-04-06
 *
 * @copyright Copyright (c) 2022 - 2024 Jason Conway. All rights reserved.
 *
 */

#include "xutils.h"
#include "xplatform.h"
#include "log.h"

char *xstrdup(const char *str)
{
    if (!str) {
        return NULL;
    }
    size_t length = strlen(str) + 1;
    return memcpy(xmalloc(length), str, length);
}

char *_xstrcat(const char **strs, size_t count)
{
    // Allocate the required length
    size_t length = 1;
    for (const char **str = strs; &str[0] < &strs[count]; str++) {
        length += strlen(*str);
    }

    char *s = xmalloc(length);

    // Append each arg to the end of the last
    size_t offset = 0;
    for (const char **str = strs; &str[0] < &strs[count]; str++) {
        size_t len = strlen(*str);
        memcpy(&s[offset], *str, len);
        offset += len;
    }
    s[offset] = '\0';

    return s;
}

void xwarn(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    (void)fprintf(stdout, "%s", "\033[0;33m");
    (void)vfprintf(stdout, format, ap);
    (void)fprintf(stdout, "\033[0m");
    (void)fflush(stdout);
    va_end(ap);
}

void xalert(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    (void)fprintf(stderr, "%s", "\033[0;31m");
    (void)vfprintf(stderr, format, ap);
    (void)fprintf(stderr, "\033[0m");
    (void)fflush(stdout);
    va_end(ap);
}

void xprintf(enum color color, enum style style, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    (void)fprintf(stdout, "\033[%c%c%c%cm", color ? '3' : '\0', color ? color : '\0', style ? ';' : '\0', style ? style : '\0');
    (void)vfprintf(stdout, format, ap);
    (void)fprintf(stdout, "\033[0m");
    (void)fflush(stdout);
    va_end(ap);
}

bool xsendall(sock_t socket, const void *data, size_t len)
{
    const uint8_t *s = data;
    for (size_t i = 0; i < len;) {
        ssize_t bytes_sent = xsend(socket, &s[i], len - i, 0);
        switch (bytes_sent) {
            case -1:
                return false;
            default:
                i += bytes_sent;
        }
    }
    return true;
}

bool xrecvall(sock_t socket, void *data, size_t len)
{
    uint8_t *s = data;
    for (size_t i = 0; i < len;) {
        ssize_t bytes_recv = xrecv(socket, &s[i], len - i, 0);
        switch (bytes_recv) {
            case -1:
                return false;
            default:
                i += bytes_recv;
        }
    }
    return true;
}

bool xgetpeeraddr(sock_t socket, char *address, in_port_t *port)
{
    struct sockaddr_in client_sockaddr;
    socklen_t len = sizeof(client_sockaddr);
    if (xgetpeername(socket, (struct sockaddr *)&client_sockaddr, &len)) {
        return false;
    }
    (void)inet_ntop(AF_INET, &client_sockaddr.sin_addr, address, INET_ADDRSTRLEN);
    *port = ntohs(client_sockaddr.sin_port);
    return true;
}

bool xstrrange(char *arg, long *larg, long min, long max)
{
    long _larg = strtol(arg, NULL, 10);
    if (_larg < min || _larg > max) {
        return false;
    }
    if (larg) {
        *larg = _larg;
    }
    return true;
}

ptrdiff_t xgetopt(xgetopt_t *ctx, int argc, char **argv, const char *optstr)
{
    char *arg = argv[!ctx->index ? (ctx->index += !!argc) : ctx->index];
    if (arg && arg[0] == '-' && arg[1] == '-' && !arg[2]) {
        ctx->index++;
        return -1;
    }
    else if (!arg || arg[0] != '-') {
        return -1;
    }
    else if ((arg[1] < '0' || arg[1] > '9') && (arg[1] < 'A' || arg[1] > 'Z') && (arg[1] < 'a' || arg[1] > 'z')) {
        return -1;
    }
    else {
        while (*optstr && arg[ctx->position + 1] != *optstr) {
            optstr++;
        }
        ctx->option = arg[ctx->position + 1];
        if (!*optstr) {
            return '?';
        }
        else if (optstr[1] == ':') {
            if (arg[ctx->position + 2]) {
                ctx->arg = &arg[ctx->position + 2];
                ctx->index++;
                ctx->position = 0;
                return ctx->option;
            }
            else if (argv[ctx->index + 1]) {
                ctx->arg = argv[ctx->index + 1];
                ctx->index += 2;
                ctx->position = 0;
                return ctx->option;
            }
            else {
                return ':';
            }
        }
        else {
            if (!arg[++ctx->position + 1]) {
                ctx->index++;
                ctx->position = 0;
            }
            return ctx->option;
        }
    }
}

bool xfexists(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (f) {
        return !fclose(f);
    }
    return false;
}

bool xisdir(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

bool _xmkdirs(char **strs, size_t count)
{
    // Allocate the required length
    uint8_t path[PATH_MAX] = {0};
    slice_t s = STATIC_SLICE(path);

    for (char **str = strs; &str[0] < &strs[count]; str++) {
        SLICE_APPEND_STR(&s, *str);

        if (!xisdir((char *)s.data)) {
            if (xmkdir((char *)s.data, 0700)) {
                return false;
            }
        }
    }

    return true;
}

char *xget_dir(char *file)
{
    char *home = xgethome();
    if (!home) {
        return NULL;
    }

    char parcel_dir[] = "/parcel/";
    char files_dir[] = "files/";

    if (xmkdirs(home, parcel_dir, files_dir)) {
        xwarn("> Unable to create directory\n");
        return NULL;
    }
    return xstrcat(home, parcel_dir, files_dir, file);
}

const char *xconstbasename(const char *path)
{
    const char *s = path;
    for (; *s; s++);
    for (; s > path && s[-1] != '/' && s[-1] != '\\'; s--);
    return s;
}

bool xfiletype(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return false;
    }

    // Only checking the first 8k
    uint8_t data[8192] = { 0 };
    size_t len = fread(data, 1, sizeof(data), fp);
    fclose(fp);

    if (!len) {
        return false;
    }

    // NULL byte check
    for (size_t i = 0; i < len; i++) {
        if (!data[i]) {
            return true;
        }
    }

    // Heuristic ratio of printable bytes
    size_t ascii = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t c = data[i];
        if ((c >= 0x20 && c <= 0x7e) || c == '\t' || c == '\n' || c == '\r') {
            ascii++;
        }
    }

    return ((100 * ascii) / len) < 70;
}


size_t xbasename(const char *path, char *filename)
{
    const size_t path_length = strnlen(path, FILENAME_MAX);
    size_t pos = path_length + 1;
    char *base = NULL;
    for (const char *start = path;; pos--) {
        if (start[pos] == '/') {
            base = (void *)&start[pos + 1];
            const size_t filename_length = strnlen(base, FILENAME_MAX);
            memcpy(filename, base, filename_length);
            return filename_length;
        }
    }
    memcpy(filename, path, path_length); // path is just the filename
    return path_length;
}

bool full_write(int fd, const void *data, size_t len)
{
    const char *s = (const char *)data;
    for (size_t offset = 0; offset < len;) {
        ptrdiff_t r = (ptrdiff_t)xwrite(fd, &s[offset], len - offset);
        if (r < 1) {
            return false;
        }
        offset += r;
    }
    return true;
}

typedef struct req_rsp_t {
    char *data;
    size_t len;
} req_rsp_t;

static bool http_request(req_rsp_t *resp)
{
    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };

    struct addrinfo *host_info = NULL;
    const char *addr = "ipinfo.io";
    const char *port  = "80";
    if (xgetaddrinfo(addr, port, &hints, &host_info)) {
        log_error("unable to get addrinfo for %s:%s", addr, port);
        freeaddrinfo(host_info);
        return false;
    }

    sock_t sock;
    if (!xsocket(&sock, host_info->ai_family, host_info->ai_socktype, host_info->ai_protocol)) {
        log_error("failed to create socket for %s:%s", addr, port);
        freeaddrinfo(host_info);
        return false;
    }

    if (connect(sock, host_info->ai_addr, host_info->ai_addrlen)) {
        log_error("failed to connect to %s:%s", addr, port);
        xclose(sock);
        return false;
    }

    freeaddrinfo(host_info);
    log_debug("connected to %s:%s", addr, port);

    // https://ipinfo.io/developers#filtering-responses
    static const char api[] = "GET /ip?token=$TOKEN HTTP/1.0\r\nHost: ipinfo.io\r\n\r\n";
    if (!xsendall(sock, api, sizeof(api))) {
        log_error("failed to send api request to %s:%s", addr, port);
        xclose(sock);
        return false;
    }

    // Receive response
    for (size_t i = 0; i < resp->len;) {
        ssize_t r = 0;
        if ((r = xrecv(sock, &resp->data[i], resp->len - i, 0)) <= 0) {
            if (r) {
                log_error("error receiving data from socket");
                xclose(sock);
                return false;
            }
            if (xclose(sock)) {
                log_error("error closing socket");
                return false;
            }
            resp->len = i;
            return true;
        }
        else {
            i += r;
        }
    }

    xclose(sock);
    return false;
}

static bool http_extract_body(req_rsp_t *r)
{
    char *s = r->data;
    for (size_t i = r->len;;) {
        char *lf = xmemchr(s, '\n', i); // Pointer to the next linefeed
        if (!lf) {
            break; // No linefeeds left
        }
        switch (lf - s) {
            case 0:
                goto body;
            case 1:
                if (*s == '\r') { // \r\n
            body:
                    r->len = (r->len - (&lf[1] - r->data)) + 1; // Set to body length
                    r->data = &lf[1];                           // Move to first body character
                    return true;
                }
                // fallthrough
            default:
                s = &lf[1];                 // Move data pointer to the character after '\n'
                i = r->len - (s - r->data); // Update remaining length
        }
    }
    return false;
}

bool xgetpublicip(char *ip)
{
    char data[RESPONSE_LENGTH] = { 0 };
    req_rsp_t r = { data, RESPONSE_LENGTH };

    if (!http_request(&r)) {
        return false;
    }
    if (!http_extract_body(&r)) {
        return false;
    }
    memcpy(ip, r.data, r.len);
    return true;
}

// Bare-minimum unsigned-int-to-string
size_t xutoa(uint32_t value, char *str)
{
    char ascii[11];
    char *digit = &ascii[0];
    for (; value > 0; value /= 10) {
        *digit++ = '0' + (char)(value % 10);
    }
    for (value = 0; digit != ascii;) {
        *str++ = *--digit;
        str[value++] = '\0';
    }
    return value;
}

void *xmemchr(const void *src, int c, size_t len)
{
    const unsigned char *s = src;
    for (; len && *s != (unsigned char)c; s++, len--);
    return len ? (void *)s : NULL;
}

void *xmemrchr(const void *src, int c, size_t len)
{
    for (const unsigned char *s = src;; len--) {
        if (s[len] == (unsigned char)c) {
            return (void *)(s + len);
        }
    }
    return NULL;
}

void *xmemdup(void *mem, size_t len)
{
    if (mem && len) {
        return memcpy(xmalloc(len), mem, len);
    }
    return NULL;
}

void xmemcpy_locked(pthread_mutex_t *lock, void *dst, void *src, size_t len)
{
    pthread_mutex_lock(lock);
    memcpy(dst, src, len);
    pthread_mutex_unlock(lock);
}

void xhexdump(const void *src, size_t len)
{
    static const char hex[16] = "0123456789abcdef";

    //              offset                        bytes                             ascii
    //            00000000  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
    char row[] = "--------  -- -- -- -- -- -- -- --  -- -- -- -- -- -- -- --  |----------------|";
    //                      ^  ^  ^  ^  ^  ^  ^  ^   ^  ^  ^  ^  ^  ^  ^  ^    ^
    //                     10 13 16 19 22 25 28 31  35 38 41 44 47 50 53 56   61

    static const uint8_t map[] = {
        10, 61, 13, 62, 16, 63, 19, 64,
        22, 65, 25, 66, 28, 67, 31, 68,
        35, 69, 38, 70, 41, 71, 44, 72,
        47, 73, 50, 74, 53, 75, 56, 76
    };

    static const char ascii_table[] = {
        0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
        0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
        0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x2e,
        0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
        0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
        0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
        0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
        0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
        0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
        0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e,
        0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e
    };

    const uint8_t *mem = src;

    for (size_t offset = 0;; offset += 16) {
        size_t line_len = ((len - offset) > 16) ? 16 : len - offset;
        if (!line_len) {
            break;
        }

        uint8_t line[16] = { 0 };
        memcpy(line, &mem[offset], line_len);

        // Offset
        for (size_t i = 0; i < 8; i++) {
            row[i] = hex[(offset >> (28 - 4 * i)) & 0xf];
        }

        for (size_t i = 0; i < 16; i++) {
            // Hex
            row[map[2 * i] + 0] = hex[line[i] >> 4];
            row[map[2 * i] + 1] = hex[line[i] & 15];

            // ASCII
            row[map[2 * i + 1]] = ascii_table[line[i]];
        }

        // Clear on last loop
        for (size_t i = line_len; i < 16; i++) {
            // Hex
            row[map[2 * i] + 0] = 0x20;
            row[map[2 * i] + 1] = 0x20;

            // ASCII
            row[map[2 * i + 1]] = 0x20;
        }

        printf("\033[33m%s\033[0m\n", row);

        if (line_len != 16) {
            break;
        }
    }
}

void xmemprint(const void *src, size_t len)
{
    const uint8_t *data = src;
    for (size_t i = 0; i < len; i += 4) {
        char hex[] = "---------";
        for (size_t j = 0; j < 4; j++) {
            hex[2 * j + 0] = "0123456789abcdef"[data[j + i] >> 4];
            hex[2 * j + 1] = "0123456789abcdef"[data[j + i] & 15];
        }
        if (i + 4 >= len) {
            hex[8] = '\n';
        }
        fprintf(stdout, "%s", hex);
    }
}
