#pragma once

#include "wire.h"
#include "wire-gen.h"

typedef enum file_msg_type_t {
    FILE_MSG_TYPE_UNKNOWN,
    FILE_MSG_TYPE_BINARY,
    FILE_MSG_TYPE_TEXT,
} file_msg_type_t;

enum file_cfg {
    FILE_USERNAME_LENGTH = 64,
    FILE_PATH_MAX_LENGTH = FILENAME_MAX,
    FILE_NAME_START = 0,
    FILE_NAME_LEN = (1 << 8) - 1, // Win32 API limit
    FILE_SIZE_START = FILE_NAME_LEN,
    FILE_DATA_START = FILE_NAME_LEN + BLOCK_LEN,
    FILE_HEADER_SIZE = FILE_DATA_START,
    FILE_DATA_MAX_SIZE = (1ull<<31) - FILE_HEADER_SIZE,
};

GEN_STD_HEADERS(file_msg, file_msg_type_t)
GEN_GETTER_SETTER_HEADERS(file_msg, gid, gid_t)
GEN_GETTER_SETTER_HEADERS(file_msg, uid, uid_t)
GEN_GETTER_SETTER_HEADERS(file_msg, mode, mode_t)



void file_msg_set_user(file_msg_t *fm, const char *user);
bool file_msg_get_user(file_msg_t *fm, char *user);
void file_msg_set_filename(file_msg_t *f, const char *name);
bool file_msg_get_filename(file_msg_t *f, char *out);
file_msg_t *file_msg_from_file(const char *user, const char *path);
bool file_msg_to_file(file_msg_t *fm, const char *directory);
wire_t *init_wire_from_file_msg(file_msg_t *file_msg);

bool handle_file_msg(wire_t *wire);




