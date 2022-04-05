/**
 * @file xplatform.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief xplatform (cross-platform) methods for Parcel
 * @version 0.9.1
 * @date 2022-01-26
 *
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 *
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/types.h>

#if __unix__ || __APPLE__
	#include <unistd.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <termios.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <ifaddrs.h>
	#include <dirent.h>

	typedef int sock_t;
#endif

#if _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <winsock2.h>
	#include <ws2ipdef.h>
	#include <ws2tcpip.h>
	#include <mstcpip.h>
	#include <iphlpapi.h>
	#include <ntsecapi.h>
	#include <winbase.h>
	#include <io.h>
	#ifdef _MSC_VER
		#pragma comment(lib, "ws2_32.lib")
		#pragma comment(lib, "advapi32")
		#pragma comment(lib, "IPHLPAPI.lib")
	#endif

	#if __MINGW32__
		// char *inet_ntop(int af, const void *src, char *dst, size_t size);
	#endif

	typedef unsigned short in_port_t;
	typedef SOCKET sock_t;
#endif

typedef struct xgetopt_t
{
	char *arg;
	int index;
	int option;
	int position;
} xgetopt_t;

typedef enum ansi
{
	RED,
	GRN,
	YEL,
	MAG
} ansi;

void xgetrandom(void *dest, size_t len);

int xsocket(sock_t *xsocket, int domain, int type, int protocol);
int xaccept(sock_t *connection_socket, sock_t listening_socket, struct sockaddr *address, socklen_t *len);

ssize_t xsend(sock_t socket, const void *data, size_t len, int flags);
ssize_t xrecv(sock_t socket, void *data, size_t len, int flags);

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

size_t xfd_count(sock_t fd, size_t count);
size_t xfd_init_count(sock_t fd);
sock_t xfd_isset(fd_set *set, fd_set *read_fds, size_t index);
sock_t xfd_inset(fd_set *set, size_t index);

int xgetaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
int xgetpeername(sock_t socket, struct sockaddr *address, socklen_t *len);
int xgetpeeraddr(sock_t socket, char *address, in_port_t *port);
bool xport_valid(char *arg);
int xsetsockopt(sock_t socket, int level, int optname, const void *optval, socklen_t optlen);
int xclose(sock_t socket);
void xexit(int status);
int xgetopt(xgetopt_t *x, int argc, char **argv, const char *optstr);
char *xprompt(const char *prompt_msg, const char *error_msg, size_t *len);
void xgetline(char **message, size_t *message_length, FILE *stream);
int xgetifaddrs(void);
int xstartup(void);

int xgetusername(char *username, size_t len);

void xwarn(const char *format, ...);
void xalert(const char *format, ...);
void xprintf(ansi color, const char *format, ...);

bool xfexists(const char *filename);
size_t xfsize(const char *filename);
char *xstrdup(const char *str);

/**
 * @brief Concatenate `count` strings right to left
 * 
 * @param count number of strings provided
 * @param ... `count` strings
 * @return pointer to concatenated string, NULL if malloc() fails
 */
char *xstrcat(size_t count, ...);
