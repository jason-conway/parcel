/**
 * @file client.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief The `parcel` Client
 * @version 0.9.2
 * @date 2021-11-08
 *
 * @copyright Copyright (c) 2021-2022 Jason Conway. All rights reserved.
 *
 */

#pragma once

#include "xplatform.h"
#include "xutils.h"
#include "x25519.h"
#include "key-exchange.h"
#include "wire.h"
#include "console.h"

enum ParcelConstants {
	USERNAME_MAX_LENGTH = 32,
	PORT_MAX_LENGTH = 6,
	ADDRESS_MAX_LENGTH = 32
};

enum command_id {
	CMD_AMBIGUOUS = -1,
	CMD_NONE,
	CMD_LIST,
	CMD_EXIT,
	CMD_USERNAME,
	CMD_ENC_INFO,
	CMD_FILE,
	CMD_CLEAR,
	CMD_VERSION,
};

enum SendType {
	SEND_NONE = 0,
	SEND_TEXT,
	SEND_FILE
};

enum RecvStatus {
	RECV_ERROR = -1,
	RECV_OK = 0,
};

typedef struct parcel_keys_t {
	uint8_t session[KEY_LEN]; // Group-derived symmetric key
	uint8_t ctrl[KEY_LEN];    // Ephemeral daemon control key
} parcel_keys_t;

typedef struct username_t {
	char data[USERNAME_MAX_LENGTH];
	size_t length;
} username_t;

typedef struct client_t {
	struct client_t *shctx; // Pointer to the "real" context, shared between threads
	sock_t socket;
	username_t username;
	parcel_keys_t keys;
	bool conn_announced;
	bool kill_threads;
	pthread_mutex_t mutex_lock;
} client_t;

int connect_server(client_t *client, const char *ip, const char *port);

int announce_connection(client_t *ctx);

int parse_input(client_t *ctx, enum command_id *cmd, char **message, size_t *message_length);

void prompt_args(char *address, username_t *username);

int proc_type(client_t *ctx, wire_t *wire);

void disp_username(username_t *username);
int cmd_exit(client_t *ctx, char **message, size_t *message_length);
void *recv_thread(void *ctx);
int send_thread(void *ctx);
