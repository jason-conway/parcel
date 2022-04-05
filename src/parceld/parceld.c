/**
 * @file parceld.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Entry point for parcel client
 * @version 0.9.1
 * @date 2022-01-28
 *
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 *
 */

#include "daemon.h"

static void usage(FILE *f)
{
	static const char usage[] =
		"usage: parceld [-dh] [-p PORT] [-M CONNS]\n"
		"  -p PORT  start daemon on port PORT\n"
		"  -q LMAX  limit length of pending connections queue to LMAX\n"
		"  -m CMAX  limit number of active server connections to CMAX\n"
		"  -d        enable debug mode\n"
		"  -h        print this usage information\n";
	fprintf(f, "%s", usage);
}

int main(int argc, char *argv[])
{
	server_t server;
	memset(&server, 0, sizeof(server_t));

	int option;
	xgetopt_t x = { 0 };

	char port[6] = "2315";

	while ((option = xgetopt(&x, argc, argv, "dhp:q:m:")) != -1) {
		switch (option) {
			case 'p':
				if (xport_valid(x.arg)) {
					memcpy(port, x.arg, strlen(x.arg));
				}
				break;
			case 'h':
				usage(stdout);
				exit(EXIT_SUCCESS);
			case ':':
				printf("Option is missing an argument\n");
				exit(EXIT_FAILURE);
		}
	}

	configure_server(&server, port);
	return main_thread((void *)&server);
}
