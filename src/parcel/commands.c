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

static int cmd_username(client_t *ctx, char **message, size_t *message_length)
{
	free(*message);
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

static bool file_exists(char *filename)
{
	FILE *f = fopen(filename, "r");
	if (f) {
		(void)fclose(f);
		return true;
	}
	return false;
}

// TODO
static int cmd_send_file(client_t *ctx, char **message, size_t *message_length)
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
	if (file_exists(file_path)) {
		
	}
	return 0;
}

static inline void cmd_print_fingerprint(const uint8_t *fingerprint)
{
	print_fingerprint("Fingerprint is: ", fingerprint);
}

static inline void cmd_exit(client_t *ctx, char *message)
{
	free(message);
	send_connection_status(ctx, true);
	exit(EXIT_SUCCESS);
}

static enum command_id parse_command(char *command)
{
	static const char *command_strings[] = {
		":q\n", ":username\n", ":fingerprint\n", ":file\n"
	};
	
	const size_t len = strlen(command);
	const size_t commands = sizeof(command_strings) / sizeof(*command_strings);

	for (size_t i = 1; i <= commands; i++) {
		if (!strncmp(command, command_strings[i - 1], len)) {
			return i;
		}
	}
	return -1;
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
	else {
		switch (parse_command(*message)) {
			case CMD_EXIT:
				cmd_exit(ctx, *message);
			case CMD_USERNAME:
				return cmd_username(ctx, message, message_length) ? -1 : CMD_USERNAME;
			case CMD_FINGERPRINT:
				cmd_print_fingerprint(ctx->fingerprint);
				return CMD_FINGERPRINT;
			case CMD_FILE:
				cmd_send_file(ctx, message, message_length);
				return CMD_FILE;
			default:
				return -1;
		}
	}
}
