/**
 * @file client.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief The `parcel` Client
 * @version 0.9.2
 * @date 2021-11-08
 *
 * @copyright Copyright (c) 2021 - 2024 Jason Conway. All rights reserved.
 *
 */

#include "client.h"

void client_get_keys(keys_t *dst, client_t *ctx)
{
    pthread_mutex_lock(&ctx->lock);
    memcpy(dst, &ctx->keys, sizeof(*dst));
    pthread_mutex_unlock(&ctx->lock);
}

void client_get_user(char *dst, client_t *ctx)
{
    pthread_mutex_lock(&ctx->lock);
    memcpy(dst, ctx->username, sizeof(ctx->username));
    pthread_mutex_unlock(&ctx->lock);
}

sock_t client_get_socket(client_t *ctx)
{
    sock_t s = 0;
    pthread_mutex_lock(&ctx->lock);
    s = ctx->socket;
    pthread_mutex_unlock(&ctx->lock);
    return s;
}

void disp_username(const char *username)
{
    char msg_prefix[USERNAME_MAX_LENGTH + 3];
    slice_t s = STATIC_SLICE(msg_prefix);
    SLICE_APPEND_STR(&s, username);
    SLICE_APPEND_CONST_STR(&s, ": ");
    full_write(STDOUT_FILENO, s.data, s.len);
    fflush(stdout);
}

static void get_prompt(slice_t *s, client_t *ctx)
{
    pthread_mutex_lock(&ctx->lock);
    SLICE_APPEND_STR(s, ctx->username);
    pthread_mutex_unlock(&ctx->lock);
    SLICE_APPEND_CONST_STR(s, ": \0");
}

bool announce_connection(client_t *ctx)
{
    // debug_print("announcing connection for username: %s\n", ctx->username);
    char user[USERNAME_MAX_LENGTH] = { 0 };
    client_get_user(user, ctx);

    keys_t keys = { 0 };
    client_get_keys(&keys, ctx);
    sock_t socket = client_get_socket(ctx);

    stat_msg_t stat = { 0 };
    wire_set_stat_msg_type(&stat, STAT_USER_CONNECT);
    wire_set_stat_msg_user(&stat, user);

    size_t len = sizeof(stat_msg_t);
    wire_t *wire = init_wire(&stat, TYPE_STAT, &len);
    
    encrypt_wire(wire, keys.session);

    if (!xsendall(socket, wire, len)) {
        xfree(wire);
        return false;
    }
    xfree(wire);

    atomic_store(&ctx->conn_announced, true);
    return true;
}

bool send_text_msg(client_t *ctx, void *data, size_t length)
{
    char user[USERNAME_MAX_LENGTH] = { 0 };
    client_get_user(user, ctx);

    keys_t keys = { 0 };
    client_get_keys(&keys, ctx);

    sock_t socket = client_get_socket(ctx);

    size_t len = sizeof(text_msg_t) + length;
    text_msg_t *text = xcalloc(len);
    wire_set_text_msg_user(text, user);
    wire_set_text_msg_data(text, data, length);

    wire_t *wire = init_wire(text, TYPE_TEXT, &len);

    encrypt_wire(wire, keys.session);

    if (!xsendall(socket, wire, len)) {
        xfree(text);
        xfree(wire);
        return false;
    }
    xfree(text);
    xfree(wire);
    return true;
}

static bool is_cmd(const char *msg)
{
    return msg[0] == '/';
}

int send_thread(void *ctx)
{
    client_t *client = ctx;
    for (;;) {
        char prompt[USERNAME_MAX_LENGTH + 3];
        slice_t s = STATIC_SLICE(prompt);
        get_prompt(&s, client);

        size_t length = 0;
        char *msg = xprompt((const char *)s.data, "text", &length); // xprompt() will never return null by design

        bool run = atomic_load(&client->keep_alive);
        if (!run) {
            xfree(msg);
            return 1;
        }

        if (is_cmd(msg)) {
            if (!exec_cmd(client, msg)) {
                // TODO: error case
            }
        }
        else {
            if (!send_text_msg(client, msg, length)) {
                // TODO: error case
            }
        }

        xfree(msg);
        nanosleep((struct timespec []) { [0] = { 0, 1000000} }, NULL);
    }

}

static int recv_remaining(client_t *ctx, wire_t **wire, size_t bytes_recv, size_t remaining)
{
    size_t wire_size = bytes_recv + remaining;
    *wire = xrealloc(*wire, wire_size);

    wire_t *dst = *wire;
    for (size_t i = 0; i < remaining;) {
        ssize_t received = xrecv(ctx->socket, &dst->data[bytes_recv - sizeof(wire_t) + i], remaining - i, 0);
        switch (received) {
            case -1:
                return -1;
            default:
                i += received;
        }
    }

    return decrypt_wire(*wire, &wire_size, ctx->keys.session) ? -1 : 0;
}

