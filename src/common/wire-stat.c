#include "wire-util.h"
#include "wire-gen.h"
#include "wire.h"
#include <stddef.h>
#include "wire-stat.h"

TYPE_BEGIN_DEF(stat_msg, stat_msg_type_t)
    uint8_t user[STAT_USERNAME_LENGTH];
TYPE_END_DEF(stat_msg, stat_msg_type_t)

GEN_STD_FUNCS(stat_msg, stat_msg_type_t)


void stat_msg_set_user(stat_msg_t *sm, const char *user)
{
    memset(sm->user, 0, sizeof(sm->user));
    const size_t len = strnlen(user, sizeof(sm->user) - 1);
    memcpy(sm->user, user, len);
}

bool stat_msg_get_user(stat_msg_t *sm, char *user)
{
    const size_t len = strnlen((const char *)sm->user, sizeof(sm->user));
    memcpy(user, sm->user, len);
    return len > 0;
}

stat_msg_t *init_stat_conn_msg(stat_msg_type_t type, const char *user)
{
    stat_msg_t *stat_msg = init_stat_msg(type, 0);
    stat_msg_set_user(stat_msg, user);
    return stat_msg;
}

stat_msg_t *init_stat_rename_msg(const char *curr_name, const char *new_name)
{
    const size_t new_name_len = strnlen(new_name, STAT_USERNAME_LENGTH);
    stat_msg_t *stat_msg = init_stat_msg(STAT_USER_RENAME, new_name_len);
    stat_msg_set_user(stat_msg, curr_name);
    stat_msg_set_data(stat_msg, new_name, new_name_len);
    return stat_msg;
}

wire_t *init_wire_from_stat_msg(stat_msg_t *stat_msg)
{
    size_t len = stat_msg_get_wire_length(stat_msg);
    return init_wire(TYPE_STAT, stat_msg, &len);
}
