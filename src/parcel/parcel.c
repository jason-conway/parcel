/**
 * @file parcel.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Entry point for parcel client
 * @version 0.9.1
 * @date 2022-01-15
 *
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 *
 */

// TODO: console - return a "back" code when backspacing on nothing
// TODO: console - utf-8 decoder? (Windows)
// TODO: test file sending both platforms

#include "client.h"

static void catch_sigint(int sig)
{
	(void)sig;
	xprintf(RED, BOLD, "\nAborting application\n");
	exit(EXIT_FAILURE);
}

static void usage(FILE *f)
{
	static const char usage[] =
		"usage: parcel [-hd] [-a ADDR] [-p PORT] [-u NAME]\n"
		"  -a ADDR  server address (www.example.com, 111.222.333.444)\n"
		"  -p PORT  server port (3724, 9216)\n"
		"  -u NAME  username displayed alongside sent messages\n"
		"  -l       use computer login as username\n"
		"  -h       print this usage information\n";
	fprintf(f, "%s", usage);
}

int main(int argc, char **argv)
{
	signal(SIGINT, catch_sigint);
	
	char address[ADDRESS_MAX_LENGTH];
	memset(address, 0, ADDRESS_MAX_LENGTH);
	
	char port[PORT_MAX_LENGTH] = "2315";
	
	client_t *client = xcalloc(sizeof(client_t));
	if (!client) {
		xalert("Error allocating client memory\n");
		return 1;
	}

	pthread_mutex_init(&client->mutex_lock, NULL);

	int option;
	xgetopt_t x = { 0 };
	while ((option = xgetopt(&x, argc, argv, "lha:p:u:")) != -1) {
		switch (option) {
			case 'a':
				if (strlen(x.arg) < ADDRESS_MAX_LENGTH) {
					memcpy(address, x.arg, strlen(x.arg));
					break;
				}
				xwarn("Address argument too long\n");
				break;
			case 'p':
				if (xport_valid(x.arg)) {
					memcpy(port, x.arg, strlen(x.arg));
					break;
				}
				xwarn("Using default port: 2315\n");
				break;
			case 'u':
				if (strlen(x.arg) < USERNAME_MAX_LENGTH) {
					memcpy(client->username, x.arg, strlen(x.arg));
					break;
				}
				xwarn("Username argument too long\n");
				break;
			case 'l': 
				if (xgetlogin(client->username, USERNAME_MAX_LENGTH)) {
					xwarn("Unable to determine login name\n");
				}
				break;
			case 'h':
				usage(stdout);
				return 0;
			case ':':
				printf("Option is missing an argument\n");
				return -1;
			default:
				usage(stderr);
				return -1;
		}
	}
	
	if (argc < 5) {
		prompt_args(address, client->username);
	}
	
	if (connect_server(client, address, port)) {
		return -1;
	}

	pthread_t recv_ctx;
	if (pthread_create(&recv_ctx, NULL, recv_thread, (void *)client)) {
		xalert("Unable to create receiver thread\n");
		return -1;
	}

	int thread_status[2] = { 0, 0 };

	thread_status[0] = send_thread((void *)client);

	if (pthread_join(recv_ctx, (void **)&thread_status[1])) {
		xalert("Unable to join receiver thread\n");
		return -1;
	}
	
	return thread_status[0] | thread_status[1];
}
