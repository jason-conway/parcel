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

static int proc_file(void *data)
{
	struct wire_file_message *wire_file = (struct wire_file_message *)data;
	printf("\n\033[1mReceived file \"%s\"\033[0m\n", wire_file->filename);
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

static void proc_text(uint8_t *wire_data)
{
	printf("\033[2K\r%s\n", (char *)wire_data);
}

static int proc_ctrl(client_t *ctx, void *data)
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

/**
 * @brief Process a received and (successfully) decrypted wire
 * 
 * @param ctx Client context
 * @param wire Wire to process
 * @return Returns a wire_type enum on success, otherwise -1
 */
int proc_type(client_t *ctx, wire_t *wire)
{
	enum wire_type type = wire_get_type(wire);
	switch (type) {
		case TYPE_CTRL: // Forward wire along to proc_ctrl()
			switch (proc_ctrl(ctx, wire->data)) {
				case CTRL_EXIT:
					xfree(wire);
					exit(EXIT_FAILURE); // TODO: figure something out for this
				case CTRL_DHKE:
					xmemcpy_locked(&ctx->shctx->mutex_lock, &ctx->shctx->keys, &ctx->keys, sizeof(parcel_keys_t));
					xmemcpy_locked(&ctx->shctx->mutex_lock, &ctx->shctx->conn_announced, &ctx->conn_announced, sizeof(bool));
					break;
			}
			break;
		case TYPE_FILE:
			if (proc_file(wire->data)) {
				xalert("proc_file()\n");
				return -1;
			}
			break;
		case TYPE_TEXT:
			proc_text(wire->data);
			disp_username(&ctx->username);
			break;
	}
	return type;
}
