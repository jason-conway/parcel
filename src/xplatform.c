/**
 * @file xplatform.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief xplatform (cross-platform) wrappers
 * @version 0.1
 * @date 2022-01-26
 *
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 *
 */

#include "xplatform.h"

/**
 * @section BSD / Winsock wrappers
 */

ssize_t xsend(sock_t socket, const void *data, size_t len, int flags)
{
#if __unix__ || __APPLE__
	return send(socket, data, len, flags);
#elif _WIN32
	return send(socket, (const char *)data, (int)len, flags);
#endif
}

ssize_t xsendall(sock_t socket, const void *data, size_t len)
{
	size_t total_sent = 0;
	ssize_t bytes_sent;
	size_t remaining = len;
	while (total_sent < len) {
		if ((bytes_sent = xsend(socket, data + total_sent, remaining, 0)) < 0) {
			return -1;
		}
		total_sent += bytes_sent;
		remaining -= bytes_sent;
	}
	return total_sent;
}

ssize_t xrecv(sock_t socket, void *data, size_t len, int flags)
{
#if __unix__ || __APPLE__
	return recv(socket, data, len, flags);
#elif _WIN32
	return recv(socket, (char *)data, (int)len, flags);
#endif
}

int xclose(sock_t socket)
{
#if __unix__ || __APPLE__
	return close(socket);
#elif _WIN32
	return closesocket(socket);
#endif
}

int xaccept(sock_t *connection_socket, sock_t listening_socket, struct sockaddr *address, socklen_t *len)
{
#if __unix__ || __APPLE__
	*connection_socket = accept(listening_socket, address, len);
	return *connection_socket;
#elif _WIN32
	sock_t sd = accept(listening_socket, address, (int *)len);
	if (sd == INVALID_SOCKET) {
		return -1;
	}
	*connection_socket = sd;
	return 0;
#endif
}

int xsocket(sock_t *xsocket, int domain, int type, int protocol)
{
#if __unix__ || __APPLE__
	*xsocket = socket(domain, type, protocol);
	return *xsocket;
#elif _WIN32
	sock_t sd = socket(domain, type, protocol);
	if (sd == INVALID_SOCKET) {
		return -1;
	}
	*xsocket = sd;
	return 0;
#endif
}

int xstartup(void)
{
#if __unix__ || __APPLE__
	return 0; // As if
#elif _WIN32
	HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD mode;
	if (GetConsoleMode(stdout_handle, &mode)) {
		mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		(void)SetConsoleMode(stdout_handle, mode);
	}
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		return -1;
	}
	return 0;
#endif
}

int xgetaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res)
{
#if __unix__ || __APPLE__
	return getaddrinfo(node, service, hints, res);
#elif _WIN32
	if (getaddrinfo(node, service, hints, res)) {
		(void)WSACleanup();
		return -1;
	}
	return 0;
#endif
}

int xsetsockopt(sock_t socket, int level, int optname, const void *optval, socklen_t optlen)
{
#if __unix__ || __APPLE__
	return setsockopt(socket, level, optname, optval, optlen);
#elif _WIN32
	(void)optlen;
	const char *opt = optval;
	if (setsockopt(socket, level, optname, opt, optlen)) {
		(void)WSACleanup();
		return -1;
	}
	return 0;
#endif
}

int xgetifaddrs(void)
{
#if __unix__ || __APPLE__
	struct ifaddrs *interfaces;
	if (getifaddrs(&interfaces)) {
		return -1;
	}

	for (struct ifaddrs *node = interfaces; node; node = node->ifa_next) {
		if (!node->ifa_addr) {
			continue;
		}
		if (node->ifa_addr->sa_family == AF_INET) {
			char interface_name[NI_MAXHOST] = { 0 };
			if (getnameinfo(node->ifa_addr, sizeof(struct sockaddr_in), interface_name, NI_MAXHOST, NULL, 0, NI_NUMERICHOST)) {
				return -1;
			}
			printf(">\t%s\n", interface_name);
		}
	}
	freeifaddrs(interfaces);
	return 0;
#elif _WIN32
	ULONG ip_adapter_len[1] = { sizeof(IP_ADAPTER_INFO) };
	PIP_ADAPTER_INFO adapter_info = (IP_ADAPTER_INFO *)HeapAlloc(GetProcessHeap(), 0, ip_adapter_len[0]);
	if (!adapter_info) {
		fprintf(stderr, "\033[31merror: HeapAlloc()\n\033[0m");
		return -1;
	}
	if (GetAdaptersInfo(adapter_info, ip_adapter_len) == ERROR_BUFFER_OVERFLOW) {
		(void)HeapFree(GetProcessHeap(), 0, (adapter_info));
		adapter_info = (IP_ADAPTER_INFO *)HeapAlloc(GetProcessHeap(), 0, ip_adapter_len[0]);
		if (!adapter_info) {
			fprintf(stderr, "\033[31merror: HeapAlloc()\n\033[0m");
			return -1;
		}
	}
	if (GetAdaptersInfo(adapter_info, ip_adapter_len)) {
		fprintf(stderr, "\033[31merror: GetAdaptersInfo()\n\033[0m");
		(void)HeapFree(GetProcessHeap(), 0, adapter_info);
		return -1;
	}
	for (PIP_ADAPTER_INFO adapter = adapter_info; adapter; adapter = adapter->Next) {
		printf(">\t%s\n", adapter->IpAddressList.IpAddress.String);
	}

	(void)HeapFree(GetProcessHeap(), 0, adapter_info);
	return 0;
#endif
}

