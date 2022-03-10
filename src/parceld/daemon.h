/**
 * @file daemon.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief
 * @version 0.1
 * @date 2021-11-21
 *
 * @copyright Copyright (c) 2021-2022 Jason Conway. All rights reserved.
 *
 */

#pragma once

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <stdatomic.h>
#include <inttypes.h>

#include "xplatform.h"
#include "x25519.h"
#include "key-exchange.h"
#include "sha256.h"
#include "wire.h"

enum Server
{
	MAX_CHUNK = 1 << 20,
	SOCK_LEN = sizeof(struct sockaddr),
};

enum KEY { KEY_LENGTH = 32 };

typedef struct server_t
{
	sock_t socket;
	uint8_t server_key[KEY_LENGTH];
	fd_set connections;
	size_t max_in_set;
} server_t;

typedef struct msg_t
{
	char data[MAX_CHUNK];
	ssize_t length;
} msg_t;

void configure_server(server_t *, const char *);
int main_thread(void *);
