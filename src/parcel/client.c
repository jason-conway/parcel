/**
 * @file client.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief The `parcel` Client
 * @version 0.9.1
 * @date 2021-11-08
 *
 * @copyright Copyright (c) 2021-2022 Jason Conway. All rights reserved.
 *
 */

#include "client.h"

void fatal(const char *msg)
{
	fprintf(stderr, ">\033[31m fatal error: %s\n\033[0m", msg);
	exit(EXIT_FAILURE);
}

void error(sock_t socket, const char *msg)
{
	fprintf(stderr, ">\033[31m parcel error: %s\n\033[0m", msg);
	(void)xclose(socket);
	exit(EXIT_FAILURE);
}

void refresh_local_ctx(client_t *shared, client_t *local)
{
	pthread_mutex_lock(&shared->mutex_lock);
		memcpy(local, shared, sizeof(client_t));
	pthread_mutex_unlock(&shared->mutex_lock);
}

void update_ctx_recv(client_t *shared, client_t *local)
{
	pthread_mutex_lock(&shared->mutex_lock);
	memcpy(shared->ctrl_key, local->ctrl_key, KEY_LEN);
	memcpy(shared->session_key, local->session_key, KEY_LEN);
	memcpy(shared->fingerprint, local->fingerprint, FINGERPRINT_LENGTH);
	pthread_mutex_unlock(&shared->mutex_lock);
}

void update_ctx_send(client_t *shared, client_t *local)
{
	pthread_mutex_lock(&shared->mutex_lock);
	if (memcmp(shared->username, local->username, USERNAME_MAX_LENGTH)) {
		memcpy(shared->username, local->username, USERNAME_MAX_LENGTH);
	}
	pthread_mutex_unlock(&shared->mutex_lock);
}

static void send_encrypted_message(sock_t socket, uint64_t type, void *data, size_t length, const uint8_t *key)
{
	size_t len = length;
	wire_t *wire = init_wire(data, type, &len);
	encrypt_wire(wire, key);
	if (xsendall(socket, wire, len) < 0) {
		xfree(wire);
		error(socket, "xsendall()");
	}
	xfree(wire);
}

void send_connection_status(client_t *ctx, bool leave)
{
	char msg[USERNAME_MAX_LENGTH + 40] = { 0 };
	static const char template[] = "\033[3%dm%s has %s the server.\033[0m";

	if (snprintf(msg, sizeof(msg), template, leave ? 5 : 2, ctx->username, leave ? "left" : "joined") < 0) {
		error(ctx->socket, "send()");
	}
	send_encrypted_message(ctx->socket, TYPE_TEXT, msg, strlen(msg) + 1, ctx->session_key);

	if (leave) {
		if (xclose(ctx->socket)) {
			error(ctx->socket, "close()");
		}
		exit(EXIT_SUCCESS);
	}
}

int send_thread(void *ctx)
{
	client_t client;
	client_t *client_ctx = (client_t *)ctx;
	refresh_local_ctx(client_ctx, &client);

	while (1) {
		char *plaintext = NULL;
		size_t length = 0;

		// Don't send an empty message
		do {
			printf("%s: ", client.username);
			fflush(stdout);
			xgetline(&plaintext, &length, stdin);
		} while (!length);

		if (!plaintext) {
			error(client.socket, "xmalloc()");
		}

		refresh_local_ctx(client_ctx, &client);

		switch (parse_input(&client, &plaintext, &length)) {
			case SEND_NONE:
				break;
			case SEND_TEXT:
				send_encrypted_message(client.socket, TYPE_TEXT, plaintext, length, client.session_key);
				break;
			case SEND_FILE:
				send_encrypted_message(client.socket, TYPE_FILE, plaintext, length, client.session_key);
				break;
			default:
				xfree(plaintext);
				error(client.socket, "parse_input()");
		}

		update_ctx_send(client_ctx, &client);

		xfree(plaintext);

		struct timespec ts = { .tv_nsec = 1000000 };
		(void)nanosleep(&ts, NULL);
	}
}

