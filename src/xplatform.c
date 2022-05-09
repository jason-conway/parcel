/**
 * @file xplatform.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief xplatform (cross-platform) wrappers
 * @version 0.9.1
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
	DWORD mode;
	if (!GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode)) {
		return -1;
	}
	mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	if (!SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), mode)) {
		return -1;
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
			printf("\t%s\n", interface_name);
		}
	}
	freeifaddrs(interfaces);
	return 0;
#elif _WIN32
	ULONG ip_adapter_len = sizeof(IP_ADAPTER_INFO);
	PIP_ADAPTER_INFO adapter_info = (IP_ADAPTER_INFO *)xmalloc(ip_adapter_len);
	if (!adapter_info) {
		return -1;
	}
	if (GetAdaptersInfo(adapter_info, &ip_adapter_len) == ERROR_BUFFER_OVERFLOW) {
		xfree(adapter_info);
		adapter_info = (IP_ADAPTER_INFO *)xmalloc(ip_adapter_len);
		if (!adapter_info) {
			return -1;
		}
	}
	if (GetAdaptersInfo(adapter_info, &ip_adapter_len)) {
		xfree(adapter_info);
		return -1;
	}
	for (PIP_ADAPTER_INFO adapter = adapter_info; adapter; adapter = adapter->Next) {
		printf("\t%s\n", adapter->IpAddressList.IpAddress.String);
	}

	xfree(adapter_info);
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
	return 0;
#elif _WIN32
	sock_t fd = fds->fd_array[index];
	if (FD_ISSET(fd, set)) {
		return fd;
	}
	return 0;
#endif
}

sock_t xfd_inset(fd_set *set, size_t index)
{
#if __unix__ || __APPLE__
	if (FD_ISSET((sock_t)index, set)) {
		return (sock_t)index;
	}
	return 0;
#elif _WIN32
	sock_t fd = set->fd_array[index];
	if (FD_ISSET(fd, set)) {
		return fd;
	}
	return 0;
#endif
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
 * @section unistd / win32 wrappers and portable implementations
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
	if (!RtlGenRandom(dest, len)) {
		exit(EXIT_FAILURE);
	}
#endif
}

int xgetlogin(char *username, size_t len)
{
#if __unix__ || __APPLE__
	return getlogin_r(username, len) ? -1 : 0;
#elif _WIN32
	LPDWORD length = (LPDWORD)&len;
	return GetUserName(username, length) ? 0 : -1;
#endif
}

size_t xfilesize(const char *filename)
{
#if __unix__ || __APPLE__
	struct stat st;
	stat(filename, &st);
	return (size_t)st.st_size;
#elif _WIN32
	WIN32_FILE_ATTRIBUTE_DATA file_attributes;
	if (GetFileAttributesEx(filename, GetFileExInfoStandard, &file_attributes)) {
		ULARGE_INTEGER ul;
		ul.HighPart = file_attributes.nFileSizeHigh;
		ul.LowPart = file_attributes.nFileSizeLow;
		return (size_t)ul.QuadPart;
	}
	return 0;
#endif
}

/**
 * @section malloc / calloc wrapper to lessen Windows runtime dependency
 */

void *xmalloc(size_t len)
{
#if __unix__ || __APPLE__
	return malloc(len);
#elif _WIN32
	return HeapAlloc(GetProcessHeap(), 0, len);
#endif
}

void *xcalloc(size_t len)
{
#if __unix__ || __APPLE__
	void *mem = malloc(len);
	if (!mem) {
		return NULL;
	}
	return memset(mem, 0, len);
#elif _WIN32
	return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len);
#endif
}

void *xrealloc(void *mem, size_t len)
{
#if __unix__ || __APPLE__
	void *_mem = realloc(mem, len);
	if (!_mem) {
		xfree(mem);
		return NULL;
	}
	return _mem;
#elif _WIN32
	// Act as malloc() when mem is null
	void *_mem = (!mem) ? HeapAlloc(GetProcessHeap(), 0, len) : HeapReAlloc(GetProcessHeap(), 0, mem, len);
	if (!_mem) {
		xfree(mem);
		return NULL;
	}
	return _mem;
#endif
}

void xfree(void *mem)
{
#if __unix__ || __APPLE__
	free(mem);
#elif _WIN32
	(void)HeapFree(GetProcessHeap(), 0, mem);
#endif
}