wire_t *recv_new_wire(client_t *ctx, size_t *wire_size)
{
    wire_t *wire = new_wire();
    const ssize_t bytes_recv = xrecv(ctx->socket, wire, RECV_MAX_BYTES, 0);

    if (bytes_recv <= 0) {
        return xfree(wire);
    }

    *wire_size = bytes_recv;
    return wire;
}

/**
 * @brief Decrypts an encrypted wire
 *
 * @param ctx Client context
 * @param wire Wire received
 * @param bytes_recv Number of bytes received
 * @return Returns length of the wire data section, negative on error
 */
static ssize_t decrypt_received_message(client_t *ctx, wire_t *wire, size_t bytes_recv)
{
    size_t length = bytes_recv;
    switch (decrypt_wire(wire, &length, ctx->keys.session)) {
        case WIRE_INVALID_KEY:
            if (decrypt_wire(wire, &length, ctx->keys.ctrl)) {
                xalert("> Recveived corrupted control key from server\n");
                return -1;
            }
            break;
        case WIRE_PARTIAL:
            debug_print("> Received %zu bytes but header specifies %zu bytes total\n", bytes_recv, length + bytes_recv);
            if (recv_remaining(ctx, &wire, bytes_recv, length)) {
                xalert("recv_remaining()\n");
                return -1;
            }
            debug_print("%s\n", "> received remainder of wire");
            break;
        case WIRE_CMAC_ERROR:
            xalert("> CMAC error\n");
            return -1;
        case WIRE_OK:
            break;
    }
    return length; // All good
}

void *recv_thread(void *ctx)
{
    client_t *client = ctx;

    for (;;) {
        size_t bytes_recv = 0;
        wire_t *wire = recv_new_wire(client, &bytes_recv);
        if (!wire) {
            // TODO: cleanly exit without user interaction
            bool run = atomic_load(&client->keep_alive);
            if (run) {
                atomic_store(&client->keep_alive, false);
                xwarn("\n%s\n", "Daemon unexpectedly closed connection");
                xwarn("%s\n", "Use '/q' to exit");
                disp_username(client->username);
            }
            else {
                xclose(client->socket);
                return (void *)0;
            }
        }

        if (decrypt_received_message(client, wire, bytes_recv) < 0) {
            xfree(wire);
            break;
        }

        if (proc_type(client, wire) < 0) {
            xfree(wire);
            break;
        }
        xfree(wire);

        nanosleep((struct timespec []) { [0] = { 0, 1000000} }, NULL);
    }

    xclose(client->socket);
    return (void *)0;
}

bool connect_server(client_t *client, const char *ip, const char *port)
{
    if (xstartup()) {
        xalert("xstartup()\n");
        return false;
    }

    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };

    struct addrinfo *srv_addr;
    if (xgetaddrinfo(ip, port, &hints, &srv_addr)) {
        xalert("xgetaddrinfo()\n");
        return false;
    }

    struct addrinfo *node = NULL;
    for (node = srv_addr; node; node = node->ai_next) {
        if (xsocket(&client->socket, node->ai_family, node->ai_socktype, node->ai_protocol) < 0) {
            continue;
        }
        if (connect(client->socket, node->ai_addr, node->ai_addrlen)) {
            if (xclose(client->socket)) {
                xalert("Unable to close socket\n");
                return false;
            }
            continue;
        }
        break;
    }

    if (!node) {
        xalert("Could not connect to server\n");
        return false;
    }

    freeaddrinfo(srv_addr);

    if (!two_party_client(client->socket, client->keys.ctrl)) {
        xalert("Failed to perform initial key exchange with server\n");
        if (xclose(client->socket)) {
            xalert("Unable to close socket\n");
        }
        return false;
    }

    xprintf(GRN, BOLD, "=== Connected to server ===\n");
    return true;
}

void prompt_args(char *address, char *username)
{
    size_t address_length = ADDRESS_MAX_LENGTH;
    if (!address[0]) {
        char *str = xprompt("\033[1m> Enter server address: \033[0m", "address", &address_length);
        memcpy(address, str, address_length);
        xfree(str);
    }

    size_t username_length = USERNAME_MAX_LENGTH;
    if (!*username) {
        char *str = xprompt("\033[1m> Enter username: \033[0m", "username", &username_length);
        memcpy(username, str, username_length);
        xfree(str);
    }
}
