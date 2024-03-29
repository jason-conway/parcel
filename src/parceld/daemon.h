/**
 * @file daemon.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief The `parcel` Daemon 
 * @version 0.9.2
 * @date 2021-11-21
 *
 * @copyright Copyright (c) 2021 - 2024 Jason Conway. All rights reserved.
 *
 */

#pragma once

#include "xplatform.h"
#include "xutils.h"
#include "x25519.h"
#include "key-exchange.h"
#include "sha256.h"
#include "wire.h"

enum ParceldConstants {
	SOCK_LEN = sizeof(struct sockaddr),
	SUPPORTED_CONNECTIONS = FD_SETSIZE,
	MAX_QUEUE = 32,
	DEFAULT_PORT = 2315,
	PORT_MAX_LENGTH = 6
};

enum SocketIndices {
	DAEMON_SOCKET = 0,
};

typedef struct server_t {
	char server_port[PORT_MAX_LENGTH];
	size_t max_queue;
	uint8_t server_key[KEY_LEN];
	struct fd_set_t {
		fd_set fds;
		size_t nfds;
	} descriptors;
	struct sfd_set_t {
		sock_t *sfds; // Socket file descriptors
		size_t nsfds; // Number of socket file descriptors
		size_t max_nsfds; // Maximum number of socket file descriptors
	} sockets;
} server_t;

typedef struct msg_t {
	uint8_t data[RECV_MAX_BYTES];
	ssize_t length;
} msg_t;

int init_daemon(server_t *ctx);
int display_daemon_info(server_t *ctx);
int main_thread(void *ctx);
