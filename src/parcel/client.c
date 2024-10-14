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

static void create_prefix(struct username *username, char *prompt)
{
    // append(ioctx, username->data, username->length);
    // APPEND_STR(ioctx, ": ");
    memcpy(prompt, username->data, username->length);
    memcpy(&prompt[username->length], ": ", 3);
}

void disp_username(struct username *username)
{
    uint8_t msg_prefix[USERNAME_MAX_LENGTH + 3];
    slice_t s = STATIC_SLICE(msg_prefix);
    slice_append(&s, username->data, username->length);
    // append(&ctx, username->data, username->length);
    SLICE_APPEND_CONST_STR(&s, ": ");
    full_write(STDOUT_FILENO, s.data, s.len);
    // create_prefix(username, msg_prefix);
    // (void)xwrite(STDOUT_FILENO, ctx.data, strlen(msg_prefix));
    fflush(stdout);
}

/**
 * @return returns number of bytes sent on success, otherwise a negative value is returned
 */
static ssize_t send_encrypted_message(sock_t socket, uint64_t type, void *data, size_t length, const uint8_t *key)
{
    wire_t *wire = init_wire(data, type, &length);
    encrypt_wire(wire, key);
    if (xsendall(socket, wire, length) < 0) {
        xfree(wire);
        return -1;
    }
    xfree(wire);
    return length;
}

bool announce_connection(client_t *ctx)
{
    uint8_t msg[USERNAME_MAX_LENGTH + 20] = { 0 };
    slice_t s = STATIC_SLICE(msg);
    SLICE_APPEND_CONST_STR(&s, "\033[1m");
    slice_append(&s, ctx->username.data, ctx->username.length);
    SLICE_APPEND_CONST_STR(&s, " is online\033[0m\0");

    if (send_encrypted_message(ctx->socket, TYPE_TEXT, s.data, s.len, ctx->keys.session) < 0) {
        return false;
    }

    ctx->internal.conn_announced = 1;
    return true;
}

int send_thread(void *ctx)
{
    client_t client;
    client_t *client_ctx = ctx;
    xmemcpy_locked(&client_ctx->mutex_lock, &client, client_ctx, sizeof(client_t));
    int status = 0;

    for (;;) {
        char prompt[USERNAME_MAX_LENGTH + 3];
        create_prefix(&client.username, prompt);

        size_t length = 0;
        char *plaintext = xprompt(prompt, "text", &length); // xprompt() will never return null by design

        xmemcpy_locked(&client_ctx->mutex_lock, &client, client_ctx, sizeof(client_t));
        if (client.internal.kill_threads) {
            xfree(plaintext);
            return 1;
        }

        enum command_id id = CMD_NONE;

        switch (parse_input(&client, &id, &plaintext, &length)) {
            case SEND_NONE:
                break;
            case SEND_TEXT:
                if (send_encrypted_message(client.socket, TYPE_TEXT, plaintext, length, client.keys.session) < 0) {
                    xalert("Error sending encrypted text\n");
                    status = -1;
                }
                break;
            case SEND_FILE:
                if (send_encrypted_message(client.socket, TYPE_FILE, plaintext, length, client.keys.session) < 0) {
                    xalert("Error sending encrypted file\n");
                    status = -1;
                }
                break;
        }

        xfree(plaintext);

        switch (id) {
            default:
                if (!status) {
                    break;
                } // fallthrough
            case CMD_EXIT:
                goto cleanup;
            case CMD_USERNAME:
                xmemcpy_locked(&client_ctx->mutex_lock, &client_ctx->username, &client.username, sizeof(struct username));
                break;
        }

        struct timespec ts = { .tv_nsec = 1000000 };
        (void)nanosleep(&ts, NULL);
    }

cleanup:
        client.internal.kill_threads = 1;
        xmemcpy_locked(&client_ctx->mutex_lock, &client_ctx->internal, &client.internal, sizeof(struct client_internal));
        return shutdown(client.socket, SHUT_RDWR) || status;
}

static int recv_remaining(client_t *ctx, wire_t **wire, size_t bytes_recv, size_t bytes_remaining)
{
    size_t wire_size = bytes_recv + bytes_remaining;
    *wire = xrealloc(*wire, wire_size);
    if (!*wire) {
        return -1;
    }

    wire_t *dst = *wire;
    for (size_t i = 0; i < bytes_remaining;) {
        ssize_t received = xrecv(ctx->socket, &dst->data[bytes_recv - sizeof(wire_t) + i], bytes_remaining - i, 0);
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
    if (!wire) {
        return NULL;
    }

    const ssize_t bytes_recv = xrecv(ctx->socket, wire, RECV_MAX_BYTES, 0);

    // Refresh any changes to shared context that may have occured while blocking on recv
    xmemcpy_locked(&ctx->shctx->mutex_lock, ctx, ctx->shctx, sizeof(client_t));

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
            xalert("%s\n", "> CMAC error");
            return -1;
        case WIRE_OK:
            break;
    }
    return length; // All good
}

void *recv_thread(void *ctx)
{
    client_t client;
    client_t *client_ctx = ctx;
    memcpy(&client, client_ctx, sizeof(client_t));

    for (;;) {
        size_t bytes_recv = 0;
        wire_t *wire = recv_new_wire(&client, &bytes_recv);
        if (!wire) {
            // TODO: cleanly exit without user interaction
            if (!client.internal.kill_threads) {
                client.internal.kill_threads = 1;
                xmemcpy_locked(&client_ctx->mutex_lock, &client_ctx->internal, &client.internal, sizeof(struct client_internal));

                xwarn("\n%s\n", "Daemon unexpectedly closed connection");
                xwarn("%s\n", "Use '/q' to exit");
                disp_username(&client.username);
            }
            break;
        }

        if (decrypt_received_message(&client, wire, bytes_recv) < 0) {
            xfree(wire);
            break;
        }

        if (proc_type(&client, wire) < 0) {
            xfree(wire);
            break;
        }

        struct timespec ts = { .tv_nsec = 1000000 };
        (void)nanosleep(&ts, NULL);
    }

    xclose(client.socket);
    return xfree(client_ctx);
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

void prompt_args(char *address, struct username *username)
{
    size_t address_length = ADDRESS_MAX_LENGTH;
    if (!address[0]) {
        char *str = xprompt("\033[1m> Enter server address: \033[0m", "address", &address_length);
        memcpy(address, str, address_length);
        xfree(str);
    }

    size_t username_length = USERNAME_MAX_LENGTH;
    if (!*username->data) {
        char *str = xprompt("\033[1m> Enter username: \033[0m", "username", &username_length);
        username->length = username_length;
        memcpy(username->data, str, username_length);
        xfree(str);
    }
}
