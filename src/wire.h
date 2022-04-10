/**
 * @file wire.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Wire is an encyption/decryption tool for Parcel built on AES in CBC mode
 * @version 0.9.1
 * @date 2022-02-06
 *
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 *
 */

#pragma once

#include "aes128.h"
#include "sha256.h"
#include "xplatform.h"

typedef struct wire_t
{
	uint8_t mac[16];
	uint8_t lac[16];
	uint8_t iv[16];
	uint8_t length[16];
	uint8_t type[16];
	uint8_t data[];
} wire_t;

enum Wire
{
	KEY_LEN = 1 << 5,
	BLOCK_LEN = 1 << 4,
	DATA_LEN_MAX = 1 << 20,
	RECV_MAX_BYTES = sizeof(wire_t) + DATA_LEN_MAX,
};

enum DataTypeFile
{
	FILE_PATH_MAX_LENGTH = FILENAME_MAX,
	FILE_NAME_START = 0,
	FILE_NAME_LEN = 1 << 6,
	FILE_SIZE_START = FILE_NAME_LEN,
	FILE_DATA_START = FILE_NAME_LEN + BLOCK_LEN,
	FILE_HEADER_SIZE = FILE_DATA_START,
	FILE_DATA_MAX_SIZE = DATA_LEN_MAX - FILE_HEADER_SIZE,
};

enum SectionLengths
{
	BASE_AUTH_LEN = sizeof(wire_t) - BLOCK_LEN,
	BASE_ENC_LEN = sizeof(((wire_t *)0)->length) + sizeof(((wire_t *)0)->type),
	BASE_DEC_LEN = sizeof(((wire_t *)0)->type)
};

enum KeyOffsets
{
	CIPHER_OFFSET = 0,
	CMAC_OFFSET = 16,
};

enum SectionOffsets
{
	WIRE_OFFSET_MAC = offsetof(wire_t, mac),
	WIRE_OFFSET_LAC = offsetof(wire_t, lac),
	WIRE_OFFSET_IV = offsetof(wire_t, iv),
	WIRE_OFFSET_LENGTH = offsetof(wire_t, length),
	WIRE_OFFSET_TYPE = offsetof(wire_t, type),
	WIRE_OFFSET_DATA = offsetof(wire_t, data),
};

/**
 * @brief WireType constants are the concatenated ascii values
 * of "text", "file", "ctrl" in hex.
 */
enum WireType
{
	TYPE_TEXT = 0x74657874,
	TYPE_FILE = 0x66696c65,
	TYPE_CTRL = 0x6374726c,
};

enum DecryptionStatus
{
	WIRE_OK,
	WIRE_CMAC_ERROR,
	WIRE_INVALID_KEY,
	WIRE_PARTIAL,
};

wire_t *new_wire(void);
wire_t *init_wire(uint8_t *data, uint64_t type, size_t *len);
size_t encrypt_wire(wire_t *wire, const uint8_t *key);
int decrypt_wire(wire_t *wire, const uint8_t *key);
int _decrypt_wire(wire_t *wire, size_t *len, const uint8_t *key);

uint64_t wire_pack64(const uint8_t *src);
uint64_t wire_get_raw(uint8_t *src);

void wire_unpack64(uint8_t *dest, uint64_t src);
void wire_set_raw(uint8_t *dest, uint64_t src);