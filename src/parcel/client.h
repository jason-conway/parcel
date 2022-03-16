/**
 * @file client.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief
 * @version 0.1
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

#include "xplatform.h"
#include "x25519.h"
#include "key-exchange.h"
#include "wire.h"

enum ClientLimits
{
	USERNAME_MAX_LENGTH = 32,
	KEY_LENGTH = 32,
	FINGERPRINT_LENGTH = 32,
	RECV_MAX_LENGTH = 1 << 20,
};

enum CommandID
{
	CMD_NONE = 0,
	CMD_USERNAME,
	CMD_FINGERPRINT,
	CMD_FILE,
};

typedef struct client_t
{
	sock_t socket;
	char username[USERNAME_MAX_LENGTH];
	uint8_t fingerprint[FINGERPRINT_LENGTH];
	uint8_t session_key[KEY_LENGTH];
	uint8_t ctrl_key[KEY_LENGTH];
	pthread_mutex_t mutex_lock;
} client_t;

void connect_server(client_t *client, const char *ip, const char *port);
void send_connection_status(client_t *ctx, bool exit);
int parse_input(client_t *ctx, char **message, size_t *message_length);

void *recv_thread(void *ctx);
int send_thread(void *ctx);

#define BLK "\033[0;30m"
#define RED "\033[0;31m"
#define GRN "\033[0;32m"
#define YEL "\033[0;33m"
#define BLU "\033[0;34m"
#define MAG "\033[0;35m"
#define CYN "\033[0;36m"
#define WHT "\033[0;37m"


#define BBLK "\033[1;30m"
#define BRED "\033[1;31m"
#define BGRN "\033[1;32m"
#define BYEL "\033[1;33m"
#define BBLU "\033[1;34m"
#define BMAG "\033[1;35m"
#define BCYN "\033[1;36m"
#define BWHT "\033[1;37m"