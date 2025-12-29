/**
 * @file client.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief The `parcel` Client
 * @version 0.9.2
 * @date 2021-11-08
 *
 * @copyright Copyright (c) 2021 - 2024 Jason Conway. All rights reserved.
 *
 */

#pragma once

#include "console.h"
#include "key-exchange.h"
#include "wire-file.h"
#include "wire-stat.h"
#include "wire.h"
#include "x25519.h"
#include "slice.h"
#include "xplatform.h"
#include "xutils.h"
#include <assert.h>

enum ParcelConstants {
    USERNAME_MAX_LENGTH = 64,
    PROMPT_MAX_LENGTH = USERNAME_MAX_LENGTH + 30, // 30 bytes of formatting goodness
    PORT_MAX_LENGTH = 6,
    ADDRESS_MAX_LENGTH = 32
};

#define SELF_SENDER "::self::"

typedef enum cmd_type_t {
    CMD_AMBIGUOUS = -1,
    CMD_NONE,
    CMD_LIST,
    CMD_EXIT,
    CMD_USERNAME,
    CMD_ENC_INFO,
    CMD_FILE,
    CMD_CLEAR,
    CMD_VERSION,
} cmd_type_t;

enum SendType {
    SEND_NONE = 0,
    SEND_TEXT,
    SEND_FILE
};

enum RecvStatus {
    RECV_ERROR = -1,
    RECV_OK = 0,
};

struct username {
    char data[USERNAME_MAX_LENGTH];
    size_t length;
};

typedef struct keys_t {
    uint8_t session[KEY_LEN]; // Group-derived symmetric key
    uint8_t ctrl[KEY_LEN];    // Ephemeral daemon control key
} keys_t;

struct client_internal {
    bitfield conn_announced : 1;
    bitfield kill_threads : 1;
};

typedef struct client_t client_t;
struct client_t {
    sock_t socket;
    char username[USERNAME_MAX_LENGTH];
    keys_t keys;
    atomic_bool conn_announced;
    atomic_bool keep_alive;
    pthread_mutex_t lock;
};

bool connect_server(client_t *client, const char *ip, const char *port);

bool announce_connection(client_t *ctx);

bool exec_cmd(client_t *ctx, char *message);

void prompt_args(char *address, char *username);

bool handle_wire(client_t *ctx, wire_t *wire);

void redraw_prompt(void);

void init_ui_lock(void);

void reset_last_sender(void);

bool need_sender(const char *username);

void update_last_sender(const char *username);

bool cmd_exit(client_t *ctx);

void *recv_thread(void *ctx);

int send_thread(void *ctx);

bool send_text_msg(client_t *client, void *data, size_t length);

sock_t client_get_socket(client_t *ctx);
void client_get_username(client_t *ctx, char *out);

void client_get_keys(client_t *ctx, keys_t *out);
void client_set_keys(client_t *ctx, keys_t *keys);

bool transmit_wire(client_t *client, wire_t *wire);
wire_t *client_init_text_wire(client_t *client, const void *data, size_t len);
wire_t *client_init_stat_conn_wire(client_t *client, stat_msg_type_t type);
wire_t *client_init_stat_rename_wire(client_t *client, const char *new);
wire_t *client_init_file_wire(client_t *client, const char *path);
