/**
 * @file daemon.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief The `parcel` Daemon 
 * @version 0.9.2
 * @date 2021-11-21
 *
 * @copyright Copyright (c) 2021-2022 Jason Conway. All rights reserved.
 *
 */

#pragma once

#include "xplatform.h"
#include "xutils.h"
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
	PORT_MAX_LENGTH = 6
};

enum SocketIndices
{
	DAEMON_SOCKET = 0,
};

typedef struct fd_set_t
{
	fd_set fds;
	size_t nfds;
} fd_set_t;

typedef struct server_t
{
	char server_port[PORT_MAX_LENGTH];
	uint8_t server_key[KEY_LEN];
	fd_set_t descriptors;
	sock_t sockets[MAX_CONNECTIONS];
	size_t active_connections;
} server_t;

typedef struct msg_t
{
	uint8_t data[RECV_MAX_BYTES];
	ssize_t length;
} msg_t;

int init_daemon(server_t *ctx);
int display_daemon_info(server_t *ctx);
int main_thread(void *ctx);