int xgetpeername(sock_t socket, struct sockaddr *address, socklen_t *len)
{
#if __unix__ || __APPLE__
	return getpeername(socket, address, len);
#elif _WIN32
	return getpeername(socket, address, (int *)len);
#endif
}
/**
 * @section FD_SET related functions
 */

sock_t xfd_isset(fd_set *fds, fd_set *set, size_t index)
{
#if __unix__ || __APPLE__
	(void)fds;
	if (FD_ISSET((sock_t)index, set)) {
		return (sock_t)index;
	}
#elif _WIN32
	sock_t fd = fds->fd_array[index];
	if (FD_ISSET(fd, set)) {
		return fd;
	}
#endif
	return 0;
}

sock_t xfd_inset(fd_set *set, size_t index)
{
#if __unix__ || __APPLE__
	if (FD_ISSET((sock_t)index, set)) {
		return (sock_t)index;
	}
#elif _WIN32
	sock_t fd = set->fd_array[index];
	if (FD_ISSET(fd, set)) {
		return fd;
	}
#endif
	return 0;
}

size_t xfd_count(sock_t fd, size_t count)
{
#if __unix__ || __APPLE__
	if (fd > (sock_t)count) {
		return (size_t)fd;
	}
	return count;
#elif _WIN32
	(void)fd;
	return count + 1;
#endif
}

size_t xfd_init_count(sock_t fd)
{
#if __unix__ || __APPLE__
	return (size_t)fd;
#elif _WIN32
	(void)fd;
	return 1;
#endif
}

/**
 * @section unistd / win32 wrappers
 */

void xgetrandom(void *dest, size_t len)
{
#if __unix__ || __APPLE__
	FILE *random = fopen("/dev/urandom", "rb");
	if (!random) {
		exit(EXIT_FAILURE);
	}
	if (!fread(dest, len, 1, random)) {
		exit(EXIT_FAILURE);
	}
	(void)fclose(random); // Error checking when stream is RO?
#elif _WIN32
	if (!SystemFunction036(dest, len)) {
		exit(EXIT_FAILURE);
	}
#endif
}

int xgetopt(xgetopt_t *x, int argc, char **argv, const char *optstr)
{
	char *arg = argv[!x->index ? (x->index += !!argc) : x->index];
	if (arg && arg[0] == '-' && arg[1] == '-' && !arg[2]) {
		x->index++;
		return -1;
	}
	else if (!arg || arg[0] != '-') {
		return -1;
	}
	else if ((arg[1] < '0' || arg[1] > '9') && (arg[1] < 'A' || arg[1] > 'Z') && (arg[1] < 'a' || arg[1] > 'z')) {
		return -1;
	}
	else {
		while (*optstr && arg[x->position + 1] != *optstr) {
			optstr++;
		}
		x->option = arg[x->position + 1];
		if (!*optstr) {
			return '?';
		}
		else if (optstr[1] == ':') {
			if (arg[x->position + 2]) {
				x->arg = &arg[x->position + 2];
				x->index++;
				x->position = 0;
				return x->option;
			}
			else if (argv[x->index + 1]) {
				x->arg = argv[x->index + 1];
				x->index += 2;
				x->position = 0;
				return x->option;
			}
			else {
				return ':';
			}
		}
		else {
			if (!arg[++x->position + 1]) {
				x->index++;
				x->position = 0;
			}
			return x->option;
		}
	}
}

void xgetline(char **message, size_t *message_length, FILE *stream)
{
	size_t line_length = 256;
	char *line = malloc(line_length);
	if (!line) {
		*message = NULL;
		return;
	}

	size_t read_length = 0;
	while (1) {
		if (read_length == line_length - 2) {
			line[read_length] = 0;
			if (line_length * 2 < line_length) {
				free(line);
				*message = NULL;
				return;
			}

			char *new_line = NULL;
			if (!(new_line = realloc(line, line_length *= 2))) {
				free(line);
				*message = NULL;
				return;
			}
			line = new_line;
		}

		int c = fgetc(stream);
		if (c == EOF) {
			goto eol;
		}

		line[read_length++] = c;
		if (c == '\n') {
		eol:
			if (read_length == 1) {
				*message_length = 0;
				*message = NULL;
				free(line);
				return;
			}
			line[read_length] = '\0';
			break;
		}
	}
	*message = line;
	*message_length = read_length;
	return;
}
