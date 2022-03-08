/**
 * @file client.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief
 * @version 0.1
 * @date 2021-11-08
 *
 * @copyright Copyright (c) 2021-2022 Jason Conway. All rights reserved.
 *
 */

#include "client.h"

static inline void fatal(const char *msg)
{
	fprintf(stderr, ">\033[31m fatal error: %s\n\033[0m", msg);
	exit(EXIT_FAILURE);
}

static inline void error(sock_t socket, const char *msg)
{
	fprintf(stderr, ">\033[31m parcel error: %s\n\033[0m", msg);
	(void)xclose(socket);
	exit(EXIT_FAILURE);
}

static void prepend_username(client_t *ctx, char **message, size_t *message_length)
{
	// Prepend username to the input string
	*message_length += strlen(ctx->username) + 2; // +2 for ": "

	char *msg = malloc(*message_length);
	if (!msg) {
		free(*message);
		*message = NULL;
		return;
	}

	if (snprintf(msg, *message_length, "%s: %s", ctx->username, *message) != (int)*message_length) {
		free(*message);
		*message = NULL;
		return;
	}

	free(*message);
	*message = msg;
	return;
}

static void send_encrypted_message(int socket, char *plaintext, size_t length, const uint8_t *key)
{
	size_t len = length;
	wire_t *wire = init_wire(plaintext, &len);
	encrypt_wire(wire, key);
	if (xsend(socket, wire, len, 0) < 0) {
		return;
	}
	free(wire);
}

static int change_username(client_t *ctx, char **message, size_t *message_length)
{
	char *new_username = NULL;
	size_t new_username_length = 0;
	while (!new_username_length) {
		printf("New username: ");
		fflush(stdout);
		xgetline(&new_username, &new_username_length, stdin);
	}
	new_username[new_username_length - 1] = '\0'; // remove \n

	static const char *template = "\033[33m%s has changed their username to %s\033[0m";
	const size_t msg_length = strlen(template) + new_username_length + strlen(ctx->username) + 1;
	char *msg = malloc(msg_length);
	if (!msg) {
		send_connection_status(ctx, true);
		return -1;
	}
	if (snprintf(msg, msg_length, template, ctx->username, new_username) < 0) {
		send_connection_status(ctx, true);
		return -1;
	}

	*message = msg;
	*message_length = msg_length;
	memset(ctx->username, 0, USERNAME_MAX_LENGTH);
	memcpy(ctx->username, new_username, new_username_length);
	free(new_username);
	return 0;
}

static int is_command(client_t *ctx, char **message, size_t *message_length)
{
	if (strcmp(*message, "=-\n") == 0) {
		free(*message);
		send_connection_status(ctx, true);
		exit(EXIT_SUCCESS);
	}
	if (strcmp(*message, "!username\n") == 0) {
		free(*message);
		if (change_username(ctx, message, message_length)) {
			send_connection_status(ctx, true); // may not work
			error(ctx->socket, "change_username()");
		}
		return CMD_USERNAME;
	}
	if (strcmp(*message, "!fingerprint\n") == 0) {
		fprint("Fingerprint is: ", ctx->fingerprint);
		return CMD_FINGERPRINT;
	}
	return CMD_NONE;
}

static int recv_handler(client_t *ctx)
{
	wire_t *wire = malloc(sizeof(*wire) + RECV_MAX_LENGTH);
	if (!wire) {
		return -1;
	}
	memset(wire, 0, sizeof(*wire) + RECV_MAX_LENGTH);

	const ssize_t bytes_recv = xrecv(ctx->socket, wire, sizeof(*wire) + RECV_MAX_LENGTH, 0);
	if (bytes_recv < 0) {
		error(ctx->socket, "recv()");
	}
	if (bytes_recv == 32) {
		if (!memcmp(wire, ctx->ctrl_key, 32)) {
			return node_key_exchange(ctx->socket, ctx->ctrl_key, ctx->session_key, ctx->fingerprint);
		}
	}
	
	if (decrypt_wire(wire, ctx->session_key)) {
		return -1;
	}

	printf("\033[2K\r%s\n%s: ", wire->data, ctx->username);
	fflush(stdout);

	free(wire);

	return 0;
}

void send_connection_status(client_t *ctx, bool leave)
{
	char msg[USERNAME_MAX_LENGTH + 36] = { 0 };
	static const char *template = "\033[3%dm%s has %s the server.\033[0m";

	if (snprintf(msg, sizeof(msg), template, leave ? 5 : 2, ctx->username, leave ? "left" : "joined") < 0) {
		error(ctx->socket, "send()");
	}
	send_encrypted_message(ctx->socket, msg, strlen(msg) + 1, ctx->session_key);

	if (leave) {
		if (xclose(ctx->socket)) {
			error(ctx->socket, "close()");
		}
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

	printf("\033[32mConnected to server\033[0m\n");
	send_connection_status(client, false);
}

void *recv_thread(void *ctx)
{
	client_t client;
	client_t *client_ctx = (client_t *)ctx;

	while (1) {
		pthread_mutex_lock(&client_ctx->mutex_lock);
		memcpy(&client, client_ctx, sizeof(client_t));
		pthread_mutex_unlock(&client_ctx->mutex_lock);

		if (recv_handler(&client)) {
			fprintf(stderr, "\nError: recv_handler\n");
			pthread_exit(NULL);
		}

		pthread_mutex_lock(&client_ctx->mutex_lock);
		memcpy(client_ctx, &client, sizeof(client_t));
		pthread_mutex_unlock(&client_ctx->mutex_lock);

		struct timespec ts = { .tv_nsec = 1000000 };
		(void)nanosleep(&ts, NULL);
	}
}

int send_thread(void *ctx)
{
	client_t client;
	client_t *client_ctx = (client_t *)ctx;

	while (1) {
		pthread_mutex_lock(&client_ctx->mutex_lock);
		memcpy(&client, client_ctx, sizeof(client_t));
		pthread_mutex_unlock(&client_ctx->mutex_lock);

		char *plaintext = NULL;
		size_t length = 0;

		// Don't send an empty message
		do {
			printf("%s: ", client.username);
			fflush(stdout);
			xgetline(&plaintext, &length, stdin);
		} while (!length);
		
		if (!plaintext) {
			error(client.socket, "malloc()");
		}

		// TODO: clean this up
		switch (is_command(&client, &plaintext, &length)) {
			case CMD_NONE:
				prepend_username(&client, &plaintext, &length);
				if (!plaintext) {
					error(client.socket, "malloc()");
				}
				break;
			case CMD_USERNAME:
				break;
			case CMD_FINGERPRINT:
				goto NO_SEND;
		}

		send_encrypted_message(client.socket, plaintext, length, client.session_key);
	NO_SEND:
		free(plaintext);

		pthread_mutex_lock(&client_ctx->mutex_lock);
		memcpy(client_ctx, &client, sizeof(client_t));
		pthread_mutex_unlock(&client_ctx->mutex_lock);

		struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000 };
		(void)nanosleep(&ts, NULL);
	}

	send_connection_status(&client, true);
	return EXIT_SUCCESS;
}
