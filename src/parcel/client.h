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
#include "x25519.h"
#include "key-exchange.h"
#include "wire.h"

enum ParcelConstants
{
	USERNAME_MAX_LENGTH = 32,
	FINGERPRINT_LENGTH = 32,
};

enum command_id
{
	CMD_NONE = 0,
	CMD_EXIT,
	CMD_USERNAME,
	CMD_FINGERPRINT,
	CMD_FILE,
};

typedef struct client_t
{
	sock_t socket;
	char username[USERNAME_MAX_LENGTH];
	uint8_t fingerprint[FINGERPRINT_LENGTH];
	uint8_t session_key[KEY_LEN];
	uint8_t ctrl_key[KEY_LEN];
	pthread_mutex_t mutex_lock;
} client_t;

void connect_server(client_t *client, const char *ip, const char *port);
void send_connection_status(client_t *ctx, bool exit);
int parse_input(client_t *ctx, char **message, size_t *message_length);

void *recv_thread(void *ctx);
int send_thread(void *ctx);
