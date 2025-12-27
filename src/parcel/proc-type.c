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
#include "log.h"
#include "wire-stat.h"
#include "wire-text.h"
#include "wire-file.h"
#include "wire-ctrl.h"
#include "wire.h"
#include "xplatform.h"
#include <stdio.h>


static bool proc_file(void *data)
{
    file_msg_t *fm = data;
    char filename[FILENAME_MAX] = { 0 };
    if (!file_msg_get_filename(fm, filename)) {
        log_error("file_msg_t has empty filename field");
        return false;
    }
    char sender[FILE_USERNAME_LENGTH] = { 0 };
    if (file_msg_get_user(fm, sender)) {
        log_error("file_msg_t has empty username field");
        return false;
    }
    size_t filesize = file_msg_get_payload_length(fm);
    printf("%s sent a file: %s (%zu kb)\n", sender, filename, filesize >> 10);
    if (!file_msg_to_file(fm, xgethome())) {
        log_error("error writing file to disk");
        return false;
    }
    return true;
}

static bool proc_stat(void *data)
{
    stat_msg_t *stat = data;
    stat_msg_type_t type = stat_msg_get_type(stat);

    char username[USERNAME_MAX_LENGTH] = { 0 };
    if (!stat_msg_get_user(stat, username)) {
        log_error("empty user field");
        return false;
    }
    const char *aux = stat_msg_get_data(stat);

    static const char *msgs[] = {
        [STAT_USER_CONNECT] = "\033[1m%s is online\033[0m\n",
        [STAT_USER_DISCONNECT] = "\033[1m%s is offline\033[0m\n",
        [STAT_USER_RENAME] = "\033[2K\r%s has changed their name to %s\033[0m\n"
    };

    switch (type) {
        case STAT_USER_CONNECT:
        case STAT_USER_DISCONNECT:
            fprintf(stdout, msgs[type], username);
            break;
        case STAT_USER_RENAME:
            fprintf(stdout, msgs[type], username, aux);
            break;
        default:
            break;
    }
    return true;
}

static bool proc_text(void *data)
{
    text_msg_t *text = data;
    text_msg_type_t type = text_msg_get_type(text);
    if (type != TEXT_MSG_NORMAL) {
        log_error("invalid type for text message");
        return false;
    }
    char username[USERNAME_MAX_LENGTH] = { 0 };
    if (!text_msg_get_user(text, username)) {
        log_error("empty user field");
        return false;
    }
    const char *aux = text_msg_get_data(text);
    fprintf(stdout, "\033[2K\r\033[90m[%s]\033[0m \033[0;33m➜\033[0m %s\n", username, aux);
    return true;
}

static bool proc_ctrl(client_t *ctx, void *data)
{
    ctrl_msg_t *ctrl = data;
    ctrl_msg_type_t type = ctrl_msg_get_type(ctrl);

    uint8_t session[32] = { 0 };
    if (type == CTRL_DHKE) {
        log_info("received DHKE ctrl msg");
        size_t rounds = ctrl_msg_get_cnt(ctrl);
        log_debug("rounds: %zu", rounds);
        sock_t s = client_get_socket(ctx);
        if (!n_party_client(s, session, rounds)) {
            log_fatal("n-party key exchange failure (%zu rounds)", rounds);
            return false;
        }
    }

    const void *renewed_key = ctrl_msg_get_data(ctrl);

    keys_t k = { 0 };
    memcpy(&k.ctrl, renewed_key, KEY_LEN);
    memcpy(&k.session, session, KEY_LEN);

    client_set_keys(ctx, &k);

    bool announced = atomic_load(&ctx->conn_announced);
    if (!announced) {
        if (!announce_connection(ctx)) {
            log_fatal("failed to create or send STAT message");
            return false;
        }
    }
    return true;
}

bool handle_wire(client_t *ctx, wire_t *wire)
{
    wire_type_t type = wire_get_type(wire);
    static const char *types[] = {
        [TYPE_NONE] = "TYPE_NONE",
        [TYPE_TEXT] = "TYPE_TEXT",
        [TYPE_FILE] = "TYPE_FILE",
        [TYPE_CTRL] = "TYPE_CTRL",
        [TYPE_STAT] = "TYPE_STAT",
        [TYPE_SESSION_KEY] = "TYPE_SESSION_KEY",
    };
    log_trace("handle_wire(%s)", types[type]);

    bool ok = true;
    switch (type) {
        case TYPE_CTRL:
            ok = proc_ctrl(ctx, wire->data);
            break;
        case TYPE_FILE:
            ok = proc_file(wire->data);
            break;
        case TYPE_TEXT:
            ok = proc_text(wire->data);
            break;
        case TYPE_STAT:
            ok = proc_stat(wire->data);
            break;
        default:
            ok = false;
            break;
    }
    redraw_prompt(ctx);
    return ok;
}
