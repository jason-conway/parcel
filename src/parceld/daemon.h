/**
 * @file daemon.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief The `parcel` Daemon 
 * @version 0.9.1
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
#include <stdnoreturn.h>

#include "xplatform.h"
#include "x25519.h"
#include "key-exchange.h"
#include "sha256.h"
#include "wire.h"

enum ParceldConstants
{
	SOCK_LEN = sizeof(struct sockaddr),
	MAX_CONNECTIONS = FD_SETSIZE,
	MAX_QUEUE = 32,
	
	DEFAULT_PORT = 2315,
	PORT_MAX_LENGTH = 6,
};

typedef struct server_t
{
	uint8_t server_key[KEY_LEN];
	fd_set descriptors;
	size_t descriptor_count;
	sock_t sockets[MAX_CONNECTIONS];
	size_t connection_count;
} server_t;

typedef struct msg_t
{
	uint8_t data[RECV_MAX_BYTES];
	ssize_t length;
} msg_t;

void configure_server(server_t *srv, const char *port);
int main_thread(void *ctx);
