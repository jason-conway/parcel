#pragma once

#include "wire.h"
#include "wire-util.h"
#include "wire-gen.h"

typedef enum text_msg_type_t {
    TEXT_MSG_NORMAL,
} text_msg_type_t;

enum text_msg_cfg {
    TEXT_USERNAME_LENGTH = 64,
};

GEN_STD_HEADERS(text_msg, text_msg_type_t)

bool text_msg_get_user(text_msg_t *tm, char *user);
void text_msg_set_user(text_msg_t *tm, const char *user);
text_msg_t *text_msg_from_text(const char *user, const void *data, size_t len);
wire_t *init_wire_from_text_msg(text_msg_t *text_msg);

