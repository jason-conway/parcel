/**
 * @file xutils.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Shared utility functions
 * @version 0.9.2
 * @date 2022-04-06
 *
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 *
 */

#include "xutils.h"
#include "xplatform.h"

char *xstrdup(const char *str)
{
	if (!str) {
		return NULL;
	}
	size_t length = strlen(str) + 1;
	char *copy = xmalloc(length);
	if (!copy) {
		return NULL;
	}
	return memcpy(copy, str, length);
}

char *xstrcat(size_t count, ...)
{
	va_list ap;
	va_start(ap, count);

	// Allocate the required length
	size_t length = 1;
	for (size_t i = 0; i < count; i++) {
		char *substring = va_arg(ap, char *);
		length += strlen(substring);
	}
	va_end(ap);

	char *str = xmalloc(length);
	if (!str) {
		return NULL;
	}

	// Append each arg to the end of the last
	size_t offset = 0;
	va_start(ap, count);
	for (size_t i = 0; i < count; i++) {
		char *substring = va_arg(ap, char *);
		size_t substring_length = strlen(substring);
		memcpy(&str[offset], substring, substring_length);
		offset += substring_length;
	}
	str[offset] = '\0';
	va_end(ap);

	return str;
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

ssize_t xsendall(sock_t socket, const void *data, size_t len)
{
	uint8_t *_data = (uint8_t *)data;
	for (size_t i = 0; i < len;) {
		ssize_t bytes_sent = xsend(socket, &_data[i], len - i, 0);
		switch (bytes_sent) {
			case -1:
				return -1;
			default:
				i += bytes_sent;
		}
	}
	return 0;
}

ssize_t xrecvall(sock_t socket, void *data, size_t len)
{
	uint8_t *_data = (uint8_t *)data;
	for (size_t i = 0; i < len;) {
		ssize_t bytes_recv = xrecv(socket, &_data[i], len - i, 0);
		switch (bytes_recv) {
			case -1:
				return -1;
			default:
				i += bytes_recv;
		}
	}
	return 0;
}

int xgetpeeraddr(sock_t socket, char *address, in_port_t *port)
{
	struct sockaddr_in client_sockaddr;
	socklen_t len = sizeof(client_sockaddr);
	if (xgetpeername(socket, (struct sockaddr *)&client_sockaddr, &len)) {
		return -1;
	}
	(void)inet_ntop(AF_INET, &client_sockaddr.sin_addr, address, INET_ADDRSTRLEN);
	*port = ntohs(client_sockaddr.sin_port);
	return 0;
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

int xgetopt(xgetopt_t *ctx, int argc, char **argv, const char *optstr)
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

slice_t *slice_append(slice_t *slice, const char *data, size_t len)
{
	char *new = xrealloc(slice->data, slice->len + len);
	if (!new) {
		return NULL;
	}

	memcpy(new + slice->len, data, len);
	slice->data = new;
	slice->len += len;
	return slice;
}

bool xfexists(const char *filename)
{
	FILE *f = fopen(filename, "r");
	if (f) {
		(void)fclose(f);
		return true;
	}
	return false;
}

bool xisdir(const char *path)
{
	struct stat info;
	return (stat(path, &info) == 0 && S_ISDIR(info.st_mode));
}

int xmkdirs(size_t depth, ...)
{
	va_list ap;
	va_start(ap, depth);
	slice_t path_slice = { NULL, 0 };
	for (size_t i = 0; i < depth; i++) {
		char *directory = va_arg(ap, char *);
		slice_append(&path_slice, directory, strlen(directory));
		char *path = xmalloc(path_slice.len + 1);
		memcpy(path, path_slice.data, path_slice.len);
		path[path_slice.len] = '\0';

		if (!xisdir(path)) {
			if (xmkdir(path, 0700)) {
				xfree(path);
				return -1;
			}
		}
		xfree(path);
	}

	xfree(path_slice.data);
	va_end(ap);
	return 0;
}

char *xget_dir(char *file)
{
	char *home = xgethome();
	if (!home) {
		return NULL;
	}

	static const char parcel_dir[] = "/parcel/";
	static const char files_dir[] = "files/";

	if (xmkdirs(3, home, parcel_dir, files_dir)) {
		xwarn("> Unable to create directory\n");
		return NULL;
	}
	return xstrcat(4, home, parcel_dir, files_dir, file);
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

static int http_request(slice_t *request)
{
	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};

	struct addrinfo *host_info;
	if (xgetaddrinfo("ipinfo.io", "80", &hints, &host_info)) {
		freeaddrinfo(host_info);
		return -1;
	}

	sock_t http_socket;
	if (xsocket(&http_socket, host_info->ai_family, host_info->ai_socktype, host_info->ai_protocol) < 0) {
		freeaddrinfo(host_info);
		return -1;
	}

	if (connect(http_socket, host_info->ai_addr, host_info->ai_addrlen)) {
		(void)xclose(http_socket);
		return -1;
	}

	freeaddrinfo(host_info);

	// https://ipinfo.io/developers#filtering-responses
	static const char api_request[] = "GET /ip?token=$TOKEN HTTP/1.0\r\nHost: ipinfo.io\r\n\r\n";
	if (xsendall(http_socket, api_request, sizeof(api_request))) {
		(void)xclose(http_socket);
		return -1;
	}

	// Receive response
	for (size_t i = 0; i < request->len;) {
		ssize_t bytes_recv = 0;
		if ((bytes_recv = xrecv(http_socket, request->data + i, request->len - i, 0)) <= 0) {
			if (bytes_recv) {
				(void)xclose(http_socket);
				return -1;
			}
			(void)xclose(http_socket);
			request->len = i;
			return 0;
		}
		else {
			i += bytes_recv;
		}
	}

	(void)xclose(http_socket);
	return -1;
}

static int http_extract_body(slice_t *request)
{
	char *pos_data = request->data;
	for (size_t i = request->len;;) {
		char *pos_linefeed = xmemchr(pos_data, '\n', i); // Pointer to the next linefeed
		if (!pos_linefeed) {
			break; // No linefeeds left
		}

		switch (pos_linefeed - pos_data) {
			case 0:
				// reached_body could be replaced with `case 0:` but it brakes vscode syntax highlighting
				goto reached_body;
			case 1:
				if (*pos_data == '\r') { // \r\n
				reached_body:
					request->len = (request->len - (pos_linefeed + 1 - request->data)) + 1; // Set to body length
					request->data = pos_linefeed + 1; // Move to first body character
					return 0;
				}
				// fallthrough
			default:
				pos_data = pos_linefeed + 1; // Move data pointer to the character after '\n'
				i = request->len - (pos_data - request->data); // Update remaining length
		}
	}
	return -1;
}

char *xgetpublicip(void)
{
	char *http_response = xcalloc(RESPONSE_LENGTH);
	if (!http_response) {
		return NULL;
	}

	slice_t request = { http_response, RESPONSE_LENGTH };

	if (http_request(&request)) {
		xfree(http_response);
		return NULL;
	}

	if (http_extract_body(&request)) {
		xfree(http_response);
		return NULL;
	}

	char *public_ip = xcalloc(request.len);
	if (!public_ip) {
		return NULL;
	}

	memcpy(public_ip, request.data, request.len);
	xfree(http_response);
	return public_ip;
}

// Bare-minimum unsigned-int-to-string
size_t xutoa(uint32_t value, char *str)
{
	char ascii[11];
	char *digit;

	for (digit = &ascii[0]; value > 0; value /= 10) {
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
	for (; len && *s != (unsigned char)c; s++, len--) { };
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
	void *_mem = xmalloc(len);
	return mem ? memcpy(_mem, mem, len) : NULL;
}

void xmemcpy_locked(pthread_mutex_t *lock, void *dest, void *src, size_t len)
{
	pthread_mutex_lock(lock);
		memcpy(dest, src, len);
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
		memcpy(line, mem + offset, line_len);

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
		uint64_t chunk = (((uint64_t)data[i + 3] << 0x00) |
						  ((uint64_t)data[i + 2] << 0x08) |
						  ((uint64_t)data[i + 1] << 0x10) |
						  ((uint64_t)data[i + 0] << 0x18));
		printf("%s%" PRIx64, i ? "-" : "", chunk);
	}
}
