/**
 * @file daemon.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief
 * @version 0.1
 * @date 2021-11-21
 *
 * @copyright Copyright (c) 2021-2022 Jason Conway. All rights reserved.
 *
 */

#include "daemon.h"

void catch_sigint(int sig)
{
	(void)sig;
	fprintf(stderr, "\n> \033[31mApplication aborted\n\033[0m");
	exit(EXIT_FAILURE);
}

static inline void fatal(const char *msg)
{
	fprintf(stderr, "> \033[31mserver error: %s\033[0m\n", msg);
	exit(EXIT_FAILURE);
}

bool add_client(server_t *srv)
{
	struct sockaddr_storage client_sockaddr;
	socklen_t len[1] = { sizeof(client_sockaddr) };
	sock_t new_client;
	if (xaccept(&new_client, srv->socket, (struct sockaddr *)&client_sockaddr, len) < 0) {
		return false;
	}
	else {
		FD_SET(new_client, &srv->connections); // Add descriptor to set and update max
		srv->connection_count = xfd_count(new_client, srv->connection_count);
	}

	if (server_key_exchange(new_client, srv->server_key)) {
		return false;
	}
	return true;
}

static void transfer_message(server_t *srv, sock_t client_socket, msg_t *msg)
{
	for (size_t i = 0; i < srv->connection_count + 1; i++) {
		sock_t fd;
		if ((fd = xfd_inset(&srv->connections, i))) {
			if (fd != srv->socket && fd != client_socket) {
				if (xsend(fd, msg->data, msg->length, 0) < 0) {
					fatal("xsend()");
				}
			}
		}
	}
}

void configure_server(server_t *srv, const char *port)
{
	if (xstartup()) {
		fatal("xstartup()");
	}
	
	printf(">\033[35;49;1m parceld: Parcel Daemon v0.1\033[0m\n");
	printf("> Interfaces:\n");
	if (xgetifaddrs()) {
		fatal("xgetifaddrs()");
	}

	printf("> Note: this system supports a maximum of \033[39;49;1m%u\033[0m connections\n", (unsigned)FD_SETSIZE);

	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_flags = AI_PASSIVE
	};

	struct addrinfo *ai;
	if (xgetaddrinfo(NULL, port, &hints, &ai)) {
		fatal("xgetaddrinfo()");
	}

	struct addrinfo *node = NULL;
	int opt[1] = { 1 };
	for (node = ai; node; node = node->ai_next) {
		if (xsocket(&srv->socket, node->ai_family, node->ai_socktype, node->ai_protocol) < 0) {
			continue;
		}
		if (xsetsockopt(srv->socket, SOL_SOCKET, SO_REUSEADDR, opt, sizeof(*opt)) < 0) {
			fatal("setsockopt()");
		}
		if (bind(srv->socket, node->ai_addr, node->ai_addrlen) < 0) {
			(void)xclose(srv->socket);
			continue;
		}
		break;
	}

	if (!node) {
		fatal("Unable to bind to socket");
	}
	freeaddrinfo(ai);

	if (listen(srv->socket, 10) < 0) {
		(void)xclose(srv->socket);
		fatal("listen()");
	}

	FD_ZERO(&srv->connections);
	FD_SET(srv->socket, &srv->connections);
	srv->connection_count = xfd_init_count(srv->socket);

	xgetrandom(srv->server_key, 32);
	printf(">\033[32m parceld running...\033[0m\n");
}

static void recv_client(server_t *srv, sock_t client_socket)
{
	msg_t msg;
	memset(&msg, 0, sizeof(msg_t));

	if ((msg.length = xrecv(client_socket, msg.data, MAX_CHUNK, 0)) <= 0) {
		if (!msg.length) {
			printf("> Connection left\n");
		}
		else {
			fprintf(stderr, ">\033[33m Connection error\033[0m\n");
		}
		FD_CLR(client_socket, &srv->connections);
		(void)xclose(client_socket);
	}
	else {
		transfer_message(srv, client_socket, &msg);
	}
}

int main_thread(void *ctx)
{
	signal(SIGINT, catch_sigint);

	server_t *server = (server_t *)ctx;
	fd_set read_fds;
	FD_ZERO(&read_fds);

	while (1) {
		read_fds = server->connections;
		if (select(server->connection_count + 1, &read_fds, NULL, NULL, NULL) < 0) {
			fatal("select()");
		}
		
		for (size_t i = 0; i < server->connection_count + 1; i++) {
			sock_t fd;
			if ((fd = xfd_isset(&server->connections, &read_fds, i))) {
				if (fd == server->socket) {
					printf("> Attempting to add new connection...\n");
					printf("> Connection %s\n", add_client(server) ? "successful" : "failed");
				}
				else {
					recv_client(server, fd);
				}
			}
		}
	}
	return 0;
}
