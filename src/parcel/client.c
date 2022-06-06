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

void fatal(client_t *ctx, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);

	(void)fprintf(stderr, "%s", "\033[0;31m");
	(void)vfprintf(stderr, format, ap);
	(void)fprintf(stderr, "\033[0m");
	va_end(ap);

	xclose(ctx->socket);
	xfree(ctx);
	exit(EXIT_FAILURE);
}

void memcpy_locked(pthread_mutex_t *lock, void *dest, void *src, size_t length)
{
	pthread_mutex_lock(lock);
		memcpy(dest, src, length);
	pthread_mutex_unlock(lock);
}

static void create_prefix(const char *username, char *prompt)
{
	size_t username_length = strlen(username);
	memcpy(prompt, username, username_length);
	memcpy(&prompt[username_length], ": ", 3);
}

static void disp_username(const char *username)
{
	char msg_prefix[USERNAME_MAX_LENGTH + 3];
	create_prefix(username, msg_prefix);
	(void)xwrite(STDOUT_FILENO, msg_prefix, strlen(msg_prefix));
	fflush(stdout);
}

static ssize_t send_encrypted_message(sock_t socket, uint64_t type, void *data, size_t length, const uint8_t *key)
{
	wire_t *wire = init_wire(data, type, &length);
	encrypt_wire(wire, key);
	if (xsendall(socket, wire, length) < 0) {
		xfree(wire);
		xalert("xsendall()\n");
		return -1;
	}
	xfree(wire);
	return length;
}

int announce_connection(client_t *ctx)
{
	char *msg = xstrcat(3, "\033[1m", ctx->username, " is online\033[0m");
	if (!msg) {
		return -1;
	}

	if (send_encrypted_message(ctx->socket, TYPE_TEXT, msg, strlen(msg) + 1, ctx->keys.session) < 0) {
		xfree(msg);
		return -1;
	}

	xfree(msg);

	ctx->conn_announced = 1;
	return 0;
}

int send_thread(void *ctx)
{
	client_t client;
	client_t *client_ctx = (client_t *)ctx;
	memcpy_locked(&client_ctx->mutex_lock, &client, client_ctx, sizeof(client_t));

	while (1) {

		size_t length = 0;

		char prompt[USERNAME_MAX_LENGTH + 3];
		create_prefix(client.username, prompt);
		char *plaintext = xprompt(prompt, "text", &length);

		if (!plaintext) {
			fatal(client_ctx, "xmalloc()\n");
		}

		memcpy_locked(&client_ctx->mutex_lock, &client, client_ctx, sizeof(client_t));
		enum command_id id = CMD_NONE;

		switch (parse_input(&client, &id, &plaintext, &length)) {
			case SEND_NONE:
				break;
			case SEND_TEXT:
				if (send_encrypted_message(client.socket, TYPE_TEXT, plaintext, length, client.keys.session) < 0) {
					// handle
				}
				break;
			case SEND_FILE:
				if (send_encrypted_message(client.socket, TYPE_FILE, plaintext, length, client.keys.session) < 0) {
					// handle
				}
				break;
			default:
				xfree(plaintext);
				fatal(client_ctx, "parse_input()\n");
		}

		xfree(plaintext);

		switch (id) {
			case CMD_EXIT:
				exit(EXIT_SUCCESS);
			case CMD_USERNAME:
				memcpy_locked(&client_ctx->mutex_lock, client_ctx->username, client.username, USERNAME_MAX_LENGTH);
				break;
			default:
				break;
		}

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

	if (decrypt_wire(_wire, &wire_size, ctx->keys.session)) {
		return -1;
	}
	wire = _wire;
	return 0;
}

wire_t *recv_new_wire(sock_t socket, size_t *wire_size)
{
	wire_t *wire = new_wire();
	if (!wire) {
		return NULL;
	}

	const ssize_t bytes_recv = xrecv(socket, wire, DATA_LEN_MAX, 0);
	if (bytes_recv < 0) {
		return xfree(wire);
	}
	*wire_size = bytes_recv;
	return wire;
}

void *recv_thread(void *ctx)
{
	client_t client;
	client_t *client_ctx = (client_t *)ctx;
	memcpy(&client, client_ctx, sizeof(client_t));

	while (1) {
		size_t bytes_recv;
		wire_t *wire = recv_new_wire(client.socket, &bytes_recv);
		if (!wire) {
			fatal(client_ctx, "malloc() failure when initializing new wire\n");
		}

		memcpy_locked(&client_ctx->mutex_lock, &client, client_ctx, sizeof(client_t));

		size_t length = bytes_recv;
		switch (decrypt_wire(wire, &length, client.keys.session)) {
			case WIRE_INVALID_KEY:
				if (decrypt_wire(wire, &length, client.keys.ctrl)) {
					xfree(wire);
					fatal(client_ctx, "> Recveived corrupted control key from server\n");
				}
				break;
			case WIRE_PARTIAL:
				debug_print("%s\n", "> wire is partial, continue receiving");

				if (recv_remaining(&client, wire, bytes_recv, length)) {
					xfree(wire);
					fatal(client_ctx, "recv_remaining()\n");
				}
				debug_print("%s\n", "> received remainder of wire");
				break;
			case WIRE_CMAC_ERROR:
				debug_print("%s\n", "> CMAC error");
				goto recv_error;
			case WIRE_OK:
				break;
		}

		switch (wire_get_type(wire)) {
			case TYPE_CTRL:
				switch (proc_ctrl(&client, wire->data)) {
					case CTRL_EXIT:
						xfree(wire);
						xalert("> Received EXIT\n");
						// TODO: proper shutdown
						exit(EXIT_SUCCESS);
					case CTRL_DHKE:
						memcpy_locked(&client_ctx->mutex_lock, &client_ctx->keys, &client.keys, sizeof(parcel_keys_t));
						memcpy_locked(&client_ctx->mutex_lock, &client_ctx->conn_announced, &client.conn_announced, sizeof(bool));
						break;
					default:
						xfree(wire);
						fatal(client_ctx, "proc_ctrl()\n");
				}
				break;
			case TYPE_FILE:
				if (proc_file(wire->data)) {
					xfree(wire);
					fatal(client_ctx, "proc_file()\n");
				}
				break;
			case TYPE_TEXT:
				proc_text(wire->data);
				disp_username(client.username);
				break;
			default:
				xfree(wire);
				fatal(client_ctx, "unknown wire type\n");
		}

	recv_error:
		xfree(wire);
		struct timespec ts = { .tv_nsec = 1000000 };
		(void)nanosleep(&ts, NULL);
	}
}

int connect_server(client_t *client, const char *ip, const char *port)
{
	if (xstartup()) {
		xalert("xstartup()\n");
		return -1;
	}

	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};

	struct addrinfo *srv_addr;
	if (xgetaddrinfo(ip, port, &hints, &srv_addr)) {
		xalert("xgetaddrinfo()\n");
		return -1;
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
		xalert("could not connect to server\n");
		return -1;
	}

	freeaddrinfo(srv_addr);

	if (two_party_client(client->socket, client->keys.ctrl)) {
		(void)xclose(client->socket);
		xalert("Failed to perform initial key exchange with server\n");
		return -1;
	}

	printf(">\033[32m Connected to server\033[0m\n");
	return 0;
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
