/**
 * @file commands.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief parcel client commands
 * @version 0.9.1
 * @date 2022-03-14
 *
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 *
 */

#include "client.h"

// Prepend client username to message string
static void prepend_username(char *username, char **plaintext, size_t *plaintext_length)
{
	bool error = true;
	char *named_message = xstrcat(3, username, ": ", *plaintext);
	if (named_message) {
		*plaintext_length = strlen(named_message);
		error = false;
	}
	xfree(*plaintext);
	*plaintext = error ? NULL : named_message;
}

static int cmd_username(client_t *ctx, char **message, size_t *message_length)
{
	xfree(*message);

	size_t new_username_length = USERNAME_MAX_LENGTH;
	char *new_username = xprompt("> New username: ", "username", &new_username_length);
	*message = xstrcat(5, "\033[1m", ctx->username, " has changed their username to ", new_username, "\033[0m");
	if (!message) {
		*message_length = 0;
		*message = NULL;
		xfree(new_username);
		send_connection_status(ctx, true);
		return -1;
	}

	*message_length = strlen(*message);
	memset(ctx->username, 0, USERNAME_MAX_LENGTH);
	memcpy(ctx->username, new_username, new_username_length);
	xfree(new_username);
	return 0;
}

// TODO: Break parts of this into their own function
static int cmd_send_file(client_t *ctx, char **message, size_t *message_length)
{
	size_t path_length = FILE_PATH_MAX_LENGTH;
	char *file_path = xprompt("> File path: ", "path", &path_length);

	if (!xfexists(file_path)) {
		xwarn("File \"%s\" not found\n", file_path);
		goto free_path_only;
	}

	size_t file_size = xfilesize(file_path);
	if (!file_size) {
		xwarn("Unable to determine size of file \"%s\"\n", file_path);
		goto free_path_only;
	}

	if (file_size > FILE_DATA_MAX_SIZE) {
		const size_t overage = file_size - FILE_DATA_MAX_SIZE;
		xwarn("File \"%s\" is %zu bytes over the maximum size of %d bytes\n", file_path, overage, FILE_DATA_MAX_SIZE);
		goto free_path_only;
	}

	*message_length = file_size + FILE_HEADER_SIZE; // To hold file name and data
	uint8_t *file_contents = xcalloc(*message_length);
	if (!file_contents) {
		goto error;
	}

	FILE *file = fopen(file_path, "rb");
	if (!file) {
		xwarn("Could not open file \"%s\" for reading\n", file_path);
		goto error;
	}

	// First 64 bytes are file name
	char *filename = xbasename(file_path);
	memcpy(&file_contents[FILE_NAME_START], filename, FILE_NAME_LEN);
	file_contents[FILE_NAME_LEN - 1] = '\0'; // Ensure filename string is null-terminated

	// Next 16 bytes hold the actual file size. Only the first 8 bytes are set
	wire_unpack64(&file_contents[FILE_SIZE_START], file_size);

	// Now read in the file contents
	if (fread(&file_contents[FILE_DATA_START], 1, file_size, file) != file_size) {
		xwarn("Error reading contents of file \"%s\"\n", filename);
		(void)fclose(file);
		goto error;
	}

	(void)fclose(file);

	xfree(file_path);
	xfree(*message);

	*message = (char *)file_contents;

	return 0;

	error:
		xfree(file_contents);
	free_path_only:
		xfree(file_path);
		send_connection_status(ctx, true);
		return -1;
}

static inline int cmd_print_fingerprint(const uint8_t *fingerprint)
{
	print_fingerprint("Fingerprint is: ", fingerprint);
	return 0;
}

static inline int cmd_not_found(char *message)
{
	xwarn("Unrecognized command, \"%s\"\n", message);
	return 0;
}

noreturn void cmd_exit(client_t *ctx, char *message)
{
	xfree(message);
	send_connection_status(ctx, true);
	exit(EXIT_SUCCESS);
}

static int cmd_list(void)
{
	static const char list[] =
		"parcel commands:\n"
		"  :list         list available commands\n"
		"  :q            leave the server\n"
		"  :username     change username\n"
		"  :fingerprint  display fingerprint the current public key\n"
		"  :file         send a file\n";
	printf("%s", list);
	return 0;
}

static int cmd_ambiguous(void)
{
	xwarn("Ambiguous command entered\n");
	cmd_list();
	return 0;
}

static enum command_id parse_command(char *command)
{
	static const char *command_strings[] = {
		":list\n", ":q\n", ":username\n", ":fingerprint\n", ":file\n"
	};

	const size_t commands = sizeof(command_strings) / sizeof(*command_strings);

	enum command_id cmd = CMD_NONE;

	const size_t len = strlen(command);
	for (size_t i = 1; i <= commands; i++) {
		if (!strncmp(command, command_strings[i - 1], len)) {
			if (cmd) {
				return CMD_AMBIGUOUS;
			}
			cmd = i;
		}
	}
	return cmd;
}

int parse_input(client_t *ctx, char **message, size_t *message_length)
{
	if (*message[0] != ':') { // Fast return
		prepend_username(ctx->username, message, message_length);
		if (!*message) {
			return -1;
		}
		return SEND_TEXT;
	}
	else {
		switch (parse_command(*message)) {
			case CMD_AMBIGUOUS:
				return cmd_ambiguous() ? -1 : SEND_NONE;
			case CMD_LIST:
				return cmd_list() ? -1 : SEND_NONE;
			case CMD_EXIT:
				cmd_exit(ctx, *message);
			case CMD_USERNAME:
				return cmd_username(ctx, message, message_length) ? -1 : SEND_TEXT;
			case CMD_FINGERPRINT:
				return cmd_print_fingerprint(ctx->fingerprint) ? -1 : SEND_NONE;
			case CMD_FILE:
				return cmd_send_file(ctx, message, message_length) ? -1 : SEND_FILE;
			default:
				return cmd_not_found(*message) ? -1 : SEND_NONE;
		}
	}
}
