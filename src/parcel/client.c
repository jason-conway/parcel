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
#include "cable.h"
#include "log.h"
#include "wire-stat.h"
#include "xplatform.h"
#include <stddef.h>
#include <stdio.h>

void client_get_keys(client_t *ctx, keys_t *out)
{
    pthread_mutex_lock(&ctx->lock);
    memcpy(out, &ctx->keys, sizeof(keys_t));
    pthread_mutex_unlock(&ctx->lock);
}

void client_set_keys(client_t *ctx, keys_t *keys)
{
    pthread_mutex_lock(&ctx->lock);
    memcpy(&ctx->keys, keys, sizeof(keys_t));
    pthread_mutex_unlock(&ctx->lock);
}

void client_get_username(client_t *ctx, char *out)
{
    pthread_mutex_lock(&ctx->lock);
    size_t len = strnlen(ctx->username, USERNAME_MAX_LENGTH - 1);
    memcpy(out, ctx->username, len);
    out[len] = '\0';
    pthread_mutex_unlock(&ctx->lock);
}

sock_t client_get_socket(client_t *ctx)
{
    pthread_mutex_lock(&ctx->lock);
    sock_t s = ctx->socket;
    pthread_mutex_unlock(&ctx->lock);
    return s;
}

void fmt_prmpt(client_t *ctx, char *out)
{
    char username[USERNAME_MAX_LENGTH] = { 0 };
    client_get_username(ctx, username);
    snprintf(out, PROMPT_MAX_LENGTH, "\033[90m[%s]\033[0m \033[0;32m➜\033[0m ", username);
}

void redraw_prompt(client_t *ctx)
{
    static char prompt[PROMPT_MAX_LENGTH];
    fmt_prmpt(ctx, prompt);
    fprintf(stdout, "%s", prompt);
    fflush(stdout);
}

bool announce_connection(client_t *ctx)
{
    wire_t *wire = client_init_stat_conn_wire(ctx, STAT_USER_CONNECT);
    bool ok = transmit_wire(ctx, wire);
    if (!ok) {
        log_error("error sending wire via cable");
    }
    xfree(wire);

    atomic_store(&ctx->conn_announced, ok);
    return ok;
}

bool send_text_msg(client_t *ctx, void *data, size_t length)
{
    wire_t *wire = client_init_text_wire(ctx, data, length);
    bool ok = transmit_wire(ctx, wire);
    if (!ok) {
        log_error("error sending wire via cable");
    }
    xfree(wire);
    return ok;
}

static bool is_cmd(const char *msg)
{
    return msg[0] == '/';
}

int send_thread(void *ctx)
{
    client_t *client = ctx;
    for (;;) {
        size_t length = 0;

        char prompt[PROMPT_MAX_LENGTH] = { 0 };
        fmt_prmpt(ctx, prompt);
        char *msg = xprompt(prompt, "text", &length); // xprompt() will never return null by design

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

cable_t *client_recv_cable(client_t *ctx)
{
    sock_t s = client_get_socket(ctx);
    size_t len = 0;
    return recv_cable(s, &len);
}

static wire_t *decrypt_cabled_wire(client_t *ctx, cable_t *cable)
{
    size_t len = cable_get_payload_len(cable);
    wire_t *wire = cable_get_data(cable);

    keys_t keys = { 0 };
    client_get_keys(ctx, &keys);
    if (!decrypt_wire(wire, len, keys.session, keys.ctrl)) {
        log_error("wire decryption error");
        return NULL;
    }
    return wire;
}

void *recv_thread(void *ctx)
{
    client_t *client = ctx;

    for (;;) {
        cable_t *cable = client_recv_cable(client);
        if (!cable) {
            // TODO: cleanly exit without user interaction
            bool run = atomic_load(&client->keep_alive);
            if (run) {
                atomic_store(&client->keep_alive, false);
                xwarn("\n%s\n", "Daemon unexpectedly closed connection");
                xwarn("%s\n", "Use '/q' to exit");
                redraw_prompt(client);
            }
            else {
                xclose(client->socket);
                return (void *)0;
            }
        }

        wire_t *wire = decrypt_cabled_wire(client, cable);
        if (!wire) {
            xfree(cable);
            continue;
        }

        if (!handle_wire(client, wire)) {
            log_error("encountered error while handling wire");
        }

        free_cabled_wire(wire);

        nanosleep((struct timespec []) { [0] = { 0, 1000000} }, NULL);
    }

    xclose(client->socket);
    return (void *)0;
}

bool connect_server(client_t *client, const char *ip, const char *port)
{
    if (xstartup()) {
        log_fatal("WSAStartup failure");
        return false;
    }

    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };

    struct addrinfo *srv_addr;
    if (xgetaddrinfo(ip, port, &hints, &srv_addr)) {
        log_fatal("failed to get addresses and ports for host");
        return false;
    }

    struct addrinfo *node = NULL;
    for (node = srv_addr; node; node = node->ai_next) {
        if (!xsocket(&client->socket, node->ai_family, node->ai_socktype, node->ai_protocol)) {
            continue;
        }
        if (connect(client->socket, node->ai_addr, node->ai_addrlen)) {
            if (xclose(client->socket)) {
                log_fatal("unable to close socket");
                return false;
            }
            continue;
        }
        break;
    }

    if (!node) {
        xalert("could not connect to server\n");
        return false;
    }

    freeaddrinfo(srv_addr);

    if (!two_party_client(client->socket, client->keys.ctrl)) {
        // [note] error logged internally 
        xclose(client->socket);
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
