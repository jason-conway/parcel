/**
 * @file daemon.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief The `parcel` Daemon
 * @version 0.9.2
 * @date 2021-11-21
 *
 * @copyright Copyright (c) 2021 - 2024 Jason Conway. All rights reserved.
 *
 */

#include "daemon.h"
#include "cable.h"
#include "wire.h"
#include <stddef.h>

void catch_sigint(int sig)
{
    (void)sig;
    xalert("\nApplication aborted\n");
    exit(EXIT_FAILURE);
}

bool init_daemon(server_t *ctx)
{
    // WSAStartup (Windows)
    if (xstartup()) {
        xalert("xstartup()");
        return false;
    }

    memset(&ctx->sockets, 0, sizeof(ctx->sockets));

    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_PASSIVE
    };

    struct addrinfo *ai = NULL;
    if (xgetaddrinfo(NULL, ctx->server_port, &hints, &ai)) {
        xalert("xgetaddrinfo()");
        return false;
    }

    struct addrinfo *node = NULL;
    for (node = ai; node; node = node->ai_next) {
        if (!xsocket(&ctx->sockets.sfds[0], node->ai_family, node->ai_socktype, node->ai_protocol)) {
            continue;
        }
        if (xsetsockopt(ctx->sockets.sfds[0], SOL_SOCKET, SO_REUSEADDR, (int32_t []){ 1 }, sizeof(int32_t)) < 0) {
            xalert("setsockopt()\n");
            return false;
        }
        if (bind(ctx->sockets.sfds[0], node->ai_addr, node->ai_addrlen) < 0) {
            if (xclose(ctx->sockets.sfds[0])) {
                xalert("xclose()\n");
                return false;
            }
            continue;
        }
        break;
    }

    if (!node) {
        xalert("Unable to bind to socket\n");
        return false;
    }
    freeaddrinfo(ai);

    if (listen(ctx->sockets.sfds[0], MAX_QUEUE) < 0) {
        xalert("listen()\n");
        xclose(ctx->sockets.sfds[0]);
        return false;
    }

    FD_ZERO(&ctx->descriptors.fds);
    FD_SET(ctx->sockets.sfds[0], &ctx->descriptors.fds);
    ctx->descriptors.nfds = xfd_init_count(ctx->sockets.sfds[0]);

    // Collect entropy for initial server key
    if (xgetrandom(ctx->server_key, KEY_LEN) < 0) {
        return false;
    }
    return true;
}

// Return `i` such that `srv`->sockets[i] == `socket`
static size_t socket_index(server_t *srv, sock_t socket)
{
    for (size_t i = 1; i <= countof(srv->sockets.sfds); i++) {
        if (srv->sockets.sfds[i] == socket) {
            log_trace("got socket index %zu", i);
            return i;
        }
    }
    return 0;
}

static int add_client(server_t *srv)
{
    struct sockaddr_storage client_sockaddr;
    socklen_t len[] = { sizeof(struct sockaddr_storage) };
    sock_t new_client;
    if (xaccept(&new_client, srv->sockets.sfds[0], (struct sockaddr *)&client_sockaddr, len) < 0) {
        log_error("unable to accept new client");
        return -1;
    }

    if (srv->sockets.nsfds + 1 == countof(srv->sockets.sfds)) {
        log_warn("rejecting new connection");
        return 1;
    }

    FD_SET(new_client, &srv->descriptors.fds); // Add descriptor to set and update max
    srv->descriptors.nfds = xfd_count(new_client, srv->descriptors.nfds);
    srv->sockets.nsfds++;

    char address[INET_ADDRSTRLEN];
    in_port_t port;
    if (!xgetpeeraddr(new_client, address, &port)) {
        log_error("unable to determine human-readable IP for new client");
        return -1;
    }

    // Copy new connection's socket to the next free slot
    log_debug("adding socket to empty slot");
    for (size_t i = 1; i < countof(srv->sockets.sfds); i++) {
        log_trace("slot[%zu]: %s", i, !srv->sockets.sfds[i] ? "free" : "in use");
        if (!srv->sockets.sfds[i]) {
            srv->sockets.sfds[i] = new_client;
            log_debug("connection from %s:%u added to slot %zu", address, port, i);
            break;
        }
    }

    if (!two_party_server(new_client, srv->server_key)) {
        log_error("two-party key exchange with new client failed");
        return -1;
    }

    if (srv->sockets.nsfds > 1) {
        log_debug("connection added - starting key regeneration");
        if (!n_party_server(srv->sockets.sfds, srv->sockets.nsfds, srv->server_key)) {
            log_fatal("key regeneration failure");
            return -1;
        }
    }
    return 0;
}

