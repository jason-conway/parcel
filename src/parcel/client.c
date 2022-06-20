/**
 * @file client.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief The `parcel` Client
 * @version 0.9.2
 * @date 2021-11-08
 *
 * @copyright Copyright (c) 2021-2022 Jason Conway. All rights reserved.
 *
 */

#include "client.h"

static void create_prefix(const char *username, char *prompt)
{
	size_t username_length = strlen(username);
	memcpy(prompt, username, username_length);
	memcpy(&prompt[username_length], ": ", 3);
}

void disp_username(const char *username)
{
	char msg_prefix[USERNAME_MAX_LENGTH + 3];
	create_prefix(username, msg_prefix);
	(void)xwrite(STDOUT_FILENO, msg_prefix, strlen(msg_prefix));
	fflush(stdout);
}

/**
 * @return returns number of bytes sent on success, otherwise a negative value is returned
 */
static ssize_t send_encrypted_message(sock_t socket, uint64_t type, void *data, size_t length, const uint8_t *key)
{
	wire_t *wire = init_wire(data, type, &length);
	encrypt_wire(wire, key);
	if (xsendall(socket, wire, length) < 0) {
		xfree(wire);
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
	client_t *client_ctx = ctx;
	xmemcpy_locked(&client_ctx->mutex_lock, &client, client_ctx, sizeof(client_t));
	int status = 0;

	while (1) {
		char prompt[USERNAME_MAX_LENGTH + 3];
		create_prefix(client.username, prompt);

		size_t length = 0;
		char *plaintext = xprompt(prompt, "text", &length); // xprompt() will never return null by design

		xmemcpy_locked(&client_ctx->mutex_lock, &client, client_ctx, sizeof(client_t));
		enum command_id id = CMD_NONE;

		switch (parse_input(&client, &id, &plaintext, &length)) {
			case SEND_NONE:
				break;
			case SEND_TEXT:
				if (send_encrypted_message(client.socket, TYPE_TEXT, plaintext, length, client.keys.session) < 0) {
					xalert("Error sending encrypted text\n");
					status = -1;
				}
				break;
			case SEND_FILE:
				if (send_encrypted_message(client.socket, TYPE_FILE, plaintext, length, client.keys.session) < 0) {
					xalert("Error sending encrypted file\n");
					status = -1;
				}
				break;
		}

		xfree(plaintext);

		switch (id) {
			default:
				if (!status) {
					break;
				} // fallthrough
			case CMD_EXIT:
				goto cleanup;
			case CMD_USERNAME:
				xmemcpy_locked(&client_ctx->mutex_lock, client_ctx->username, client.username, USERNAME_MAX_LENGTH);
				break;
		}

		struct timespec ts = { .tv_nsec = 1000000 };
		(void)nanosleep(&ts, NULL);
	}

	cleanup:
		xmemcpy_locked(&client_ctx->mutex_lock, &client_ctx->kill_threads, &(bool[]){ 1 }, sizeof(bool));
		xclose(client.socket);
		return status;
}

static int recv_remaining(client_t *ctx, wire_t **wire, size_t bytes_recv, size_t bytes_remaining)
{
	size_t wire_size = bytes_recv + bytes_remaining;
	*wire = xrealloc(*wire, wire_size);
	if (!*wire) {
		return -1;
	}

	wire_t *dest = *wire;
	for (size_t i = 0; i < bytes_remaining;) {
		ssize_t received = xrecv(ctx->socket, &dest->data[bytes_recv - sizeof(wire_t)] + i, bytes_remaining - i, 0);
		switch (received) {
			case -1:
				return -1;
			default:
				i += received;
		}
	}

	return decrypt_wire(*wire, &wire_size, ctx->keys.session) ? -1 : 0;
}

wire_t *recv_new_wire(client_t *ctx, size_t *wire_size)
{
	wire_t *wire = new_wire();
	if (!wire) {
		return NULL;
	}

	const ssize_t bytes_recv = xrecv(ctx->socket, wire, DATA_LEN_MAX, 0);
	if (bytes_recv < 0) {
		return xfree(wire);
	}

	// Refresh any changes to shared context that may have occured while blocking on recv
	xmemcpy_locked(&ctx->shctx->mutex_lock, ctx, ctx->shctx, sizeof(client_t));

	*wire_size = bytes_recv;
	return wire;
}

/**
 * @brief Decrypts an encrypted wire
 * 
 * @param ctx Client context
 * @param wire Wire received
 * @param bytes_recv Number of bytes received
 * @return Returns length of the wire data section, negative on error
 */
static ssize_t decrypt_received_message(client_t *ctx, wire_t *wire, size_t bytes_recv)
{
	size_t length = bytes_recv;
	switch (decrypt_wire(wire, &length, ctx->keys.session)) {
		case WIRE_INVALID_KEY:
			if (decrypt_wire(wire, &length, ctx->keys.ctrl)) {
				xalert("> Recveived corrupted control key from server\n");
				return -1;
			}
			break;
		case WIRE_PARTIAL:
			debug_print("> Received %zu bytes but header specifies %zu bytes total\n", bytes_recv, length + bytes_recv);
			if (recv_remaining(ctx, &wire, bytes_recv, length)) {
				xalert("recv_remaining()\n");
				return -1;
			}
			debug_print("%s\n", "> received remainder of wire");
			break;
		case WIRE_CMAC_ERROR:
			debug_print("%s\n", "> CMAC error");
			return -1;
		case WIRE_OK:
			break;
	}
	return length; // All good
}

void *recv_thread(void *ctx)
{
	client_t client;
	client_t *client_ctx = ctx;
	memcpy(&client, client_ctx, sizeof(client_t));
	
	int status = 0;

	while (1) {
		size_t bytes_recv = 0;
		wire_t *wire = recv_new_wire(&client, &bytes_recv);
		if (!wire) {
			status = client.kill_threads ? 0 : -1;
			break;
		}
		
		if (decrypt_received_message(&client, wire, bytes_recv) < 0) {
			goto recv_error;
		}
		
		// TODO: revisit
		switch(proc_type(&client, wire)) {
			case TYPE_TEXT:
				break;
			case TYPE_CTRL:
				break;
			case TYPE_FILE:
				break;
			default:
				status = -1;
				break;
		}

	recv_error:
		xfree(wire);
		struct timespec ts = { .tv_nsec = 1000000 };
		(void)nanosleep(&ts, NULL);
		
		if (client.kill_threads || status) {
			break;
		}
	}
	// end_thread:
		xclose(client.socket);
		xfree(client_ctx);
		return NULL;
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

	xprintf(GRN, BOLD, "=== Connected to server ===\n");
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
