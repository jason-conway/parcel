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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdatomic.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <stdnoreturn.h>

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
	CMD_NONE = 0,
	CMD_LIST,
	CMD_EXIT,
	CMD_USERNAME,
	CMD_ENC_INFO,
	CMD_FILE,
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
	uint8_t ctrl[KEY_LEN]; // Ephemeral daemon control key
} parcel_keys_t;

typedef struct client_t
{
	sock_t socket;
	char username[USERNAME_MAX_LENGTH];
	parcel_keys_t keys;
	pthread_mutex_t mutex_lock;
} client_t;

void connect_server(client_t *client, const char *ip, const char *port);
void send_connection_status(client_t *ctx, bool exit);
int parse_input(client_t *ctx, enum command_id *cmd, char **message, size_t *message_length);
void prompt_args(char *address, char *username);
noreturn void fatal(const char *msg);
int proc_file(uint8_t *data);
int proc_ctrl(client_t *ctx, uint8_t *wire_data);
void proc_text(uint8_t *wire_data);

void *recv_thread(void *ctx);
int send_thread(void *ctx);
