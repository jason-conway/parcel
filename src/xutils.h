/**
 * @file xutils.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Shared utility functions
 * @version 0.9.2
 * @date 2022-04-06
 *
 * @copyright Copyright (c) 2022 - 2024 Jason Conway. All rights reserved.
 *
 */

#pragma once

#include "slice.h"
#include "xplatform.h"

typedef struct xgetopt_t {
    char *arg;
    ptrdiff_t index;
    ptrdiff_t option;
    ptrdiff_t position;
} xgetopt_t;

enum color {
    DEF = '\0', // No color
    BLK = '0',  // 0x30
    RED = '1',  // 0x31
    GRN = '2',  // 0x32
    YEL = '3',  // 0x33
    BLU = '4',  // 0x34
    MAG = '5',  // 0x35
    CYA = '6',  // 0x36
    WHT = '7'   // 0x37
};

enum style {
    NONE = '0',
    BOLD = '1',
    ITALIC = '3',
    UNDERLINE = '4'
};

// typedef struct slice_t {
// 	char *data;
// 	size_t len;
// } slice_t;

enum HTTP {
    RESPONSE_LENGTH = 4096
};

// Print a formatted string in yellow to stdout
void xwarn(const char *format, ...);

// Print a formatted string in red to stderr
void xalert(const char *format, ...);

// Print a formatted string to stdout... in color
void xprintf(enum color color, enum style style, const char *format, ...);

// Write `data` to file descriptor `fd`, retrying if interrupted or short write occurs
bool full_write(int fd, const void *data, size_t len);

#define WRITE_CONST_STR(fd, s) full_write((fd), s, sizeof(s) - 1)

#define WRITE_STR(fd, s) full_write((fd), (s), strlen(s))

/**
 * @brief Duplicate a string
 *
 * @param str pointer to original string
 * @return Pointer to duplicate string, NULL if malloc() fails
 */
char *xstrdup(const char *str);

/**
 * @brief Concatenate `count` strings right to left
 *
 * @param count number of strings provided
 * @param ... `count` strings
 * @return pointer to concatenated string, NULL if malloc() fails
 */
char *_xstrcat(const char **strs, size_t count);

#define xstrcat(...) _xstrcat((const char *[]) { __VA_ARGS__ }, sizeof(const char *[]) { __VA_ARGS__ } / sizeof(const char *))

/**
 * @brief Extract filename from full path
 *
 * @param path Full path to file
 * @param filename Buffer for the filename to be placed in
 * @return Returns the length of filename, not including null-terminator
 */
size_t xbasename(const char *path, char *filename);

bool xfexists(const char *filename);

/**
 * @brief Send message of len bytes across multiple send() calls if required
 *
 * @param sockfd File descriptor of the sending socket
 * @param data Data to be sent
 * @param len Length of 'data'
 * @return false on failure
 */
bool xsendall(sock_t socket, const void *data, size_t len);

/**
 * @brief Receive len-bytes into data, blocking until full
 *
 * @param sockfd File descriptor of the connected socket
 * @param data Message buffer to place received data into
 * @param len Number of bytes to recieve
 * @return Number of bytes remaining, i.e., 0 on success
 */
ssize_t xrecvall(sock_t socket, void *data, size_t len);

/**
 * @brief Convert ascii to long, checking that the converted value is between the provided bounds
 *
 * @param[in] arg input string
 * @param[inout] larg optional pointer to store the converted integer at
 * @param[in] min lower bound
 * @param[in] max upper bound
 * @return true if in bounds, otherwise false
 */
bool xstrrange(char *arg, long *larg, long min, long max);

ptrdiff_t xgetopt(xgetopt_t *optctx, int argc, char **argv, const char *optstr);

// slice_t *slice_append(slice_t *slice, const char *data, size_t len);

// bool xgetpublicip(char *ip);

size_t xutoa(uint32_t value, char *str);

/**
 * @brief Make a directory if it doesn't exist
 *
 * @param depth depth of target directory
 * @param ... path of new directory, one arg for each depth
 * @return Zero on success
 */
bool _xmkdirs(char **strs, size_t count);

#define xmkdirs(...) _xmkdirs((char *[]) { __VA_ARGS__ }, sizeof(char *[]) { __VA_ARGS__ } / sizeof(char *))

void *xmemdup(void *mem, size_t len);
void *xmemchr(const void *src, int c, size_t len);
void *xmemrchr(const void *src, int c, size_t len);
void xmemprint(const void *src, size_t len);
void xmemcpy_locked(pthread_mutex_t *lock, void *dst, void *src, size_t len);
void xhexdump(const void *src, size_t len);
