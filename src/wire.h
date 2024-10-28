/**
 * @file wire.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Wire is an encyption/decryption tool for Parcel built on AES in CBC mode
 * @version 0.9.2
 * @date 2022-02-06
 *
 * @copyright Copyright (c) 2022 - 2024 Jason Conway. All rights reserved.
 *
 */

#pragma once

#include "aes128.h"
#include "sha256.h"
#include "xplatform.h"
#include "xutils.h"

typedef enum msg_type_t {
    TYPE_ERROR = -1,
    TYPE_NONE,
    TYPE_TEXT,
    TYPE_FILE,
    TYPE_CTRL,
    TYPE_STAT,
} msg_type_t;

typedef struct header_t {
    uint8_t len[8]; // length of entire wire
    uint8_t type[8];   // type of wire, see enum wire_type
} __attribute__((packed)) header_t;


typedef struct wire_t {
    uint8_t mac[16];    // message authentication code for entire wire
    uint8_t lac[16];    // message authentication code for wire length
    uint8_t iv[16];     // initialization vector for AES context
    header_t header;
    uint8_t data[];     // wire data
} __attribute__((packed)) wire_t;

enum cfg {
    KEY_LEN = 2 * AES_KEY_LEN,
    BLOCK_LEN = AES_BLOCK_SIZE,
    DATA_LEN_MAX = 1ull << 16,
    RECV_MAX_BYTES = sizeof(wire_t) + DATA_LEN_MAX,
};

enum TypeFile {
    FILE_PATH_MAX_LENGTH = FILENAME_MAX,
    FILE_NAME_START = 0,
    FILE_NAME_LEN = (1 << 8) - 1, // Win32 API limit
    FILE_SIZE_START = FILE_NAME_LEN,
    FILE_DATA_START = FILE_NAME_LEN + BLOCK_LEN,
    FILE_HEADER_SIZE = FILE_DATA_START,
    FILE_DATA_MAX_SIZE = (1ull<<31) - FILE_HEADER_SIZE,
};

enum TypeCtrl {
    CTRL_KEY_OFFSET = 16,
    CTRL_DATA_LEN = CTRL_KEY_OFFSET + KEY_LEN
};

enum SectionLengths {
    BASE_AUTH_LEN = sizeof(wire_t) - sizeof(header_t),
};

enum KeyOffsets {
    CIPHER_OFFSET = 0,
    CMAC_OFFSET = 16,
};

enum SectionOffsets {
    WIRE_OFFSET_MAC = offsetof(wire_t, mac),
    WIRE_OFFSET_LAC = offsetof(wire_t, lac),
    WIRE_OFFSET_IV = offsetof(wire_t, iv),
    WIRE_OFFSET_LENGTH = offsetof(wire_t, header.len),
    WIRE_OFFSET_TYPE = offsetof(wire_t, header.type),
    WIRE_OFFSET_DATA = offsetof(wire_t, data),
};
typedef enum ctrl_msg_type_t {
    CTRL_ERROR = -1,
    CTRL_EXIT,
    CTRL_DHKE,
} ctrl_msg_type_t;

enum DecryptionStatus {
    WIRE_OK,
    WIRE_CMAC_ERROR,
    WIRE_INVALID_KEY,
    WIRE_PARTIAL,
};

typedef enum stat_msg_type_t {
    STAT_USER_CONNECT,
    STAT_USER_DISCONNECT,
    STAT_USER_RENAME,
} stat_msg_type_t;



typedef struct ctrl_msg_t {
    uint8_t type[16];
    uint8_t args[16];
    uint8_t renewed_key[32];
} ctrl_msg_t;

typedef struct stat_msg_t {
    uint8_t user[32];
    uint8_t type[16];
    uint8_t data[32];
} stat_msg_t;


typedef struct text_msg_t {
    uint8_t user[32];
    uint8_t data[];
} text_msg_t;

typedef struct file_msg_t {
    uint8_t filename[64];
    uint8_t size[16];
    uint8_t data[];
} file_msg_t;

wire_t *new_wire(void);
wire_t *init_wire(void *data, uint64_t type, size_t *len);

size_t encrypt_wire(wire_t *wire, const uint8_t *key);
int decrypt_wire(wire_t *wire, size_t *len, const uint8_t *key);

msg_type_t wire_get_msg_type(wire_t *ctx);

ctrl_msg_type_t wire_get_ctrl_msg_type(ctrl_msg_t *ctrl);
void wire_set_ctrl_msg_type(ctrl_msg_t *ctrl, ctrl_msg_type_t type);

uint64_t wire_get_ctrl_msg_args(ctrl_msg_t *ctrl);
void wire_set_ctrl_msg_args(ctrl_msg_t *ctrl, uint64_t args);

void wire_set_ctrl_msg_key(ctrl_msg_t *ctrl, const uint8_t *renewed_key);

size_t wire_get_length(wire_t *wire);
void wire_set_length(wire_t *wire, size_t len);

void wire_set_file_msg_size(file_msg_t *f, size_t size);
size_t wire_get_file_msg_size(file_msg_t *f);
void wire_set_file_msg_data(file_msg_t *f, const void *data, size_t len);
void wire_set_file_msg_filename(file_msg_t *f, const char *filename, size_t len);


void wire_set_stat_msg_data(stat_msg_t *stat, const void *data);
void wire_set_stat_msg_type(stat_msg_t *stat, stat_msg_type_t type);
void wire_set_stat_msg_user(stat_msg_t *stat, const char *user);
stat_msg_type_t wire_get_stat_msg_type(stat_msg_t *stat);

void wire_set_text_msg_user(text_msg_t *text, const char *user);
void wire_set_text_msg_data(text_msg_t *text, const void *data, size_t len);
size_t get_aligned_len(size_t len);