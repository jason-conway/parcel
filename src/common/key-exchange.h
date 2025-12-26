/**
 * @file key-exchange.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Parcel Client-Server Key Exchange
 * @version 0.9.2
 * @date 2022-02-01
 *
 * @copyright Copyright (c) 2022 - 2024 Jason Conway. All rights reserved.
 *
 */

#include "sha256.h"
#include "wire.h"
#include "x25519.h"
#include "xplatform.h"
#include "log.h"

enum KeyExchangeStatus {
    DHKE_ERROR = -1,
    DHKE_OK,
};

typedef enum key_type_t {
    KEY_TYPE_NONE,
    KEY_CLIENT_PUBLIC,
    KEY_SERVER_PUBLIC,
    KEY_EX_INIT_ROUND,
    KEY_EX_INTERMEDIATE,
    KEY_EX_LAST_ROUND,
} key_type_t;

typedef struct ke_t {
    const uint8_t type;
    uint8_t key[KEY_LEN];
} __attribute__((packed)) ke_t;

bool two_party_client(sock_t socket, uint8_t *ctrl_key);
bool two_party_server(sock_t socket, uint8_t *session_key);

bool n_party_client(sock_t socket, uint8_t *session_key, size_t rounds);
bool n_party_server(sock_t *sockets, size_t connections, uint8_t *ctrl_key);
