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

#include "client.h"

static void catch_sigint(int sig)
{
	(void)sig;
	xprintf(RED, "\nAborting application\n");
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
		"  -h       print this usage information\n"
		"  -d       enable debug mode (verbose)\n";
	fprintf(f, "%s", usage);
}

int main(int argc, char **argv)
{
	signal(SIGINT, catch_sigint);

	char address[ADDRESS_MAX_LENGTH];
	memset(address, 0, ADDRESS_MAX_LENGTH);
	
	char port[PORT_MAX_LENGTH] = "2315";
	
	client_t client = { .mutex_lock = PTHREAD_MUTEX_INITIALIZER };
	pthread_mutex_init(&client.mutex_lock, NULL);
		
	int option;
	xgetopt_t x = { 0 };
	while ((option = xgetopt(&x, argc, argv, "lha:p:u:")) != -1) {
		switch (option) {
			case 'a':
				if (strlen(x.arg) < ADDRESS_MAX_LENGTH) {
					memcpy(address, x.arg, strlen(x.arg));
					break;
				}
				fatal("server address too long");
			case 'p':
				if (strlen(x.arg) < PORT_MAX_LENGTH) {
					memcpy(port, x.arg, strlen(x.arg));
					break;
				}
				fatal("server port too long");
			case 'u':
				if (strlen(x.arg) < USERNAME_MAX_LENGTH) {
					memcpy(client.username, x.arg, strlen(x.arg));
					break;
				}
				fatal("username too long");
			case 'l': 
				if (xgetusername(client.username, USERNAME_MAX_LENGTH)) {
					fatal("unable to determine device username");
				}
				break;
			case 'd':
				printf("> Option 'd' is not implemented yet\n");
				break;
			case 'h':
				usage(stdout);
				exit(EXIT_SUCCESS);
			case ':':
				printf("> Option is missing an argument\n");
				exit(EXIT_FAILURE);
		}
	}
	if (argc < 5) {
		prompt_args(address, client.username);
	}
	
	connect_server(&client, address, port);
	pthread_t recv_ctx;
	pthread_create(&recv_ctx, NULL, recv_thread, (void *)&client);

	// Handle sending from main thread
	return send_thread((void *)&client);
}
