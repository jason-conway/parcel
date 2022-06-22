/**
 * @file daemon.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief The `parcel` Daemon 
 * @version 0.9.2
 * @date 2021-11-21
 *
 * @copyright Copyright (c) 2021-2022 Jason Conway. All rights reserved.
 *
 */

#include "daemon.h"

void catch_sigint(int sig)
{
	(void)sig;
	xalert("\nApplication aborted\n");
	exit(EXIT_FAILURE);
}

int init_daemon(server_t *ctx)
{
	if (xstartup()) {
		xalert("xstartup()");
		return -1;
	}

	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_flags = AI_PASSIVE
	};

	struct addrinfo *ai;
	if (xgetaddrinfo(NULL, ctx->server_port, &hints, &ai)) {
		xalert("xgetaddrinfo()");
		return -1;
	}

	struct addrinfo *node = NULL;
	int opt[] = { 1 };
	for (node = ai; node; node = node->ai_next) {
		if (xsocket(&ctx->sockets[DAEMON_SOCKET], node->ai_family, node->ai_socktype, node->ai_protocol) < 0) {
			continue;
		}
		if (xsetsockopt(ctx->sockets[DAEMON_SOCKET], SOL_SOCKET, SO_REUSEADDR, opt, sizeof(*opt)) < 0) {
			xalert("setsockopt()");
			return -1;
		}
		if (bind(ctx->sockets[DAEMON_SOCKET], node->ai_addr, node->ai_addrlen) < 0) {
			(void)xclose(ctx->sockets[DAEMON_SOCKET]);
			continue;
		}
		break;
	}

	if (!node) {
		xalert("Unable to bind to socket");
		return -1;
	}
	freeaddrinfo(ai);

	if (listen(ctx->sockets[DAEMON_SOCKET], MAX_QUEUE) < 0) {
		(void)xclose(ctx->sockets[DAEMON_SOCKET]);
		xalert("listen()");
		return -1;
	}

	FD_ZERO(&ctx->descriptors.fds);
	FD_SET(ctx->sockets[DAEMON_SOCKET], &ctx->descriptors.fds);
	ctx->descriptors.nfds = xfd_init_count(ctx->sockets[DAEMON_SOCKET]);

	// Collect entropy for initial server key
	xgetrandom(ctx->server_key, 32);
	return 0;
}

// Return `i` such that `srv`->sockets[i] == `socket`
static size_t socket_index(server_t *srv, sock_t socket)
{
	for (size_t i = 1; i < MAX_CONNECTIONS; i++) {
		if (srv->sockets[i] == socket) {
			debug_print("Got socket index %zu\n", i);
			return i;
		}
	}
	return 0;
}

static int add_client(server_t *srv)
{
	struct sockaddr_storage client_sockaddr;
	socklen_t len[] = { sizeof(client_sockaddr) };
	sock_t new_client;
	if (xaccept(&new_client, srv->sockets[DAEMON_SOCKET], (struct sockaddr *)&client_sockaddr, len) < 0) {
		debug_print("%s\n", "Could not accept new client");
		return -1;
	}
	
	FD_SET(new_client, &srv->descriptors.fds); // Add descriptor to set and update max
	srv->descriptors.nfds = xfd_count(new_client, srv->descriptors.nfds);
	srv->active_connections++;

	char address[INET_ADDRSTRLEN];
	in_port_t port;
	if (xgetpeeraddr(new_client, address, &port) < 0) {
		debug_print("%s\n", "Could not get human-readable IP for new client");
		return -1;
	}

	// Copy new connection's socket to the next free slot
	for (size_t i = 1; i < MAX_CONNECTIONS; i++) {
		if (!srv->sockets[i]) {
			srv->sockets[i] = new_client;
			debug_print("Connection from %s port %u added to slot %zu\n", address, port, i);
			debug_print("Active connections: %zu\n", srv->active_connections);
			break;
		}
	}

	if (two_party_server(new_client, srv->server_key)) {
		debug_print("%s\n", "Successful Diffie-Hellman");
		return -1;
	}
	return 0;
}

static int transfer_message(server_t *srv, size_t sender_index, msg_t *msg)
{
	for (size_t i = 1; i <= srv->active_connections; i++) {
		if (i == sender_index) {
			continue;
		}
		debug_print("Sending to socket %zu\n", i);
		if (xsendall(srv->sockets[i], msg->data, msg->length) < 0) {
			return -1;
		}
	}
	return 0;
}

