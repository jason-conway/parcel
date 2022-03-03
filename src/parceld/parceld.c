/**
 * @file parceld.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Parcel Daemon
 * @version 0.1
 * @date 2022-01-28
 * 
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 * 
 */

#include "daemon.h"

static void usage(FILE *f)
{
	static const char usage[] =
		"usage: parceld [-p PORT] [-s SEED]\n"
		"  -p PORT  server port (3724, 9216)\n"
		"  -h       print this usage information\n";
	fprintf(f, "%s", usage);
}

int main(int argc, char *argv[])
{
	server_t server;
	memset(&server, 0, sizeof(server_t));

	int option;
	xgetopt_t x = { 0 };
	char *port = NULL;
	while ((option = xgetopt(&x, argc, argv, "hp:")) != -1) {
		switch (option) {
			case 'p':
				port = x.arg;
				break;
			case 'h':
				usage(stdout);
				exit(EXIT_SUCCESS);
			default:
				usage(stderr);
				exit(EXIT_FAILURE);
		}
	}

	configure_server(&server, port);
	return main_thread((void *)&server);
}
