/**
 * @file proc-type.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief
 * @version 0.9.1
 * @date 2022-03-28
 *
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 *
 */

#include "client.h"

int proc_file(uint8_t *data)
{
	size_t filename_length = strnlen((char *)data, FILE_NAME_LEN);
	char *filename = xcalloc(filename_length);
	if (!filename) {
		return -1;
	}
	memcpy(filename, &data[FILE_NAME_START], filename_length);

	FILE *file = fopen(filename, "wb");
	if (!file) {
		xwarn("Could not open file \"%s\" for writing\n", filename);
		return -1;
	}

	const size_t file_data_size = wire_pack64(&data[FILE_SIZE_START]);
	if (fwrite(&data[FILE_DATA_START], 1, file_data_size, file) != file_data_size) {
		xwarn("Error writing to file \"%s\"\n", filename);
		(void)fclose(file);
		(void)remove(filename);
		xfree(filename);
		return -1;
	}

	if (fflush(file) || fclose(file)) {
		xwarn("Error closing file \"%s\"\n", filename);
		free(filename);
		return -1;
	}

	return 0;
}

void proc_text(uint8_t *wire_data)
{
	printf("\033[2K\r%s\n", (char *)wire_data);
}

int proc_ctrl(client_t *ctx, uint8_t *wire_data)
{
	size_t rounds = wire_get_raw(&wire_data[0]);
	memcpy(ctx->keys.ctrl, &wire_data[CTRL_KEY_OFFSET], KEY_LEN);
	return n_party_client(ctx->socket, ctx->keys.session, rounds);
}
