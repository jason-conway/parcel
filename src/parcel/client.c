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

static void disp_username(const char *username)
{
	printf("%s: ", username);
	fflush(stdout);
}

static void format_prompt(const char *username, char *prompt)
{
	size_t username_length = strlen(username);
	memcpy(prompt, username, username_length);
	memcpy(&prompt[username_length], ": ", 3);
}

void memcpy_locked(pthread_mutex_t *lock, void *dest, void *src, size_t length)
{
	pthread_mutex_lock(lock);
		memcpy(dest, src, length);
	pthread_mutex_unlock(lock);
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
	send_encrypted_message(ctx->socket, TYPE_TEXT, msg, strlen(msg) + 1, ctx->keys.session);

}

int send_thread(void *ctx)
{
	client_t client;
	client_t *client_ctx = (client_t *)ctx;
	memcpy_locked(&client_ctx->mutex_lock, &client, client_ctx, sizeof(client_t));

	while (1) {
		
		size_t length = 0;

		char prompt[USERNAME_MAX_LENGTH + 3];
		format_prompt(client.username, prompt);
		char *plaintext = xprompt(prompt, "text", &length);

		if (!plaintext) {
			error(client.socket, "xmalloc()");
		}

		memcpy_locked(&client_ctx->mutex_lock, &client, client_ctx, sizeof(client_t));
		enum command_id id = CMD_NONE;

		switch (parse_input(&client, &id, &plaintext, &length)) {
			case SEND_NONE:
				break;
			case SEND_TEXT:
				send_encrypted_message(client.socket, TYPE_TEXT, plaintext, length, client.keys.session);
				break;
			case SEND_FILE:
				send_encrypted_message(client.socket, TYPE_FILE, plaintext, length, client.keys.session);
				break;
			default:
				xfree(plaintext);
				error(client.socket, "parse_input()");
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
			fatal("malloc() failure when initializing new wire");
		}

		memcpy_locked(&client_ctx->mutex_lock, &client, client_ctx, sizeof(client_t));
		
		size_t length = bytes_recv;
		switch (decrypt_wire(wire, &length, client.keys.session)) {
			case WIRE_INVALID_KEY:
				if (decrypt_wire(wire, &length, client.keys.ctrl)) {
					xfree(wire);
					fatal("> Recveived corrupted control key from server\n");
				}
				break;
			case WIRE_PARTIAL:
				xwarn("> wire is partial, continue receiving\n");
				if (recv_remaining(&client, wire, bytes_recv, length)) {
					xfree(wire);
					fatal("recv_remaining()");
				}
				xwarn("> received remainder of wire\n");
				break;
			case WIRE_CMAC_ERROR:
				xwarn("> CMAC error\n");
				goto recv_error;
			case WIRE_OK:
				break;
		}

		switch (wire_get_type(wire)) {
			case TYPE_CTRL:
				if (proc_ctrl(&client, wire->data)) {
					goto type_proc_error;
				}
				memcpy_locked(&client_ctx->mutex_lock, &client_ctx->keys, &client.keys, sizeof(parcel_keys_t));
				break;
			case TYPE_FILE:
				if (proc_file(wire->data)) {
					goto type_proc_error;
				}
				break;
			case TYPE_TEXT:
				proc_text(wire->data);
				disp_username(client.username); // TODO: needed?
				break;
			default:
				type_proc_error:
					xfree(wire);
					fatal("proc_ctrl()");
		}

	recv_error:
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

	if (two_party_client(client->socket, client->keys.ctrl)) {
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
