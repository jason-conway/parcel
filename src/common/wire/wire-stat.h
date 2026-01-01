#pragma once

#include "wire.h"
#include "wire-gen.h"

typedef enum stat_msg_type_t {
    STAT_USER_CONNECT,
    STAT_USER_DISCONNECT,
    STAT_USER_RENAME,
} stat_msg_type_t;

enum stat_msg_cfg {
    STAT_USERNAME_LENGTH = 64,
};

GEN_STD_HEADERS(stat_msg, stat_msg_type_t)

bool stat_msg_get_user(stat_msg_t *sm, char *user);
void stat_msg_set_user(stat_msg_t *sm, const char *user);
wire_t *init_wire_from_stat_msg(stat_msg_t *stat_msg);
stat_msg_t *init_stat_conn_msg(stat_msg_type_t type, const char *user);

stat_msg_t *init_stat_rename_msg(const char *curr_name, const char *new_name);
