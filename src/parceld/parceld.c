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
	server_t server = {
		.server_port = "2315"
	};

	int option;
	xgetopt_t x = { 0 };

	while ((option = xgetopt(&x, argc, argv, "dhp:q:m:")) != -1) {
		switch (option) {
			case 'p':
				if (xport_valid(x.arg)) {
					memcpy(server.server_port, x.arg, strlen(x.arg));
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

	if (!configure_server(&server)) {
		return 1;
	}
	
	server_ready(&server);
	return main_thread((void *)&server);
}
