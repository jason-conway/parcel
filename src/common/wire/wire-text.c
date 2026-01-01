#include "wire-util.h"
#include "wire-gen.h"
#include "wire.h"
#include <stddef.h>
#include "wire-text.h"


TYPE_BEGIN_DEF(text_msg, text_msg_type_t)
    uint8_t user[TEXT_USERNAME_LENGTH];
TYPE_END_DEF(text_msg, text_msg_type_t)

GEN_STD_FUNCS(text_msg, text_msg_type_t)

void text_msg_set_user(text_msg_t *tm, const char *user)
{
    memset(tm->user, 0, sizeof(tm->user));
    const size_t len = strnlen(user, sizeof(tm->user) - 1);
    memcpy(tm->user, user, len);
}

bool text_msg_get_user(text_msg_t *tm, char *user)
{
    const size_t len = strnlen((const char *)tm->user, sizeof(tm->user));
    memcpy(user, tm->user, len);
    return len > 0;
}

text_msg_t *text_msg_from_text(const char *user, const void *data, size_t len)
{
    text_msg_t *text_msg = init_text_msg(TEXT_MSG_NORMAL, len + 1);
    text_msg_set_data(text_msg, data, len);
    text_msg_set_user(text_msg, user);
    return text_msg;
}

wire_t *init_wire_from_text_msg(text_msg_t *text_msg)
{
    size_t len = text_msg_get_wire_length(text_msg);
    return init_wire(TYPE_TEXT, text_msg, &len);
}