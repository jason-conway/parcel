#pragma once

#include "wire.h"
#include "wire-util.h"

typedef struct session_key_t {
    uint8_t key[KEY_LEN];
} session_key_t;

wire_t *init_wire_from_session_key(session_key_t *session_key);

