/**
 * @file proc-type.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Process decrypted wires (client-side)
 * @version 0.9.2
 * @date 2022-03-28
 *
 * @copyright Copyright (c) 2022 - 2024 Jason Conway. All rights reserved.
 *
 */

#include "client.h"

static bool proc_file(void *data)
{
    file_msg_t *wire_file = data;
    printf("\n\033[1mReceived file \"%s\"\033[0m\n", wire_file->filename);
    char *save_path = xget_dir(wire_file->filename);
    if (!save_path) {
        return false;
    }
    FILE *file = fopen(save_path, "wb");
    if (!file) {
        xwarn("> Could not open file \"%s\" for writing\n", wire_file->filename);
        return false;
    }
    xfree(save_path);

    const size_t file_data_size = file_msg_get_filesize(wire_file);

    if (fwrite(wire_file->filedata, 1, file_data_size, file) != file_data_size) {
        xwarn("> Error writing to file \"%s\"\n", wire_file->filename);
        (void)fclose(file);
        xfree(wire_file->filename);
        return false;
    }

    if (fflush(file) || fclose(file)) {
        xwarn("> Error closing file \"%s\"\n", wire_file->filename);
        return false;
    }

    return true;
}

static void proc_text(void *data)
{
    const char *text = data;
    WRITE_CONST_STR(STDOUT_FILENO, "\033[2K\r");
    WRITE_STR(STDOUT_FILENO, text);
    WRITE_CONST_STR(STDOUT_FILENO, "\n");
}

static int proc_ctrl(client_t *ctx, void *data)
{
    ctrl_msg_t *wire_ctrl = data;
    memcpy(ctx->keys.ctrl, wire_ctrl->renewed_key, KEY_LEN);

    switch (wire_get_ctrl_type(wire_ctrl)) {
        case CTRL_ERROR:
            return CTRL_ERROR;
        case CTRL_EXIT:
            return CTRL_EXIT;
        case CTRL_DHKE:
            debug_print("%s", "> Received DHKE ctrl msg\n");
            if (n_party_client(ctx->socket, ctx->keys.session, wire_get_ctrl_args(wire_ctrl))) {
                if (!ctx->internal.conn_announced) {
                    if (!announce_connection(ctx)) {
                        return CTRL_ERROR;
                    }
                }
                return CTRL_DHKE;
            }
            return DHKE_ERROR;
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
msg_type_t proc_type(client_t *ctx, wire_t *wire)
{
    msg_type_t type = wire_get_msg_type(wire);
    debug_print("msg_type: %d\n", (int)type);
    switch (type) {
        case TYPE_CTRL: // Forward wire along to proc_ctrl()
            switch (proc_ctrl(ctx, wire->data)) {
                case CTRL_EXIT:
                    xfree(wire);
                    exit(EXIT_FAILURE); // TODO: figure something out for this
                case CTRL_DHKE:
                    xmemcpy_locked(&ctx->shctx->mutex_lock, &ctx->shctx->keys, &ctx->keys, sizeof(struct keys));
                    xmemcpy_locked(&ctx->shctx->mutex_lock, &ctx->shctx->internal, &ctx->internal, sizeof(struct client_internal));
                    break;
                case CTRL_ERROR:
                    xalert("proc_ctrl()\n");
                    return TYPE_ERROR;
            }
            break;
        case TYPE_FILE:
            if (!proc_file(wire->data)) {
                xalert("proc_file()\n");
                return TYPE_ERROR;
            }
            break;
        case TYPE_TEXT:
            proc_text(wire->data);
            disp_username(&ctx->username);
            break;
        default:
            return TYPE_ERROR;
    }
    return type;
}
