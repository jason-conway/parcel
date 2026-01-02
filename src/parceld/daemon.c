/**
 * @file daemon.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief The `parcel` Daemon
 * @version 0.9.2
 * @date 2021-11-21
 *
 * @copyright Copyright (c) 2021 - 2026 Jason Conway. All rights reserved.
 *
 */

#include "daemon.h"
#include "cable.h"
#include "wire.h"

#define SRV_SOCK(srv) ((srv)->sockets.sfds[DAEMON_SOCKET])
#define SRV_SOCK_CNT(srv) ((srv)->sockets.cnt)
#define GET_SOCK(srv, i) ((srv)->sockets.sfds[(i)])
#define SET_SOCK(srv, i, sock) ((srv)->sockets.sfds[(i)] = (sock))

void catch_sigint(int sig)
{
    (void)sig;
    xalert("\nApplication aborted\n");
    exit(EXIT_FAILURE);
}

bool init_daemon(server_t *srv)
{
    // WSAStartup (Windows)
    if (xstartup()) {
        log_fatal("WSAStartup failure");
        return false;
    }

    memset(&srv->sockets, 0, sizeof(srv->sockets));

    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_PASSIVE
    };

    struct addrinfo *ai = NULL;
    if (xgetaddrinfo(NULL, srv->server_port, &hints, &ai)) {
        log_fatal("failed to get address info for port %s", srv->server_port);
        return false;
    }

    struct addrinfo *node = NULL;
    for (node = ai; node; node = node->ai_next) {
        if (!xsocket(&SRV_SOCK(srv), node->ai_family, node->ai_socktype, node->ai_protocol)) {
            continue;
        }
        if (xsetsockopt(SRV_SOCK(srv), SOL_SOCKET, SO_REUSEADDR, (int32_t []){ 1 }, sizeof(int32_t)) < 0) {
            log_fatal("failed to set socket options");
            return false;
        }
        if (bind(SRV_SOCK(srv), node->ai_addr, node->ai_addrlen) < 0) {
            if (xclose(SRV_SOCK(srv))) {
                log_fatal("unable to close server socket after failed bind");
                return false;
            }
            continue;
        }
        break;
    }

    if (!node) {
        log_fatal("unable to bind to socket");
        return false;
    }
    freeaddrinfo(ai);

    if (listen(SRV_SOCK(srv), MAX_QUEUE) < 0) {
        log_fatal("failed to begin listen with queue limit: %zu", (size_t)MAX_QUEUE);
        xclose(SRV_SOCK(srv));
        return false;
    }

    FD_ZERO(&srv->descriptors.fds);
    FD_SET(SRV_SOCK(srv), &srv->descriptors.fds);
    srv->descriptors.nfds = xfd_init_count(SRV_SOCK(srv));

    // Collect entropy for initial server key
    if (xgetrandom(srv->server_key, KEY_LEN) < 0) {
        return false;
    }
    return true;
}

// Return `i` such that `srv`->sockets[i] == `socket`
static size_t socket_index(server_t *srv, sock_t socket)
{
    for (size_t i = 1; i <= countof(srv->sockets.sfds); i++) {
        if (GET_SOCK(srv, i) == socket) {
            log_trace("got socket index %zu", i);
            return i;
        }
    }
    return 0;
}

// TODO: enumerate ACCEPT_XXX return values
static int add_client(server_t *srv)
{
    struct sockaddr_storage client_sockaddr;
    socklen_t len[] = { sizeof(struct sockaddr_storage) };
    sock_t new_client;
    if (xaccept(&new_client, SRV_SOCK(srv), (struct sockaddr *)&client_sockaddr, len) < 0) {
        log_error("unable to accept new client");
        return -1;
    }

    if (SRV_SOCK_CNT(srv) + 1 == countof(srv->sockets.sfds)) {
        log_warn("rejecting new connection");
        return 1;
    }

    FD_SET(new_client, &srv->descriptors.fds); // Add descriptor to set and update max
    srv->descriptors.nfds = xfd_count(new_client, srv->descriptors.nfds);
    srv->sockets.cnt++;

    char address[INET_ADDRSTRLEN];
    in_port_t port;
    if (!xgetpeeraddr(new_client, address, &port)) {
        log_error("unable to determine human-readable IP for new client");
        return -1;
    }

    // Copy new connection's socket to the next free slot
    log_debug("adding socket to empty slot");
    for (size_t i = 1; i < countof(srv->sockets.sfds); i++) {
        log_trace("slot[%zu]: %s", i, !GET_SOCK(srv, i) ? "free" : "in use");
        if (!GET_SOCK(srv, i)) {
            SET_SOCK(srv, i, new_client);
            log_debug("connection from %s:%u added to slot %zu", address, port, i);
            break;
        }
    }

    if (!two_party_server(new_client, srv->server_key)) {
        log_error("two-party key exchange with new client failed");
        return -1;
    }

    if (SRV_SOCK_CNT(srv) > 1) {
        log_debug("connection added - starting key regeneration");
        if (!n_party_server(srv->sockets.sfds, srv->sockets.cnt, srv->server_key)) {
            log_fatal("key regeneration failure");
            return -1;
        }
    }
    return 0;
}

