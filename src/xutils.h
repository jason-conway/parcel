/**
 * @file xutils.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Shared utility functions
 * @version 0.9.1
 * @date 2022-04-06
 * 
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 * 
 */

#pragma once

#include "xplatform.h"

typedef struct xgetopt_t {
	char *arg;
	int index;
	int option;
	int position;
} xgetopt_t;

typedef enum ansi {
	RED,
	GRN,
	YEL,
	MAG
} ansi;

typedef struct http_request_t {
	char *data;
	size_t length;
} http_request_t;

enum HTTP {
	RESPONSE_LENGTH = 4096
};

// Print a formatted string in yellow to stdout
void xwarn(const char *format, ...);

// Print a formatted string in red to stderr
void xalert(const char *format, ...);

// Print a formatted string to stdout... in color
void xprintf(ansi color, const char *format, ...);

void xlog(const char *format, ...);

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
char *xstrcat(size_t count, ...);


char *xbasename(char *path);

bool xfexists(const char *filename);

/**
 * @brief Send message of len bytes across multiple send() calls if required
 * 
 * @param sockfd File descriptor of the sending socket
 * @param data Data to be sent
 * @param len Length of 'data'
 * @return Number of bytes sent
 */
ssize_t xsendall(sock_t socket, const void *data, size_t len);

/**
 * @brief Receive len-bytes into data, blocking until full
 * 
 * @param sockfd File descriptor of the connected socket
 * @param data Message buffer to place received data into
 * @param len Number of bytes to recieve
 * @return Number of bytes remaining, i.e., 0 on success
 */
ssize_t xrecvall(sock_t socket, void *data, size_t len);

bool xport_valid(char *arg);

int xgetopt(xgetopt_t *x, int argc, char **argv, const char *optstr);

char *xprompt(const char *prompt_msg, const char *error_msg, size_t *len);

void xgetline(char **message, size_t *message_length, FILE *stream);

char *xgetpublicip(void);
