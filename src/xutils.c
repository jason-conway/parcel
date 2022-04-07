/**
 * @file xutils.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Shared utility functions
 * @version 0.9.1
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
	va_end(ap);
}

void xalert(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	(void)fprintf(stderr, "%s", "\033[0;31m");
	(void)vfprintf(stderr, format, ap);
	(void)fprintf(stderr, "\033[0m");
	va_end(ap);
}

void xprintf(ansi color, const char *format, ...)
{
	static const char *escape_codes[] = {
		"\033[0;31m", "\033[0;32m", "\033[0;33m", "\033[0;35m"
	};

	va_list ap;
	va_start(ap, format);
	(void)fprintf(stdout, "%s", escape_codes[color]);
	(void)vfprintf(stdout, format, ap);
	(void)fprintf(stdout, "\033[0m");
	va_end(ap);
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

ssize_t xrecvall(sock_t socket, void *data, size_t len)
{
	return xrecv(socket, data, len, MSG_WAITALL);
}

int xgetpeeraddr(sock_t socket, char *address, in_port_t *port)
{
	struct sockaddr_in client_sockaddr;
	socklen_t len[1] = { sizeof(client_sockaddr) };
	if (xgetpeername(socket, (struct sockaddr *)&client_sockaddr, len)) {
		return -1;
	}
	(void)inet_ntop(AF_INET, &client_sockaddr.sin_addr, address, INET_ADDRSTRLEN);
	*port = ntohs(client_sockaddr.sin_port);
	return 0;
}

bool xport_valid(char *arg)
{
	long port = strtol(arg, NULL, 10);
	if (port < 0 || port > 65535) {
		xwarn("Port cannot be %s\n", port < 0 ? "negative" : "greater than 65535");
		xwarn("Using default port: 2315\n");
		return false;
	}
	return true;
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
	size_t line_length = 64;
	char *line = xmalloc(line_length);
	if (!line) {
		goto error;
	}

	size_t read_length = 0;
	while (1) {
		if (read_length == line_length - 2) {
			line[read_length] = 0;
			if (line_length * 2 < line_length) {
				goto error;
			}

			// xrealloc frees original memory if reallocating fails
			if (!(line = xrealloc(line, line_length *= 2))) { 
				goto error;
				return;
			}
			
		}
	
		int c = fgetc(stream);
		if (c == EOF) {
			goto eol;
		}

		line[read_length] = (char)c;
		if (c == '\n') {
		eol:
			if (!read_length) {
				xfree(line);
				goto error;
			}
			line[read_length] = '\0';
			break;
		}
		read_length++;
	}

	*message = line;
	*message_length = read_length;
	return;

error:
	*message = NULL;
	*message_length = 0;
}

char *xprompt(const char *prompt_msg, const char *error_msg, size_t *len)
{
	char *line;
	size_t line_length;
	
	while (1) {
		line = NULL;
		line_length = 0;

		do {
			printf("%s", prompt_msg);
			fflush(stdout);
			xgetline(&line, &line_length, stdin);
		} while (!line_length);

		if (line_length > *len) {
			xwarn("Maximum %s length is %zu bytes", error_msg, *len);
			xfree(line);
			continue;
		}
		break;
	}
	*len = line_length;
	return line;
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

char *xbasename(char *path)
{
	if (!path || !*path) {
		return ".";
	}

	size_t i = strlen(path) - 1;

	for (; i && path[i] == '/'; i--) {
		path[i] = '\0';
	}
	for (; i && path[i - 1] != '/'; i--) { };

	return &path[i];
}