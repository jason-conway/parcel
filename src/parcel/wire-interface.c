#include "wire.h"
#include "wire-stat.h"
#include "wire-file.h"
#include "wire-text.h"
#include "wire-ctrl.h"
#include "client.h"
#include "xplatform.h"
#include "xutils.h"
#include "cable.h"
#include <stddef.h>

wire_t *client_init_stat_conn_wire(client_t *client, stat_msg_type_t type)
{
    assert(type == STAT_USER_CONNECT || type == STAT_USER_DISCONNECT);

    char username[USERNAME_MAX_LENGTH] = { 0 };
    client_get_username(client, username);

    stat_msg_t *stat_msg = init_stat_conn_msg(type, username);
    wire_t *wire = init_wire_from_stat_msg(stat_msg);
    xfree(stat_msg);
    return wire;
}

wire_t *client_init_stat_rename_wire(client_t *client, const char *new)
{
    char username[USERNAME_MAX_LENGTH] = { 0 };
    client_get_username(client, username);

    stat_msg_t *stat_msg = init_stat_rename_msg(username, new);
    wire_t *wire = init_wire_from_stat_msg(stat_msg);
    xfree(stat_msg);
    return wire;
}

text_msg_t *client_init_text_msg(client_t *client, const void *data, size_t len)
{
    char username[USERNAME_MAX_LENGTH] = { 0 };
    client_get_username(client, username);
    return text_msg_from_text(username, data, len);
}

wire_t *client_init_text_wire(client_t *client, const void *data, size_t len)
{
    text_msg_t *text_msg = client_init_text_msg(client, data, len);
    wire_t *wire = init_wire_from_text_msg(text_msg);
    xfree(text_msg);
    return wire;
}

wire_t *client_init_file_wire(client_t *client, const char *path)
{
    char username[USERNAME_MAX_LENGTH] = { 0 };
    client_get_username(client, username);

    file_msg_t *file_msg = file_msg_from_file(username, path);
    if (!file_msg) {
        return NULL;
    }
    wire_t *wire = init_wire_from_file_msg(file_msg);
    xfree(file_msg);
    return wire;
}


bool transmit_wire(client_t *client, wire_t *wire)
{
    keys_t keys = { 0 };
    client_get_keys(client, &keys);

    sock_t sock = client_get_socket(client);

    size_t len = wire_get_length(wire);
    encrypt_wire(wire, keys.session);
    cable_t *cable = init_cable(wire, &len);

    bool ok = xsendall(sock, cable, len);
    xfree(cable);
    return ok;
}
