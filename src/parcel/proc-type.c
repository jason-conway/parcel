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

int proc_file(void *data)
{
	struct wire_file_message *wire_file = (struct wire_file_message *)data;

	char *save_path = xget_dir(wire_file->filename);
	if (!save_path) {
		return -1;
	}
	FILE *file = fopen(save_path, "wb");
	if (!file) {
		xwarn("> Could not open file \"%s\" for writing\n", wire_file->filename);
		return -1;
	}
	xfree(save_path);

	const size_t file_data_size = wire_pack64(wire_file->filesize);
	if (fwrite(wire_file->filedata, 1, file_data_size, file) != file_data_size) {
		xwarn("> Error writing to file \"%s\"\n", wire_file->filename);
		(void)fclose(file);
		xfree(wire_file->filename);
		return -1;
	}

	if (fflush(file) || fclose(file)) {
		xwarn("> Error closing file \"%s\"\n", wire_file->filename);
		return -1;
	}

	return 0;
}

void proc_text(uint8_t *wire_data)
{
	printf("\033[2K\r%s\n", (char *)wire_data);
}

int proc_ctrl(client_t *ctx, void *data)
{
	struct wire_ctrl_message *wire_ctrl = (struct wire_ctrl_message *)data;

	memcpy(ctx->keys.ctrl, wire_ctrl->renewed_key, KEY_LEN);

	switch (wire_get_ctrl_function(wire_ctrl)) {
		case CTRL_EXIT:
			return CTRL_EXIT;
		case CTRL_DHKE:
			switch (n_party_client(ctx->socket, ctx->keys.session, wire_get_ctrl_args(wire_ctrl))) {
				case DHKE_OK:
					if (!ctx->conn_announced) {
						if (announce_connection(ctx)) {
							return -1;
						}
					}
					return CTRL_DHKE;
				case DHKE_ERROR:
					return DHKE_ERROR;
			}
	}
	return -1;
}
