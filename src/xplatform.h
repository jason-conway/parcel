/**
 * @file xplatform.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief xplatform (cross-platform) methods for Parcel
 * @version 0.9.2
 * @date 2022-01-26
 *
 * @copyright Copyright (c) 2022 - 2024 Jason Conway. All rights reserved.
 *
 */

#pragma once

#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if __unix__ || __APPLE__
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <dirent.h>
#include <termios.h>
#include <sys/time.h>
#include <poll.h>
typedef int sock_t;
typedef struct termios console_t;
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
#include <direct.h>
#include <conio.h>
#ifdef _MSC_VER
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "advapi32")
    #pragma comment(lib, "IPHLPAPI.lib")
#endif
#ifndef SHUT_RDWR
    #define SHUT_RDWR SD_BOTH
#endif
typedef USHORT in_port_t;
typedef SOCKET sock_t;
typedef DWORD console_t;
#endif

typedef unsigned int bitfield;

#ifndef PARCEL_VERSION
    #define PARCEL_VERSION 0.9.2
#endif

#define STR(a)  XSTR(a)
#define XSTR(a) #a

#ifdef DEBUG
    #define debug_print(fmt, ...) \
        do { \
            fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, __VA_ARGS__); \
        } while (0)
#else
    #define debug_print(fmt, ...)
#endif

/**
 * @brief Platform-specific malloc(3)
 *
 * @param len bytes to be allocated
 * @return pointer to allocated memory
 */
void *xmalloc(size_t len);

/**
 * @brief Platform-specific calloc(3)-like
 *
 * @param len bytes to be allocated
 * @return pointer to allocated and zeroed memory
 */
void *xcalloc(size_t len);

/**
 * @brief Platform-specific realloc(3)
 *
 * @param[in] mem pointer to previous allocation or NULL
 * @param[in] len bytes to be allocated
 * @return pointer to allocation
 */
void *xrealloc(void *mem, size_t len);

/**
 * @brief Frees allocations created via xmalloc, xcalloc, or xrealloc
 *
 * @param[inout] mem pointer to allocation, set NULL after freeing
 * @return returns NULL to allow for return chaining
 */
void *xfree(void *mem);


int xsocket(sock_t *xsocket, int domain, int type, int protocol);
int xaccept(sock_t *connection_socket, sock_t listening_socket, struct sockaddr *address, socklen_t *len);

ssize_t xsend(sock_t socket, const void *data, size_t len, int flags);
ssize_t xrecv(sock_t socket, void *data, size_t len, int flags);

size_t xfd_count(sock_t fd, size_t count);
size_t xfd_init_count(sock_t fd);
sock_t xfd_isset(fd_set *set, fd_set *read_fds, size_t index);
sock_t xfd_inset(fd_set *set, size_t index);

int xgetaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
int xgetpeername(sock_t socket, struct sockaddr *address, socklen_t *len);
bool xgetpeeraddr(sock_t socket, char *address, in_port_t *port);

int xsetsockopt(sock_t socket, int level, int optname, const void *optval, socklen_t optlen);
int xclose(sock_t socket);

int xgetifaddrs(const char *prefix, const char *suffix);
int xstartup(void);

int xgetlogin(char *username, size_t len);
ssize_t xgetrandom(void *dst, size_t len);
size_t xfilesize(const char *filename);
char *xget_dir(char *file);
int xmkdir(const char *path, mode_t mode);
char *xgethome(void);

size_t xwinsize(void);
ssize_t xwrite(int fd, const void *data, size_t len);
char xgetch(void);
size_t xgetcp(uint8_t *c);

enum xconsole_mode {
    CONSOLE_MODE_RAW,
    CONSOLE_MODE_ORIG,
};
int xtcsetattr(console_t *orig, enum xconsole_mode mode);
