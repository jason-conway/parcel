/**
 * @file parceld.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Entry point for parcel daemon
 * @version 0.9.2
 * @date 2022-01-28
 *
 * @copyright Copyright (c) 2022 - 2024 Jason Conway. All rights reserved.
 *
 */

#include "daemon.h"

static void usage(FILE *f)
{
    static const char usage[] =
        "usage: parceld [-h] [-p PORT] [-m CMAX] [-q LMAX]\n"
        "  -p PORT  start daemon on port PORT\n"
        "  -q LMAX  limit length of pending connections queue to LMAX\n"
        "  -m CMAX  limit number of active server connections to CMAX\n"
        "  -h        print this usage information\n"
        "  -v        print build version\n";
    fprintf(f, "%s", usage);
}

int main(int argc, char **argv)
{
    server_t server = {
        .server_port = "2315",
        .max_queue = MAX_QUEUE,
        .sockets.sfds = NULL,
        .sockets.nsfds = 0,
        .sockets.max_nsfds = SUPPORTED_CONNECTIONS,
    };

    xgetopt_t xgo = { 0 };
    for (ptrdiff_t opt = -1; (opt = xgetopt(&xgo, argc, argv, "hvp:q:m:")) != -1;) {
        switch (opt) {
            case 'p':
                if (xstrrange(xgo.arg, NULL, 0, 65535)) {
                    memcpy(server.server_port, xgo.arg, strlen(xgo.arg));
                }
                break;
            case 'q':
                if (xstrrange(xgo.arg, (long *)&server.max_queue, 0, MAX_QUEUE)) {
                    log_info("using a max queue of %zu", server.max_queue);
                    break;
                }
                xwarn("Specified queue limit is outside allowed range\n");
                xwarn("Using default maximum, %u\n", MAX_QUEUE);
                break;
            case 'm':
                if (xstrrange(xgo.arg, (long *)&server.sockets.max_nsfds, 0, SUPPORTED_CONNECTIONS)) {
                    log_info("connection max set to %zu", server.sockets.max_nsfds);
                    break;
                }
                xwarn("Specified connection limit is outside allowed range\n");
                xwarn("Using default maximum, %u\n", SUPPORTED_CONNECTIONS);
                break;
            case 'h':
                usage(stdout);
                return 0;
            case 'v':
                fprintf(stdout, "\033[1mparcel " STR(PARCEL_VERSION) "\033[0m\n");
                return 0;
            case ':':
                fprintf(stderr, "option is missing an argument\n");
                return 1;
            default:
                usage(stderr);
                return -1;
        }
    }

    if (!init_daemon(&server)) {
        return 1;
    }

    if (!display_daemon_info(&server)) {
        return 1;
    }

    return main_thread((void *)&server);
}
