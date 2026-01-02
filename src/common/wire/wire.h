/**
 * @file wire.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Wire is an encyption/decryption tool for Parcel built on AES in CBC mode
 * @version 0.9.2
 * @date 2022-02-06
 *
 * @copyright Copyright (c) 2022 - 2026 Jason Conway. All rights reserved.
 *
 */

#pragma once

#include "aes128.h"
#include "sha256.h"
#include "xplatform.h"
#include "xutils.h"
#include "log.h"
#include "wire-gen.h"
#include <stdint.h>

typedef enum wire_type_t {
    TYPE_ERROR = -1,
    TYPE_NONE,
    TYPE_TEXT,
    TYPE_FILE,
    TYPE_CTRL,
    TYPE_STAT,
    TYPE_SESSION_KEY
} wire_type_t;

typedef struct wire_auth_t {
    uint8_t mac_outer[16]; // message authentication code for an entire wire
    uint8_t mac_inner[16];  // message authentication code for the wire length
    uint8_t iv[16];         // initialization vector for AES context
} __packed wire_auth_t;

typedef struct header_t {
    uint8_t signature[6];
    uint8_t wire_len[8];   // length of entire wire (auth + header + data + padding)
    uint8_t alignment; // padding bytes added to data to align with block size
    uint8_t type;     // type of wire, see enum wire_type
} __packed header_t;

typedef struct wire_t {
    wire_auth_t auth;
    header_t header;
    uint8_t data[];
} __packed wire_t;


enum cfg {
    KEY_LEN = 2 * AES_KEY_LEN,
    BLOCK_LEN = AES_BLOCK_SIZE,
    DATA_LEN_MAX = 1ull << 16,
    RECV_MAX_BYTES = sizeof(wire_t) + DATA_LEN_MAX,
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
    WIRE_OFFSET_MAC_OUTER = offsetof(wire_t, auth.mac_outer),
    WIRE_OFFSET_MAC_INNER = offsetof(wire_t, auth.mac_inner),
    WIRE_OFFSET_IV        = offsetof(wire_t, auth.iv),
    WIRE_OFFSET_MAGIC     = offsetof(wire_t, header.signature),
    WIRE_OFFSET_LENGTH    = offsetof(wire_t, header.wire_len),
    WIRE_OFFSET_ALIGNMENT = offsetof(wire_t, header.alignment),
    WIRE_OFFSET_TYPE      = offsetof(wire_t, header.type),
    WIRE_OFFSET_DATA      = offsetof(wire_t, data),
};

typedef enum decrypt_status_t {
    WIRE_OK,
    WIRE_CMAC_ERROR,
    WIRE_INVALID_KEY,
    WIRE_PARTIAL,
} decrypt_status_t;



wire_t *alloc_wire(void);
wire_t *init_wire(wire_type_t type, const void *data, size_t *len);

bool encrypt_wire(wire_t *wire, const uint8_t *key);
bool decrypt_wire(wire_t *wire, size_t len, const uint8_t *key1, const uint8_t *key2);

wire_type_t wire_get_type(const wire_t *ctx);

size_t wire_get_data_length(wire_t *wire);
size_t wire_get_aligned_data_length(wire_t *wire);
size_t wire_get_length(wire_t *wire);
void wire_set_length(wire_t *wire, size_t len);

size_t get_aligned_len(size_t len);

void wire_set_header(wire_t *wire, header_t *hdr);


size_t wire_get_alignment(const wire_t *wire);
void wire_set_alignment(wire_t *wire, size_t alignment);
