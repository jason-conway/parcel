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

static bool cmd_username(client_t *ctx)
{
    size_t new_username_length = USERNAME_MAX_LENGTH;
    char *new_username = xprompt("> New username: ", "username", &new_username_length);
    
    stat_msg_t stat = { 0 };
    wire_set_stat_msg_type(&stat, STAT_USER_RENAME);
    wire_set_stat_msg_data(&stat, new_username);

    pthread_mutex_lock(&ctx->lock);
    wire_set_stat_msg_user(&stat, ctx->username);
    pthread_mutex_unlock(&ctx->lock);

    size_t len = sizeof(stat_msg_t);
    wire_t *wire = init_wire(&stat, TYPE_STAT, &len);
    
    keys_t keys = { 0 };
    client_get_keys(&keys, ctx);
    sock_t socket = client_get_socket(ctx);

    encrypt_wire(wire, keys.session);

    if (!xsendall(socket, wire, len)) {
        xfree(wire);
        xfree(new_username);
        return false;
    }

    xfree(wire);
    pthread_mutex_lock(&ctx->lock);
    memset(&ctx->username, 0, USERNAME_MAX_LENGTH);
    memcpy(&ctx->username, new_username, new_username_length);
    pthread_mutex_unlock(&ctx->lock);
    xfree(new_username);
    return true;
}

size_t file_msg_from_file(file_msg_t **f)
{
    char *path = xprompt("> File path: ", "path", (size_t[]) { FILE_PATH_MAX_LENGTH });

    if (!xfexists(path)) {
        xwarn("> File \"%s\" not found\n", path);
        xfree(path);
        return 0;
    }

    size_t size = xfilesize(path);
    if (!size) {
        xwarn("> Unable to determine size of file \"%s\"\n", path);
        xfree(path);
        return 0;
    }

    if (size > FILE_DATA_MAX_SIZE) {
        const size_t overage = size - FILE_DATA_MAX_SIZE;
        xwarn("> File \"%s\" is %zu bytes over the maximum size of %d bytes\n", path, overage, FILE_DATA_MAX_SIZE);
        xfree(path);
        return 0;
    }

    FILE *file = fopen(path, "rb");
    if (!file) {
        xwarn("> Could not open file \"%s\" for reading\n", path);
        xfree(path);
        return 0;
    }

    size_t len = size + sizeof(file_msg_t);
    *f = xcalloc(len);

    wire_set_file_msg_size(*f, size);
    xbasename(path, (char *)((*f)->filename));
    xfree(path);

    if (fread((*f)->data, 1, size, file) != size) {
        xwarn("> Error reading contents of file\n");
        fclose(file);
        xfree(*f);
        *f = NULL;
        return 0;
    }
    fclose(file);
    return len;
}

// TODO: This should not be fatal
static bool cmd_send_file(client_t *ctx)
{
    file_msg_t *f = NULL;
    size_t len = file_msg_from_file(&f);
    if (!len) {
        return false;
    }

    wire_t *wire = init_wire(f, TYPE_FILE, &len);

    keys_t keys = { 0 };
    client_get_keys(&keys, ctx);
    sock_t socket = client_get_socket(ctx);

    encrypt_wire(wire, keys.session);
    if (!xsendall(socket, wire, len)) {
        xfree(f);
        xfree(wire);
        return false;
    }

    xfree(f);
    xfree(wire);

    return true;
}

static void cmd_print_enc_info(client_t *ctx)
{
    keys_t keys = { 0 };
    client_get_keys(&keys, ctx);
    
    printf("Session Key: ");
    fflush(stdout);
    xmemprint(keys.session, KEY_LEN);
    printf("Control Key: ");
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
    stat_msg_t stat = { 0 };
    wire_set_stat_msg_type(&stat, STAT_USER_DISCONNECT);
    
    pthread_mutex_lock(&ctx->lock);
    wire_set_stat_msg_user(&stat, ctx->username);
    pthread_mutex_unlock(&ctx->lock);

    size_t len = sizeof(stat_msg_t);
    wire_t *wire = init_wire(&stat, TYPE_STAT, &len);
    
    keys_t keys = { 0 };
    client_get_keys(&keys, ctx);
    sock_t socket = client_get_socket(ctx);

    encrypt_wire(wire, keys.session);

    if (!xsendall(socket, wire, len)) {
        xfree(wire);
        return false;
    }
    xfree(wire);
    shutdown(socket, SHUT_RDWR);
    atomic_store(&ctx->keep_alive, false);
    return true;
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
    switch (cmd) {
        default:
            cmd_not_found(message);
            return true;
        case CMD_AMBIGUOUS:
            cmd_ambiguous();
            return true;
        case CMD_LIST:
            cmd_list();
            return true;
        case CMD_CLEAR:
            cmd_clear();
            return true;
        case CMD_VERSION:
            cmd_version();
            return true;

        case CMD_ENC_INFO:
            cmd_print_enc_info(ctx);
            return true;
        case CMD_EXIT:
            return cmd_exit(ctx);
        case CMD_USERNAME:
            return cmd_username(ctx);
        case CMD_FILE:
            return cmd_send_file(ctx);
        case CMD_NONE:
            return true;
    }
}
