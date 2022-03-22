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
	pthread_exit(NULL);
	exit(EXIT_FAILURE);
}

static void send_encrypted_message(int socket, uint64_t type, void *data, size_t length, const uint8_t *key)
{
	size_t len = length;
	wire_t *wire = init_wire(data, type,  &len);
	encrypt_wire(wire, key);
	if (xsendall(socket, wire, len) < 0) {
		free(wire);
		error(socket, "xsendall()");
	}
	free(wire);
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
		pthread_exit(NULL);
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

		switch (parse_input(&client, &plaintext, &length)) {
			case CMD_NONE:
			case CMD_USERNAME:
				send_encrypted_message(client.socket, TYPE_TEXT, plaintext, length, client.session_key);
				break;
			case CMD_FINGERPRINT:
				break;
			case CMD_FILE:
				break; // TODO
		}
		
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

static int recv_handler(client_t *ctx)
{
	wire_t *wire = new_wire();
	if (!wire) {
		return -1;
	}

	const ssize_t bytes_recv = xrecv(ctx->socket, wire, sizeof(*wire) + RECV_MAX_LENGTH, 0);
	if (bytes_recv < 0) {
		free(wire);
		error(ctx->socket, "recv()");
	}
	
	size_t length[1] = { bytes_recv };
	switch (_decrypt_wire(wire, length, ctx->session_key)) {
		case INCORRECT_KEY:
			if (_decrypt_wire(wire, length, ctx->ctrl_key)) {
				return -1;
			}
			break;
		case CMAC_ERROR:
			return -1;
		case WIRE_OK:
			break;
	}

	// if (wire_get_type(wire) == TYPE_CTRL) {
	// 	// size_t rounds = strtol((char *)wire->data, NULL, 10);
	// 	size_t rounds = wire_get_first_word(wire);
	// 	memcpy(ctx->ctrl_key, &wire->data[16], 32);
	// 	if (node_key_exchange(ctx->socket, rounds, ctx->session_key, ctx->fingerprint)) {
	// 		return -1;
	// 	}
	// }
	
	switch (wire_get_type(wire)) {
		case TYPE_CTRL: {
			size_t rounds = wire_get_first_word(wire);
			memcpy(ctx->ctrl_key, &wire->data[16], 32);
			if (node_key_exchange(ctx->socket, rounds, ctx->session_key, ctx->fingerprint)) {
				return -1;
			}
			break;
		}
		case TYPE_FILE:
			// TODO
			break;
		case TYPE_TEXT:
			printf("\033[2K\r%s\n%s: ", wire->data, ctx->username);
			fflush(stdout);
			break;
	}
	
	free(wire);
	return 0;
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
			error(client.socket, "recv_handler()");
		}

		pthread_mutex_lock(&client_ctx->mutex_lock);
			memcpy(client_ctx, &client, sizeof(client_t));
		pthread_mutex_unlock(&client_ctx->mutex_lock);

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

	printf("\033[32mConnected to server\033[0m\n");
	// send_connection_status(client, false);
}