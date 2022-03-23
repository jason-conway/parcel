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
	fprintf(stderr, "\n>\033[31m Application aborted\n\033[0m");
	exit(EXIT_FAILURE);
}

static inline void fatal(const char *msg)
{
	xprintf(RED, "server error: %s\n", msg);
	exit(EXIT_FAILURE);
}

void configure_server(server_t *srv, const char *port)
{
	if (xstartup()) {
		fatal("xstartup()");
	}
	
	
	xprintf(MAG, "parceld: Parcel Daemon v0.9.1\n");
	printf("Interfaces:\n");
	if (xgetifaddrs()) {
		fatal("xgetifaddrs()");
	}

	printf("Note: this system supports a maximum of \033[39;49;1m%u\033[0m connections\n", MAX_CONNECTIONS);

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
		if (xsocket(&srv->sockets[0], node->ai_family, node->ai_socktype, node->ai_protocol) < 0) {
			continue;
		}
		if (xsetsockopt(srv->sockets[0], SOL_SOCKET, SO_REUSEADDR, opt, sizeof(*opt)) < 0) {
			fatal("setsockopt()");
		}
		if (bind(srv->sockets[0], node->ai_addr, node->ai_addrlen) < 0) {
			(void)xclose(srv->sockets[0]);
			continue;
		}
		break;
	}

	if (!node) {
		fatal("Unable to bind to socket");
	}
	freeaddrinfo(ai);

	if (listen(srv->sockets[0], MAX_QUEUE) < 0) {
		(void)xclose(srv->sockets[0]);
		fatal("listen()");
	}

	FD_ZERO(&srv->descriptors);
	FD_SET(srv->sockets[0], &srv->descriptors);
	srv->descriptor_count = xfd_init_count(srv->sockets[0]);
	srv->connection_count = 0;

	xgetrandom(srv->server_key, 32);
	xprintf(GRN, "Daemon started...\n");
}

static size_t socket_index(server_t *srv, sock_t socket)
{
	for (size_t i = 1; i < MAX_CONNECTIONS; i++) {
		if (srv->sockets[i] == socket) {
			return i;
		}
	}
	return 0;
}

static bool add_client(server_t *srv)
{
	struct sockaddr_storage client_sockaddr;
	socklen_t len[1] = { sizeof(client_sockaddr) };
	sock_t new_client;
	if (xaccept(&new_client, srv->sockets[0], (struct sockaddr *)&client_sockaddr, len) < 0) {
		return false;
	}
	else {
		FD_SET(new_client, &srv->descriptors); // Add descriptor to set and update max
		srv->descriptor_count = xfd_count(new_client, srv->descriptor_count);
		srv->connection_count++;
	}

	// Copy new connection's socket to the next free slot
	for (size_t i = 1; i < MAX_CONNECTIONS; i++) {
		if (!srv->sockets[i]) {
			srv->sockets[i] = new_client;
			printf("Connection %zu added in slot %zu\n", srv->connection_count, i);
			break;
		}
	}

	if (two_party_server(new_client, srv->server_key)) {
		return false;
	}
	return true;
}

static int transfer_message(server_t *srv, size_t sender_index, msg_t *msg)
{
	for (size_t i = 1; i <= srv->connection_count; i++) {
		if (i == sender_index) {
			continue;
		}
		if (xsendall(srv->sockets[i], msg->data, msg->length) < 0) {
			return -1;
		}
	}
	return 0;
}

static int recv_client(server_t *srv, size_t sender_index)
{
	msg_t msg;
	memset(&msg, 0, sizeof(msg_t));

	if ((msg.length = xrecv(srv->sockets[sender_index], msg.data, MAX_CHUNK, 0)) <= 0) {
		if (msg.length) {
			// fprintf(stderr, ">\033[33m Connection error\033[0m\n");
			xprintf(YEL, "Connection error\n");
		}
		else {
			struct sockaddr_in client_sockaddr;
			socklen_t len[1] = { sizeof(client_sockaddr) };
			if (xgetpeername(srv->sockets[sender_index], (struct sockaddr *)&client_sockaddr, len)) {
				fatal("xgetpeername()");
			}
			char address[INET_ADDRSTRLEN];
			(void)inet_ntop(AF_INET, &client_sockaddr.sin_addr, address, INET_ADDRSTRLEN);

			printf("Disconnected from %s port %d\n", address, ntohs(client_sockaddr.sin_port));
		}

		FD_CLR(srv->sockets[sender_index], &srv->descriptors);
		(void)xclose(srv->sockets[sender_index]);

		// Replace this slot with the ending slot
		if (srv->connection_count == 1) {
			srv->sockets[sender_index] = 0;
		}
		else {
			srv->sockets[sender_index] = srv->sockets[srv->connection_count];
			srv->sockets[srv->connection_count] = 0;
		}

		srv->connection_count--;
		// key_exchange_router(srv->socket, &srv->connections, srv->max_in_set, srv->server_key);
	}
	else {
		if (transfer_message(srv, sender_index, &msg) < 0) {
			fprintf(stderr, "Error broadcasting message\n");
			return -1;
		}
		printf("Fanout message from slot %zu\n", sender_index);
	}
	return 0;
}

int main_thread(void *ctx)
{
	signal(SIGINT, catch_sigint);

	server_t *server = (server_t *)ctx;
	fd_set read_fds;
	FD_ZERO(&read_fds);

	while (1) {
		read_fds = server->descriptors;
		if (select(server->descriptor_count + 1, &read_fds, NULL, NULL, NULL) < 0) {
			fatal("select()");
		}

		for (size_t i = 0; i < server->descriptor_count + 1; i++) {
			sock_t fd;
			if ((fd = xfd_isset(&server->descriptors, &read_fds, i))) {
				if (fd == server->sockets[0]) {
					printf("Pending connection from unknown client\n");
					if (add_client(server)) {
						if (key_exchange_router(server->sockets, server->connection_count, server->server_key)) {
							fatal("key_exchange_router()");
						}
						printf("Connection added successfully\n");
					}
					else {
						fatal("add_client()");
					}
				}
				else {
					const size_t sender_index = socket_index(server, fd);
					if (recv_client(server, sender_index)) {
						fatal("recv_client()");
					}
				}
			}
		}
	}
	return 0;
}