static int recv_remaining(client_t *ctx, wire_t *wire, size_t bytes_recv, size_t bytes_remaining)
{
	size_t wire_size = bytes_recv + bytes_remaining;
	wire_t *_wire = xmalloc(wire_size);
	if (!_wire) {
		return -1;
	}
	memcpy(_wire, wire, bytes_recv);
	xfree(wire);
	if (xrecvall(ctx->socket, &_wire[bytes_recv], bytes_remaining)) {
		xfree(_wire);
		return -1;
	}

	if (_decrypt_wire(_wire, &wire_size, ctx->session_key)) {
		return -1;
	}
	wire = _wire;
	return 0;
}

void *recv_thread(void *ctx)
{
	client_t client;
	client_t *client_ctx = (client_t *)ctx;
	refresh_local_ctx(client_ctx, &client);

	while (1) {
		wire_t *wire = new_wire();
		if (!wire) {
			fatal("malloc() failure when initializing new wire");
		}

		const ssize_t bytes_recv = xrecv(client.socket, wire, DATA_LEN_MAX, 0);
		if (bytes_recv < 0) {
			xfree(wire);
			error(client.socket, "recv()");
		}

		size_t length = bytes_recv;

		refresh_local_ctx(client_ctx, &client);

		switch (_decrypt_wire(wire, &length, client.session_key)) {
			case WIRE_INVALID_KEY:
				if (_decrypt_wire(wire, &length, client.ctrl_key)) {
					xfree(wire);
					fatal("_decrypt_wire()");
				}
				break;
			case WIRE_PARTIAL:
				if (recv_remaining(&client, wire, bytes_recv, length)) {
					xfree(wire);
					fatal("recv_remaining()");
				}
				break;
			case WIRE_CMAC_ERROR:
				xfree(wire);
				fatal("_decrypt_wire()");
			case WIRE_OK:
				break;
		}

		switch (wire_get_raw(wire->type)) {
			case TYPE_CTRL:
				if (proc_ctrl(&client, wire->data)) {
					xfree(wire);
					fatal("proc_ctrl()");
				}
				update_ctx_recv(client_ctx, &client);
				break;
			case TYPE_FILE:
				if (proc_file(wire->data)) {
					xfree(wire);
					fatal("proc_file()");
				}
				break;
			case TYPE_TEXT:
				proc_text(&client, wire->data);
				break;
		}

		xfree(wire);

		struct timespec ts = { .tv_nsec = 1000000 };
		(void)nanosleep(&ts, NULL);
	}
}

void connect_server(client_t *client, const char *ip, const char *port)
{
	if (xstartup()) {
		fatal("xstartup()");
	}

	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};

	struct addrinfo *srv_addr;
	if (xgetaddrinfo(ip, port, &hints, &srv_addr)) {
		fatal("xgetaddrinfo()");
	}
	struct addrinfo *node = NULL;
	for (node = srv_addr; node; node = node->ai_next) {
		if (xsocket(&client->socket, node->ai_family, node->ai_socktype, node->ai_protocol) < 0) {
			continue;
		}
		if (connect(client->socket, node->ai_addr, node->ai_addrlen)) {
			(void)xclose(client->socket);
			continue;
		}
		break;
	}

	if (!node) {
		fatal("could not connect to server");
	}

	freeaddrinfo(srv_addr);

	if (two_party_client(client->socket, client->ctrl_key, client->fingerprint)) {
		error(client->socket, "key_exchange()");
	}

	printf(">\033[32m Connected to server\033[0m\n");

	// TODO: This needs to be done after a key exchange sequence
	// send_connection_status(client, false);
}

void prompt_args(char *address, char *username)
{
	char *args[] = { &address[0], &username[0] };

	size_t lengths[] = {
		ADDRESS_MAX_LENGTH,
		USERNAME_MAX_LENGTH
	};

	char *prompts[] = {
		"> Enter server address: ",
		"> Enter username: "
	};

	char *arg_name[] = {
		"address",
		"username"
	};

	for (size_t i = 0; i < 2; i++) {
		if (!*args[i]) {
			size_t len = lengths[i];
			char *str = xprompt(prompts[i], arg_name[i], &len);
			memcpy(args[i], str, len);
			xfree(str);
		}
	}
}
