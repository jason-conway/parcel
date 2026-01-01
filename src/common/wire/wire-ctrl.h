#pragma once

#include "wire.h"
#include "wire-gen.h"
#include "cable.h"

typedef enum ctrl_msg_type_t {
    CTRL_ERROR = -1,
    CTRL_EXIT,
    CTRL_DHKE,
} ctrl_msg_type_t;

GEN_STD_HEADERS(ctrl_msg, ctrl_msg_type_t)

GEN_GETTER_SETTER_HEADERS(ctrl_msg, cnt, size_t)

void ctrl_msg_set_cnt(ctrl_msg_t *ctrl, size_t cnt);

ctrl_msg_t *init_ctrl(size_t count, const uint8_t *renewed_key);
wire_t *init_wire_from_ctrl_msg(ctrl_msg_t *ctrl_msg);
cable_t *init_ctrl_key_cable(size_t count, const uint8_t *renewed_key, const uint8_t *ctrl_key);
