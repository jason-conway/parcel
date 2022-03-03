/**
 * @file main.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Parcel entry point
 * @version 0.1
 * @date 2022-01-15
 *
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 *
 */

#include "client.h"

void catch_sigint(int sig)
{
	(void)sig;
	fprintf(stderr, "\033[31m\nApplication aborted\n\033[0m");
	exit(EXIT_FAILURE);
}

static void usage(FILE *f)
{
	static const char usage[] =
		"usage: parcel [-hd] [-a ADDR] [-p PORT] [-u NAME]\n"
		"  -a ADDR  server address (www.example.com, 111.222.333.444)\n"
		"  -p PORT  server port (3724, 9216)\n"
		"  -u NAME  username displayed alongside sent messages\n"
		"  -h       print this usage information\n"
		"  -d       enable debug mode (verbose)\n";
	fprintf(f, "%s", usage);
}

int main(int argc, char **argv)
{
	signal(SIGINT, catch_sigint);

	char *address = NULL;
	char *port = NULL;
	char *username = NULL;
	
	int option;
	xgetopt_t x = { 0 };
	while ((option = xgetopt(&x, argc, argv, "ha:p:u:")) != -1) {
		switch (option) {
			case 'a':
				address = x.arg;
				break;
			case 'p':
				port = x.arg;
				break;
			case 'u':
				username = x.arg;
				break;
			case 'd':
				// TODO
				break;
			case 'h':
				usage(stdout);
				exit(EXIT_SUCCESS);
			default:
				usage(stderr);
				exit(EXIT_FAILURE);
		}
	}

	client_t client = { .mutex_lock = PTHREAD_MUTEX_INITIALIZER };
	pthread_mutex_init(&client.mutex_lock, NULL);

	size_t username_length = 0;
	if ((username_length = strlen(username) + 1) > USERNAME_MAX_LENGTH) {
		fprintf(stderr, "Username is too long\n");
	}
	memcpy(client.username, username, username_length);
	
	connect_server(&client, address, port);
	pthread_t recv_ctx;
	pthread_create(&recv_ctx, NULL, recv_thread, (void *)&client);

	// Handle sending from main thread
	return send_thread((void *)&client);
}
