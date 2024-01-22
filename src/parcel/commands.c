/**
 * @file commands.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief parcel client commands
 * @version 0.9.2
 * @date 2022-03-14
 *
 * @copyright Copyright (c) 2022 - 2024 Jason Conway. All rights reserved.
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
	*message = xstrcat(5, "\033[1m", ctx->username.data, " has changed their username to ", new_username, "\033[0m");
	if (!message) {
		*message_length = 0;
		*message = NULL;
		xfree(new_username);
		return -1;
	}

	*message_length = strlen(*message);
	ctx->username.length = new_username_length;

	memset(&ctx->username.data, 0, USERNAME_MAX_LENGTH);
	memcpy(&ctx->username.data, new_username, ctx->username.length);
	xfree(new_username);
	return 0;
}

// TODO: This should not be fatal
static int cmd_send_file(char **message, size_t *message_length)
{
	size_t path_length = FILE_PATH_MAX_LENGTH;
	char *file_path = xprompt("> File path: ", "path", &path_length);

	if (!xfexists(file_path)) {
		xwarn("> File \"%s\" not found\n", file_path);
		goto free_path;
	}

	size_t file_size = xfilesize(file_path);
	if (!file_size) {
		xwarn("> Unable to determine size of file \"%s\"\n", file_path);
		goto free_path;
	}

	if (file_size > FILE_DATA_MAX_SIZE) {
		const size_t overage = file_size - FILE_DATA_MAX_SIZE;
		xwarn("> File \"%s\" is %zu bytes over the maximum size of %d bytes\n", file_path, overage, FILE_DATA_MAX_SIZE);
		goto free_path;
	}

	*message_length = file_size + sizeof(struct wire_file_message); // To hold file name and data
	struct wire_file_message *file_contents = xcalloc(*message_length);
	if (!file_contents) {
		goto free_all;
	}
	
	wire_set_raw(file_contents->filesize, file_size);
	
	FILE *file = fopen(file_path, "rb");
	if (!file) {
		xwarn("> Could not open file \"%s\" for reading\n", file_path);
		goto free_all;
	}

	char filename[FILE_NAME_LEN];
	memset(filename, 0, sizeof(filename));

	const size_t filename_length = xbasename(file_path, filename) + 1;
	memcpy(file_contents->filename, filename, filename_length);
	
	// Now read in the file contents
	if (fread(file_contents->filedata, 1, file_size, file) != file_size) {
		xwarn("> Error reading contents of file\n");
		(void)fclose(file);
		goto free_all;
	}

	(void)fclose(file);

	xfree(*message);
	*message = (char *)file_contents;
	return 0;

free_all:
	xfree(file_contents);
free_path:
	xfree(file_path);
	return -1;
}

static void cmd_print_enc_info(uint8_t *session, uint8_t *control)
{
	printf("Session Key: ");
	fflush(stdout);
	xmemprint(session, KEY_LEN);
	printf("Control Key: ");
	fflush(stdout);
	xmemprint(control, KEY_LEN);
}

static void cmd_clear(void)
{
	clear_screen();
}

static inline int cmd_not_found(char *message)
{
	xwarn("Unrecognized command, \"%s\"\n", message);
	return 0;
}

int cmd_exit(client_t *ctx, char **message, size_t *message_length)
{
	xfree(*message);

	*message = xstrcat(3, "\033[1m", ctx->username.data, " is offline\033[0m");
	if (!*message) {
		*message = NULL;
		return -1;
	}

	*message_length = strlen(*message);
	return 0;
}

int cmd_set_ambiguous_width(void)
{
	size_t len = 2; // single digit plus null
	char *new_width = xprompt("> Ambiguous character width: ", "width", &len);
	if (!new_width) {
		return -1;
	}
	long width = 0;
	if (!xstrrange(new_width, &width, 1, 2)) {
		xwarn("Width of \'%s\' is not\n", new_width);
		xfree(new_width);
		return -1;
	}
	xfree(new_width);
	set_ambiguous_width(width);
	return 0;
}

static int cmd_list(void)
{
	static const char list[] =
		"parcel commands:\n"
		"  /list         list available commands\n"
		"  /x            exit the server and close parcel\n"
		"  /username     change username\n"
		"  /encinfo      display current encryption parameters\n"
		"  /file         send a file\n"
		"  /clear        clear the screen\n"
		"  /version      print build version\n"
		"  /ambwidth     set ambiguous character width\n";
	return !printf("%s", list);
}

static void cmd_version(void)
{
	xprintf(DEF, BOLD, "%s\n", "parcel " STR(PARCEL_VERSION));
}

static int cmd_ambiguous(void)
{
	xwarn("Ambiguous command entered\n");
	return cmd_list();
}

static enum command_id parse_command(char *command)
{
	static const char *command_strings[] = {
		"/list", "/x", "/username", "/encinfo", "/file", "/clear", "/version", "/ambwidth"
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

int parse_input(client_t *ctx, enum command_id *cmd, char **message, size_t *message_length)
{
	if (*message[0] != '/') { // Fast return
		prepend_username(ctx->username.data, message, message_length);
		if (!*message) {
			return -1;
		}
		return SEND_TEXT;
	}
	else {
		*cmd = parse_command(*message);
		switch (*cmd) {
			case CMD_AMBIGUOUS:
				return cmd_ambiguous() ? -1 : SEND_NONE;
			case CMD_LIST:
				return cmd_list() ? -1 : SEND_NONE;
			case CMD_EXIT:
				return cmd_exit(ctx, message, message_length) ? -1 : SEND_TEXT;
			case CMD_USERNAME:
				return cmd_username(ctx, message, message_length) ? -1 : SEND_TEXT;
			case CMD_ENC_INFO:
				cmd_print_enc_info(ctx->keys.session, ctx->keys.ctrl);
				return SEND_NONE;
			case CMD_FILE:
				return cmd_send_file(message, message_length) ? SEND_NONE : SEND_FILE;
			case CMD_CLEAR:
				cmd_clear();
				return SEND_NONE;
			case CMD_VERSION:
				cmd_version();
				return SEND_NONE;
			case CMD_AMBIGUOUS_WIDTH:
				return cmd_set_ambiguous_width() ? -1 : SEND_NONE;
			default:
				return cmd_not_found(*message) ? -1 : SEND_NONE;
		}
	}
}
