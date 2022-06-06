/**
 * @file client.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief
 * @version 0.9.1
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

enum ParcelConstants
{
	USERNAME_MAX_LENGTH = 32,
	PORT_MAX_LENGTH = 6,
	ADDRESS_MAX_LENGTH = 32
};

enum command_id
{
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

enum SendType
{
	SEND_NONE = 0,
	SEND_TEXT,
	SEND_FILE
};

typedef struct parcel_keys_t
{
	uint8_t session[KEY_LEN]; // Group-derived symmetric key
	uint8_t ctrl[KEY_LEN];    // Ephemeral daemon control key
} parcel_keys_t;

typedef struct client_t
{
	sock_t socket;
	char username[USERNAME_MAX_LENGTH];
	parcel_keys_t keys;
	bool conn_announced;
	pthread_mutex_t mutex_lock;
} client_t;

int connect_server(client_t *client, const char *ip, const char *port);

int announce_connection(client_t *ctx);

int parse_input(client_t *ctx, enum command_id *cmd, char **message, size_t *message_length);

void prompt_args(char *address, char *username);

// void fatal(const char *msg);

int proc_file(void *data);
int proc_ctrl(client_t *ctx, void *data);
void proc_text(uint8_t *wire_data);

int cmd_exit(client_t *ctx, char **message, size_t *message_length);

void *recv_thread(void *ctx);
int send_thread(void *ctx);