// TODO: needs testing
char *xgethome(void)
{
#if __unix__ || __APPLE__
	char *home = getenv("HOME");
	if (!home || *home != '/') {
		return NULL;
	}
	return home;
#elif _WIN32
	char *home = getenv("HOMEPATH");
	return !home ? NULL : home;
#endif
}

// TODO: needs testing
int xmkdir(char *path)
{
#if __unix__ || __APPLE__
	char *c = strchr(path + 1, '/');
	while (c) {
		*c = 0;
		if (mkdir(path, 0700)) {
			return -1;
		}
		struct stat info;
		if (!stat(path, &info) && S_ISDIR(info.st_mode)) {
			return -1;
		}

		*c = '/';
		c = strchr(c + 1, '/');
	}
	return 0;
#elif _WIN32
	if (!CreateDirectory(path, 0)) {
		if (GetLastError() == ERROR_PATH_NOT_FOUND) {
			return -1;
		}
		else {
			DWORD file_attributes = GetFileAttributes(path);
			if ((file_attributes == INVALID_FILE_ATTRIBUTES) || !(file_attributes & FILE_ATTRIBUTE_DIRECTORY)) {
				return -1;
			}
		}
	}
	return 0;
#endif
}

char *xstrcat(size_t count, ...);

// TODO: rework
// TODO: needs testing
char *xget_dir(char *file)
{
	char *home = xgethome();
	if (!home) {
		return NULL;
	}

#if __unix__ || __APPLE__
	static const char parcel[] = "/parcel";
	static const char files[] = "/files/";
	char *path = xstrcat(4, home, parcel, files, file);
#elif _WIN32
	static const char parcel[] = "\\parcel";
	static const char files[] = "\\files\\";
	char *path = xstrcat(3, home, parcel, files);
#endif

	if (xmkdir(path)) {
		return NULL;
	}

#if __unix__ || __APPLE__
	return path;
#elif _WIN32
	xfree(path);
	return xstrcat(4, home, parcel, files, file);
#endif
}

ssize_t xwrite(int fd, const void *data, size_t len)
{
#if __unix__ || __APPLE__
	return write(fd, data, len);
#elif _WIN32
 	(void)fd;
	DWORD bytes_written = 0;
	if (!WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), data, len, &bytes_written, NULL)) {
		return -1;
	}
	return bytes_written;
#endif
}

char xgetch(void)
{
#if __unix__ || __APPLE__
	char c;
	return (read(STDIN_FILENO, &c, 1) == 1) ? c : 0;
#elif _WIN32
	char c;
	DWORD bytes_read;
	if (!ReadConsole(GetStdHandle(STD_INPUT_HANDLE), &c, 1, &bytes_read, NULL)) {
		return 0;
	}
	return (bytes_read == 1) ? c : 0;
#endif
}

size_t xwinsize(void)
{
#if __unix__ || __APPLE__
	struct winsize w;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) < 0) {
		return 0;
	}
	return w.ws_col;
#elif _WIN32
	CONSOLE_SCREEN_BUFFER_INFO info;
	if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
		return 0;
	}
	return (info.srWindow.Right - info.srWindow.Left + 1);
#endif
}

int xtcsetattr(console_t *orig, enum console_mode mode)
{
#if __unix__ || __APPLE__
	console_t raw;
	if (mode == CONSOLE_MODE_RAW) {
		if (tcgetattr(STDIN_FILENO, orig) < 0) {
			return -1;
		}

		memcpy(&raw, orig, sizeof(raw));

		raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
		raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
		raw.c_cflag |= CS8;
		raw.c_cc[VMIN] = 1;
		raw.c_cc[VTIME] = 0;
	}
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, (mode == CONSOLE_MODE_RAW) ? &raw : orig) < 0) {
		return -1;
	}
	return 0;
#elif _WIN32
	console_t raw;
	if (mode == CONSOLE_MODE_RAW) {
		if (!GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), orig)) {
			return -1;
		}

		memcpy(&raw, orig, sizeof(raw));
		raw &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
		raw |= ENABLE_VIRTUAL_TERMINAL_INPUT;
		// if (!SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), raw)) {
		// 	return -1;
		// }

		DWORD mode;
		if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode)) {
			mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
			(void)SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), mode);
		}
	}
	
	if (!SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), (mode == CONSOLE_MODE_RAW) ? raw : *orig)) {
		return -1;
	}
	return 0;

#endif
}