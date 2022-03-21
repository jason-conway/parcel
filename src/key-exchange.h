/**
 * @file key-exchange.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Parcel Client-Server Key Exchange
 * @version 0.1
 * @date 2022-02-01
 *
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 *
 */

#include "sha256.h"
#include "x25519.h"
#include "wire.h"
#include "xplatform.h"
#include <inttypes.h>

void fprint(const char *str, const uint8_t *fingerprint);
int two_party_client(const sock_t socket, uint8_t *key, uint8_t *fingerprint);
int two_party_server(const sock_t socket, uint8_t *session_key);
int node_key_exchange(const sock_t socket, size_t rounds, uint8_t *session_key, uint8_t *fingerprint);
// int key_exchange_router(sock_t server, fd_set *connections, size_t max_in_set, uint8_t *key);
int key_exchange_router(sock_t *sockets, size_t connections, uint8_t *key);