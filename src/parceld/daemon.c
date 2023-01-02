/**
 * @file daemon.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief The `parcel` Daemon 
 * @version 0.9.2
 * @date 2021-11-21
 *
 * @copyright Copyright (c) 2021 - 2023 Jason Conway. All rights reserved.
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
	// WSAStartup (Windows)
	if (xstartup()) {
		xalert("xstartup()");
		return -1;
	}

	if (!(ctx->sockets.sfds = xcalloc(sizeof(sock_t) * ctx->sockets.max_nsfds))) {
		xalert("xcalloc()");
		return -1;
	}

	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
		.ai_flags = AI_PASSIVE
	};

	struct addrinfo *ai = NULL;
	if (xgetaddrinfo(NULL, ctx->server_port, &hints, &ai)) {
		xalert("xgetaddrinfo()");
		return -1;
	}

	struct addrinfo *node = NULL;
	int opt[] = { 1 };
	for (node = ai; node; node = node->ai_next) {
		if (xsocket(&ctx->sockets.sfds[0], node->ai_family, node->ai_socktype, node->ai_protocol) < 0) {
			continue;
		}
		if (xsetsockopt(ctx->sockets.sfds[0], SOL_SOCKET, SO_REUSEADDR, opt, sizeof(*opt)) < 0) {
			xalert("setsockopt()");
			return -1;
		}
		if (bind(ctx->sockets.sfds[0], node->ai_addr, node->ai_addrlen) < 0) {
			(void)xclose(ctx->sockets.sfds[0]);
			continue;
		}
		break;
	}

	if (!node) {
		xalert("Unable to bind to socket");
		return -1;
	}
	freeaddrinfo(ai);

	if (listen(ctx->sockets.sfds[0], MAX_QUEUE) < 0) {
		(void)xclose(ctx->sockets.sfds[0]);
		xalert("listen()");
		return -1;
	}

	FD_ZERO(&ctx->descriptors.fds);
	FD_SET(ctx->sockets.sfds[0], &ctx->descriptors.fds);
	ctx->descriptors.nfds = xfd_init_count(ctx->sockets.sfds[0]);

	// Collect entropy for initial server key
	if (xgetrandom(ctx->server_key, KEY_LEN) < 0) {
		return -1;
	}
	return 0;
}

// Return `i` such that `srv`->sockets[i] == `socket`
static size_t socket_index(server_t *srv, sock_t socket)
{
	for (size_t i = 1; i <= srv->sockets.max_nsfds; i++) {
		if (srv->sockets.sfds[i] == socket) {
			debug_print("Got socket index %zu\n", i);
			return i;
		}
	}
	return 0;
}

static int add_client(server_t *srv)
{
	struct sockaddr_storage client_sockaddr;
	socklen_t len[] = { sizeof(struct sockaddr_storage) };
	sock_t new_client;
	if (xaccept(&new_client, srv->sockets.sfds[0], (struct sockaddr *)&client_sockaddr, len) < 0) {
		debug_print("%s\n", "Could not accept new client");
		return -1;
	}

	if (srv->sockets.nsfds + 1 == srv->sockets.max_nsfds) {
		xwarn("Daemon at full capacity... rejecting new connection\n");
		return 1;
	}

	FD_SET(new_client, &srv->descriptors.fds); // Add descriptor to set and update max
	srv->descriptors.nfds = xfd_count(new_client, srv->descriptors.nfds);
	srv->sockets.nsfds++;

	char address[INET_ADDRSTRLEN];
	in_port_t port;
	if (xgetpeeraddr(new_client, address, &port) < 0) {
		debug_print("%s\n", "Could not get human-readable IP for new client");
		return -1;
	}

	// Copy new connection's socket to the next free slot
	debug_print("%s\n", "Add socket to empty slot");
	for (size_t i = 1; i < srv->sockets.max_nsfds; i++) {
		debug_print("Slot %zu %s\n", i, !srv->sockets.sfds[i] ? "free" : "in use");
		if (!srv->sockets.sfds[i]) {
			srv->sockets.sfds[i] = new_client;
			debug_print("Connection from %s port %u added to slot %zu\n", address, port, i);
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
	for (size_t i = 1; i <= srv->sockets.nsfds; i++) {
		if (i == sender_index) {
			continue;
		}
		// debug_print("Sending to socket %zu\n", i);
		if (xsendall(srv->sockets.sfds[i], msg->data, msg->length) < 0) {
			return -1;
		}
	}
	return 0;
}

static int disconnect_client(server_t *ctx, size_t client_index)
{
	FD_CLR(ctx->sockets.sfds[client_index], &ctx->descriptors.fds);
	const int closed = xclose(ctx->sockets.sfds[client_index]);

	// Replace this slot with the ending slot
	if (ctx->sockets.nsfds == 1) {
		ctx->sockets.sfds[client_index] = 0;
	}
	else {
		ctx->sockets.sfds[client_index] = ctx->sockets.sfds[ctx->sockets.nsfds];
		ctx->sockets.sfds[ctx->sockets.nsfds] = 0;
	}
	ctx->sockets.nsfds--;
	return closed;
}

static int recv_client(server_t *srv, size_t sender_index)
{
	msg_t *msg = xcalloc(sizeof(msg_t));
	if (!msg) {
		xalert("xcalloc()\n");
		return -1;
	}
	
	if ((msg->length = xrecv(srv->sockets.sfds[sender_index], msg->data, sizeof(msg->data), 0)) <= 0) {
		if (msg->length) {
			xwarn("Client %zu disconnected improperly\n", sender_index);
		}
		else {
			char address[INET_ADDRSTRLEN];
			in_port_t port;
			if (xgetpeeraddr(srv->sockets.sfds[sender_index], address, &port) < 0) {
				xwarn("Unable to determine IP and port of client %zu, despite proper disconnect\n", sender_index);
			}
			debug_print("Connection from %s port %d ended\n", address, port);
		}

		if (disconnect_client(srv, sender_index)) {
			xalert("Error closing socket\n");
			xfree(msg);
			return -1;
		}

		debug_print("Active connections: %zu\n", srv->sockets.nsfds);
		
		if (n_party_server(srv->sockets.sfds, srv->sockets.nsfds, srv->server_key)) {
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
		"=> %zu\n"
		"\033[1mPublicly accessible at:\033[0m\n"
		"=> %s:%s\n"
		"\033[1mLocally accessible at:\033[0m\n"
	};

	char *public_ip = xgetpublicip();
	printf(header, ctx->sockets.max_nsfds, public_ip ? public_ip : "error", ctx->server_port);
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

	for (;;) {
		read_fds = server->descriptors.fds;
		if (select(server->descriptors.nfds + 1, &read_fds, NULL, NULL, NULL) < 0) {
			xalert("n_party_server()\n");
			return -1;
		}

		for (size_t i = 0; i <= server->descriptors.nfds; i++) {
			sock_t fd;
			if ((fd = xfd_isset(&server->descriptors.fds, &read_fds, i))) {
				if (fd == server->sockets.sfds[0]) {
					debug_print("%s\n", "Pending connection from unknown client");
					switch (add_client(server)) {
						case -1:
							xalert("n_party_server()\n");
							return -1;
						case 1:
							debug_print("%s\n", "Incoming connection was rejected");
							goto jmpout;
						case 0:
							if (n_party_server(server->sockets.sfds, server->sockets.nsfds, server->server_key)) {
								xalert("n_party_server()\n");
								return -1;
							}
							debug_print("%s\n", "Connection added successfully");
							break;
					}
				}
				else {
					const size_t sender_index = socket_index(server, fd);
					if (recv_client(server, sender_index)) {
						xalert("recv_client()\n");
						return -1;
					}
				}
				jmpout:;
			}
		}
	}
	return 0;
}