static int disconnect_client(server_t *ctx, size_t client_index)
{
	FD_CLR(ctx->sockets[client_index], &ctx->descriptors.fds);
	const int closed = xclose(ctx->sockets[client_index]);

	// Replace this slot with the ending slot
	if (ctx->active_connections == 1) {
		ctx->sockets[client_index] = 0;
	}
	else {
		ctx->sockets[client_index] = ctx->sockets[ctx->active_connections];
		ctx->sockets[ctx->active_connections] = 0;
	}
	ctx->active_connections--;
	return closed;
}

static int recv_client(server_t *srv, size_t sender_index)
{
	msg_t *msg = xcalloc(sizeof(msg_t));
	if (!msg) {
		xalert("xcalloc()\n");
		return -1;
	}
	
	if ((msg->length = xrecv(srv->sockets[sender_index], msg->data, sizeof(msg->data), 0)) <= 0) {
		if (msg->length) {
			xwarn("Client %zu disconnected improperly\n", sender_index);
		}
		else {
			char address[INET_ADDRSTRLEN];
			in_port_t port;
			if (xgetpeeraddr(srv->sockets[sender_index], address, &port) < 0) {
				xwarn("Unable to determine IP and port of client %zu, despite proper disconnect\n", sender_index);
			}
			debug_print("Connection from %s port %d ended\n", address, port);
		}

		if (disconnect_client(srv, sender_index)) {
			xalert("Error closing socket\n");
			xfree(msg);
			return -1;
		}

		debug_print("Active connections: %zu\n", srv->active_connections);
		
		if (n_party_server(srv->sockets, srv->active_connections, srv->server_key)) {
			xalert("Catastrophic key exchange failure\n");
			xfree(msg);
			return -1;
		}
	}
	else {
		if (transfer_message(srv, sender_index, msg) < 0) {
			xalert("Error broadcasting message from slot %zu\n", sender_index);
			xfree(msg);
			return -1;
		}
		debug_print("Fanout of slot %zu's message complete\n", sender_index);
	}
	xfree(msg);
	return 0;
}

int display_daemon_info(server_t *ctx)
{
	const char header[] = {
		"\033[32;1m===  parceld " STR(PARCEL_VERSION) "  ===\033[0m\n"
		"\033[1mMaximum active connections:\033[0m\n"
		"=> %u\n"
		"\033[1mPublicly accessible at:\033[0m\n"
		"=> %s:%s\n"
		"\033[1mLocally accessible at:\033[0m\n"
	};

	char *public_ip = xgetpublicip();
	printf(header, MAX_CONNECTIONS, public_ip ? public_ip : "error", ctx->server_port);
	xfree(public_ip);
	
	if (xgetifaddrs("=> ", ctx->server_port)) {
		xalert("Failed to obtain local interfaces\n");
		return -1;
	}
	printf("\033[1mDaemon started...\033[0m\n");
	return 0;
}

int main_thread(void *ctx)
{
	signal(SIGINT, catch_sigint);

	server_t *server = (server_t *)ctx;
	fd_set read_fds;
	FD_ZERO(&read_fds);

	while (1) {
		read_fds = server->descriptors.fds;
		if (select(server->descriptors.nfds + 1, &read_fds, NULL, NULL, NULL) < 0) {
			xalert("n_party_server()\n");
			return -1;
		}

		for (size_t i = 0; i <= server->descriptors.nfds; i++) {
			sock_t fd;
			if ((fd = xfd_isset(&server->descriptors.fds, &read_fds, i))) {
				if (fd == server->sockets[DAEMON_SOCKET]) {
					debug_print("%s\n", "Pending connection from unknown client");
					if (add_client(server)) {
						xalert("n_party_server()\n");
						return -1;
					}

					if (n_party_server(server->sockets, server->active_connections, server->server_key)) {
						xalert("n_party_server()\n");
						return -1;
					}
					debug_print("%s\n", "Connection added successfully");
				}
				else {
					const size_t sender_index = socket_index(server, fd);
					if (recv_client(server, sender_index)) {
						xalert("recv_client()\n");
						return -1;
					}
				}
			}
		}
	}
	return 0;
}
