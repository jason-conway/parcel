/**
 * @file commands.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief parcel client commands
 * @version 0.9.2
 * @date 2022-03-14
 *
 * @copyright Copyright (c) 2022 - 2026 Jason Conway. All rights reserved.
 *
 */

#include "client.h"
#include "wire-stat.h"
#include <stdatomic.h>

static bool cmd_username(client_t *ctx)
{
    size_t new_username_length = USERNAME_MAX_LENGTH;
    char *new_username = xprompt("> New username: ", "username", &new_username_length);
    
    wire_t *wire = client_init_stat_rename_wire(ctx, new_username);
    bool ok = transmit_wire(ctx, wire);
    if (!ok) {
        log_error("error sending wire via cable");
    }
    xfree(wire);

    pthread_mutex_lock(&ctx->lock);
        memset(&ctx->username, 0, USERNAME_MAX_LENGTH);
        memcpy(&ctx->username, new_username, new_username_length);
    pthread_mutex_unlock(&ctx->lock);

    xfree(new_username);
    return ok;
}

static bool cmd_send_file(client_t *ctx)
{
    char *path = xprompt("> File path: ", "path", (size_t[]) { FILE_PATH_MAX_LENGTH });

    if (!xfexists(path)) {
        xwarn("> File \"%s\" not found\n", path);
        xfree(path);
        return 0;
    }
    wire_t *wire = client_init_file_wire(ctx, path);
    bool ok = transmit_wire(ctx, wire);
    if (!ok) {
        log_error("error sending wire via cable");
    }
    xfree(wire);
    xfree(path);
    return ok;
}

static void cmd_print_enc_info(client_t *ctx)
{
    keys_t keys = { 0 };
    client_get_keys(ctx, &keys);
    
    fprintf(stdout, "Session Key: ");
    fflush(stdout);
    xmemprint(keys.session, KEY_LEN);
    fprintf(stdout, "Control Key: ");
    fflush(stdout);
    xmemprint(keys.ctrl, KEY_LEN);
}

static void cmd_clear(void)
{
    clear_screen();
}

static void cmd_not_found(char *message)
{
    xwarn("Unrecognized command, \"%s\"\n", message);
}

bool cmd_exit(client_t *ctx)
{
    wire_t *wire = client_init_stat_conn_wire(ctx, STAT_USER_DISCONNECT);
    bool ok = transmit_wire(ctx, wire);
    if (!ok) {
        log_error("error sending wire via cable");
    }
    xfree(wire);

    shutdown(ctx->socket, SHUT_RDWR);
    atomic_store(&ctx->keep_alive, false);
    return ok;
}

static void cmd_list(void)
{
    static const char list[] =
        "parcel commands:\n"
        "  /list         list available commands\n"
        "  /q            exit the server and close parcel\n"
        "  /username     change username\n"
        "  /encinfo      display current encryption parameters\n"
        "  /file         send a file\n"
        "  /clear        clear the screen\n"
        "  /version      print build version\n";
    WRITE_CONST_STR(STDOUT_FILENO, list);
}

static void cmd_version(void)
{
    xprintf(DEF, BOLD, "%s\n", "parcel " STR(PARCEL_VERSION));
}

static void cmd_ambiguous(void)
{
    xwarn("Ambiguous command entered\n");
    cmd_list();
}

static cmd_type_t parse_command(char *command)
{
    static const char *command_strings[] = {
        "/list", "/q", "/username", "/encinfo", "/file", "/clear", "/version"
    };

    const size_t commands = sizeof(command_strings) / sizeof(*command_strings);

    cmd_type_t cmd = CMD_NONE;

    const size_t len = strlen(command);
    for (size_t i = 1; i <= commands; i++) {
        if (!strncmp(command, command_strings[i - 1], len)) {
            if (cmd) {
                return CMD_AMBIGUOUS;
            }
            cmd = (cmd_type_t)i;
        }
    }
    return cmd;
}

bool exec_cmd(client_t *ctx, char *message)
{
    cmd_type_t cmd = parse_command(message);
    bool ok = true;
    switch (cmd) {
        default:
            cmd_not_found(message);
            break;
        case CMD_AMBIGUOUS:
            cmd_ambiguous();
            break;
        case CMD_LIST:
            cmd_list();
            break;
        case CMD_CLEAR:
            cmd_clear();
            break;
        case CMD_VERSION:
            cmd_version();
            break;
        case CMD_ENC_INFO:
            cmd_print_enc_info(ctx);
            break;
        case CMD_EXIT:
            ok = cmd_exit(ctx);
            break;
        case CMD_USERNAME:
            ok = cmd_username(ctx);
            break;
        case CMD_FILE:
            ok = cmd_send_file(ctx);
            break;
        case CMD_NONE:
            break;
    }
    reset_last_sender();
    return ok;
}
