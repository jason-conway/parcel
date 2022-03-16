/**
 * @file commands.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief parcel client commands
 * @version 0.1
 * @date 2022-03-14
 * 
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 * 
 */

#include "client.h"

static void prepend_username(client_t *ctx, char **message, size_t *message_length)
{
	// Prepend username to the input string
	*message_length += strlen(ctx->username) + 2; // +2 for ": "

	char *msg = malloc(*message_length);
	if (!msg) {
		free(*message);
		*message = NULL;
		return;
	}

	if (snprintf(msg, *message_length, "%s: %s", ctx->username, *message) != (int)*message_length) {
		free(*message);
		*message = NULL;
		return;
	}

	free(*message);
	*message = msg;
}

static int change_username(client_t *ctx, char **message, size_t *message_length)
{
	char *new_username = NULL;
	size_t new_username_length = 0;
	do {
		printf("> New username: ");
		fflush(stdout);
		xgetline(&new_username, &new_username_length, stdin);
	} while (!new_username_length);
	new_username[new_username_length - 1] = '\0'; // remove \n

	static const char *template = "\033[33m%s has changed their username to %s\033[0m";
	const size_t msg_length = strlen(template) + new_username_length + strlen(ctx->username) + 1;
	char *msg = malloc(msg_length);
	if (!msg) {
		send_connection_status(ctx, true);
		return -1;
	}
	if (snprintf(msg, msg_length, template, ctx->username, new_username) < 0) {
		send_connection_status(ctx, true);
		return -1;
	}

	*message = msg;
	*message_length = msg_length;
	memset(ctx->username, 0, USERNAME_MAX_LENGTH);
	memcpy(ctx->username, new_username, new_username_length);
	free(new_username);
	return 0;
}

// TODO
static int send_file(client_t *ctx, char **message, size_t *message_length)
{
	(void)ctx;
	(void)message;
	(void)message_length;

	char *file_path = NULL;
	size_t path_length = 0;
	do {
		printf("> File Path: ");
		fflush(stdout);
		xgetline(&file_path, &path_length, stdin);
	} while (!path_length);
	file_path[path_length - 1] = '\0'; // remove \n
	return 0;
}

// This needs work later
int parse_input(client_t *ctx, char **message, size_t *message_length)
{
	if (*message[0] != ':') { // Fast return
		prepend_username(ctx, message, message_length);
		if (!(*message)) {
			return -1;
		}
		return CMD_NONE;
	}
	else if (!strcmp(*message, ":q\n")) {
		free(*message);
		send_connection_status(ctx, true);
		pthread_exit(NULL);
		exit(EXIT_SUCCESS);
	}
	else if (!strcmp(*message, ":username\n")) {
		free(*message);
		if (change_username(ctx, message, message_length)) {
			send_connection_status(ctx, true); // may not work
			return -1;
		}
		return CMD_USERNAME;
	}
	else if (!strcmp(*message, ":fingerprint\n")) {
		fprint("Fingerprint is: ", ctx->fingerprint);
		return CMD_FINGERPRINT;
	}
	else if (!strcmp(*message, ":file\n")) {
		send_file(ctx, message, message_length);
		return CMD_FINGERPRINT;
	}
	else {
		return -1;
	}
}