static bool transfer_message(server_t *srv, size_t sender_index, cable_t *cable)
{
    size_t len = cable_get_total_len(cable);
    for (size_t i = 1; i <= SRV_SOCK_CNT(srv); i++) {
        if (i == sender_index) {
            log_trace("skipping message orgin");
            continue;
        }
        log_trace("forwarding message to socket %zu", i);
        if (!xsendall(GET_SOCK(srv, i), cable, len)) {
            return false;
        }
    }
    return true;
}

static int disconnect_client(server_t *srv, size_t client_index)
{
    FD_CLR(GET_SOCK(srv, client_index), &srv->descriptors.fds);

    const int ret = xclose(GET_SOCK(srv, client_index));

    // Replace this slot with the ending slot
    if (SRV_SOCK_CNT(srv) == 1) {
        // srv->sockets.sfds[client_index] = 0;
        SET_SOCK(srv, client_index, 0);
    }
    else {
        // srv->sockets.sfds[client_index] = srv->sockets.sfds[srv->sockets.cnt];
        SET_SOCK(srv, client_index, GET_SOCK(srv, SRV_SOCK_CNT(srv)));
        SET_SOCK(srv, SRV_SOCK_CNT(srv), 0);
        // srv->sockets.sfds[srv->sockets.cnt] = 0;
    }
    srv->sockets.cnt--;
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
        if (!xgetpeeraddr(GET_SOCK(srv, client_id), address, &port)) {
            log_warn("unable to determine IP and port of client %zu, despite proper disconnect", client_id);
        }
        log_info("connection from %s port %d ended", address, port);
    }

    if (disconnect_client(srv, client_id)) {
        log_error("error closing socket");
        return false;
    }
    log_info("active connections: %zu", srv->sockets.cnt);
    if (!n_party_server(srv->sockets.sfds, srv->sockets.cnt, srv->server_key)) {
        log_fatal("catastrophic key exchange");
        return false;
    }
    return true;
}

static bool recv_client(server_t *srv, size_t sender_index)
{
    cable_t *cable = alloc_cable();
    ssize_t ret = xrecv(GET_SOCK(srv, sender_index), cable, sizeof(cable_header_t), 0);
    if (ret <= 0) {
        xfree(cable);
        log_trace("socket %zu disconnected", sender_index);
        return daemon_handle_disconnect(srv, sender_index, ret == 0);
    }

    size_t len = cable_recv_data(GET_SOCK(srv, sender_index), &cable);
    if (!len) {
        // [note] cause logged by function
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
    };
    fprintf(stdout, "%s", header);

    fprintf(stdout, "\033[1mLocally accessible at:\033[0m\n");
    if (xgetifaddrs("=> ", ctx->server_port)) {
        log_warn("failed to obtain a list of local interfaces");
        return false;
    }

    fprintf(stdout, "\033[1mPublicly accessible at:\033[0m\n");
    char ip[INET_ADDRSTRLEN] = { 0 };
    bool ok = xgetpublicip(ip);
    fprintf(stdout, "=> %s:%s\n", ok ? ip : "error", ctx->server_port);
    fprintf(stdout, "\033[1mDaemon started...\033[0m\n");
    return true;
}

int main_thread(void *ctx)
{
    signal(SIGINT, catch_sigint);

    server_t *srv = (server_t *)ctx;
    fd_set rdy;
    FD_ZERO(&rdy);

    log_set_loglvl(LOG_TRACE);

    for (;;) {
        rdy = srv->descriptors.fds;
        if (select(srv->descriptors.nfds + 1, &rdy, NULL, NULL, NULL) < 0) {
            log_fatal("error waiting for selection");
            return -1;
        }

        for (size_t i = 0; i <= srv->descriptors.nfds; i++) {
            sock_t fd;
            if ((fd = xfd_isset(&srv->descriptors.fds, &rdy, i))) {
                if (fd == SRV_SOCK(srv)) {
                    log_debug("pending connection from unknown client");
                    switch (add_client(srv)) {
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
                    const size_t sender_index = socket_index(srv, fd);
                    if (!recv_client(srv, sender_index)) {
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