static bool transfer_message(server_t *srv, size_t sender_index, cable_t *cable)
{
    size_t len = cable_get_total_len(cable);
    for (size_t i = 1; i <= srv->sockets.nsfds; i++) {
        if (i == sender_index) {
            log_trace("skipping message orgin");
            continue;
        }
        log_trace("forwarding message to socket %zu", i);
        if (!xsendall(srv->sockets.sfds[i], cable, len)) {
            return false;
        }
    }
    return true;
}

static int disconnect_client(server_t *ctx, size_t client_index)
{
    FD_CLR(ctx->sockets.sfds[client_index], &ctx->descriptors.fds);
    const int ret = xclose(ctx->sockets.sfds[client_index]);

    // Replace this slot with the ending slot
    if (ctx->sockets.nsfds == 1) {
        ctx->sockets.sfds[client_index] = 0;
    }
    else {
        ctx->sockets.sfds[client_index] = ctx->sockets.sfds[ctx->sockets.nsfds];
        ctx->sockets.sfds[ctx->sockets.nsfds] = 0;
    }
    ctx->sockets.nsfds--;
    return ret;
}

bool daemon_handle_disconnect(server_t *srv, size_t client_id, bool clean)
{
    if (!clean) {
        log_warn("client %zu disconnected improperly", client_id);
    }
    else {
        char address[INET_ADDRSTRLEN] = { 0 };
        in_port_t port;
        if (!xgetpeeraddr(srv->sockets.sfds[client_id], address, &port)) {
            log_warn("unable to determine IP and port of client %zu, despite proper disconnect", client_id);
        }
        log_info("connection from %s port %d ended", address, port);
    }

    if (disconnect_client(srv, client_id)) {
        log_error("error closing socket");
        return false;
    }
    log_info("active connections: %zu", srv->sockets.nsfds);
    if (!n_party_server(srv->sockets.sfds, srv->sockets.nsfds, srv->server_key)) {
        log_fatal("catastrophic key exchange");
        return false;
    }
    return true;
}

static bool recv_client(server_t *srv, size_t sender_index)
{
    cable_t *cable = new_cable();
    ssize_t ret = xrecv(srv->sockets.sfds[sender_index], cable, sizeof(cable_header_t), 0);
    if (ret <= 0) {
        xfree(cable);
        log_trace("socket %zu disconnected", sender_index);
        return daemon_handle_disconnect(srv, sender_index, ret == 0);
    }

    size_t len = cable_recv_data(srv->sockets.sfds[sender_index], &cable);
    if (!len) {
        xfree(cable);
        return false;
    }
    log_trace("received %zu byte cable from slot %zu", len, sender_index);
    if (!transfer_message(srv, sender_index, cable)) {
        log_error("error broadcasting message from slot %zu", sender_index);
        return false;
    }
    log_debug("message fanout from slot %zu complete", sender_index);
    return true;
}

bool display_daemon_info(server_t *ctx)
{
    const char header[] = {
        "\033[32;1m===  parceld " STR(PARCEL_VERSION) "  ===\033[0m\n"
        "\033[1mMaximum active connections:\033[0m\n"
        "=> " STR(FD_SETSIZE) "\n"
        "\033[1mLocally accessible at:\033[0m\n"
    };
        // "\033[1mPublicly accessible at:\033[0m\n"
        // "=> %s:%s\n"

    // char *public_ip = xgetpublicip();
    fprintf(stdout, "%s", header);
    // printf(header, ctx->sockets.max_nsfds, public_ip ? public_ip : "error", ctx->server_port);
    // xfree(public_ip);

    if (xgetifaddrs("=> ", ctx->server_port)) {
        log_warn("failed to obtain a list of local interfaces");
        return false;
    }
    fprintf(stdout, "\033[1mDaemon started...\033[0m\n");
    return true;
}

int main_thread(void *ctx)
{
    signal(SIGINT, catch_sigint);

    server_t *server = (server_t *)ctx;
    fd_set rdy;
    FD_ZERO(&rdy);

    for (;;) {
        rdy = server->descriptors.fds;
        if (select(server->descriptors.nfds + 1, &rdy, NULL, NULL, NULL) < 0) {
            log_fatal("error waiting for selection");
            return -1;
        }

        for (size_t i = 0; i <= server->descriptors.nfds; i++) {
            sock_t fd;
            if ((fd = xfd_isset(&server->descriptors.fds, &rdy, i))) {
                if (fd == server->sockets.sfds[0]) {
                    log_debug("pending connection from unknown client");
                    switch (add_client(server)) {
                        case -1:
                            log_fatal("key exchange failure");
                            return -1;
                        case 1:
                            log_warn("incoming connection was rejected");
                            goto jmpout;
                        case 0:
                            log_debug("connection added successfully");
                            break;
                    }
                }
                else {
                    const size_t sender_index = socket_index(server, fd);
                    if (!recv_client(server, sender_index)) {
                        // [note] reason for failure logged internally
                        return -1;
                    }
                }
jmpout:;
            }
        }
    }
    return 0;
}
