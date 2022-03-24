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

// Prepend client username to message string
static void prepend_username(client_t *ctx, char **message, size_t *message_length)
{
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

// TODO
static int cmd_send_file(client_t *ctx, char **message, size_t *message_length)
{
	(void)ctx;
	(void)message_length;
	
	char *file_path = NULL;
	size_t path_length = 0;
	do {
		printf("> File Path: ");
		fflush(stdout);
		xgetline(&file_path, &path_length, stdin);
	} while (!path_length);
	file_path[path_length - 1] = '\0'; // remove \n
	
	// Check if file is valid
	// Open file
	// Read file into buffer at *message
	// Update message_length

	if (!xfexists(file_path)) {
		xprintf(YEL, "File \"%s\" not found\n", file_path);
		return -1;
	}

	size_t file_size = xfsize(file_path); 
	if (!file_size) {
		xprintf(YEL, "Unable to determine size of file \"%s\"\n", file_path);
		return -1;
	}
	// TODO: make enum
	const size_t max_file = DATA_LEN_MAX - BLOCK_LEN;
	if (file_size > max_file) {
		xprintf(YEL, "File \"%s\" is %zu bytes over the maximum supported size of %d bytes\n", file_path, (size_t)(file_size - max_file), max_file);
		return -1;
	}

	uint8_t *file_contents = malloc(file_size + BLOCK_LEN);
	if (!file_contents) {
		return -1;
	}
	FILE *file = fopen(file_path, "rb");
	if (!file) {
		xprintf(RED, "Could not open file \"%s\" for reading\n", file_path);
		return -1;
	}
	
	// First block will be the file name
	memcpy(&file_contents[0], file_path, BLOCK_LEN);

	if (fread(&file_contents[16], 1, file_size, file) != file_size) {
		xprintf(RED, "Error reading contents of file \"%s\"\n", file_path);
		return -1;
	}
	(void)fclose(file);
	free(*message);
	*message = (char *)file_contents;
	return 0;
}

static inline void cmd_print_fingerprint(const uint8_t *fingerprint)
{
	print_fingerprint("Fingerprint is: ", fingerprint);
}

_Noreturn static void cmd_exit(client_t *ctx, char *message)
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
