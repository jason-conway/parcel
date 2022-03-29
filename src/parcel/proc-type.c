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

int proc_file(uint8_t *data, size_t length)
{
	char filename[32];
	memset(filename, 0, FILENAME_LEN);
	size_t filename_length = strnlen((char *)data, FILENAME_LEN);
	memcpy(filename, data, filename_length);

	FILE *file = fopen(filename, "wb");
	if (!file) {
		xprintf(RED, "Could not open file \"%s\" for writing\n", filename);
		return -1;
	}

	const size_t file_data_size = length - FILENAME_LEN;
	if (fwrite(&data[FILENAME_LEN], 1, file_data_size, file) != file_data_size) {
		xprintf(RED, "Error writing to file \"%s\"\n", filename);
		(void)fclose(file);
		return -1;
	}

	if (fflush(file) || fclose(file)) {
		return -1;
	}

	return 0;
}

void proc_text(client_t *ctx, uint8_t *wire_data)
{
	printf("\033[2K\r%s\n%s: ", wire_data, ctx->username);
	fflush(stdout);
}

int proc_ctrl(client_t *ctx, uint8_t *wire_data)
{
	size_t rounds = wire_get_raw(wire_data);
	memcpy(ctx->ctrl_key, &wire_data[16], KEY_LEN);
	return n_party_client(ctx->socket, rounds, ctx->session_key, ctx->fingerprint);
}
