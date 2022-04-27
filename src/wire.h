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
#include "xutils.h"

typedef struct wire_t
{
	uint8_t mac[16];    // message authentication code for entire wire
	uint8_t lac[16];    // message authentication code for wire length
	uint8_t iv[16];     // initialization vector for AES context
	uint8_t length[16]; // length of entire wire
	uint8_t type[16];   // type of wire, see enum wire_type
	uint8_t data[];     // wire data
} wire_t;

enum Wire
{
	KEY_LEN = 2 * AES_KEY_LEN,
	BLOCK_LEN = AES_BLOCK_SIZE,
	DATA_LEN_MAX = 1 << 20,
	RECV_MAX_BYTES = sizeof(wire_t) + DATA_LEN_MAX,
};

enum TypeFile
{
	FILE_PATH_MAX_LENGTH = FILENAME_MAX,
	FILE_NAME_START = 0,
	FILE_NAME_LEN = 1 << 6,
	FILE_SIZE_START = FILE_NAME_LEN,
	FILE_DATA_START = FILE_NAME_LEN + BLOCK_LEN,
	FILE_HEADER_SIZE = FILE_DATA_START,
	FILE_DATA_MAX_SIZE = DATA_LEN_MAX - FILE_HEADER_SIZE,
};

enum TypeCtrl
{
	CTRL_KEY_OFFSET = 16,
	CTRL_DATA_LEN = CTRL_KEY_OFFSET + KEY_LEN
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
enum wire_type
{
	TYPE_TEXT = 0x74657874,
	TYPE_FILE = 0x66696c65,
	TYPE_CTRL = 0x6374726c,
};

enum ctrl_function
{
	CTRL_EXIT = 0x65786974, // "exit"
	CTRL_DHKE = 0x64686b65 // "dhke"
};

enum DecryptionStatus
{
	WIRE_OK,
	WIRE_CMAC_ERROR,
	WIRE_INVALID_KEY,
	WIRE_PARTIAL,
};

struct wire_ctrl_message
{
	uint8_t function[16];
	uint8_t args[16];
	uint8_t renewed_key[32];
};

struct wire_file_message
{
	char filename[64];
	uint8_t filesize[16];
	uint8_t filedata[];
};

wire_t *new_wire(void);
wire_t *init_wire(void *data, uint64_t type, size_t *len);

size_t encrypt_wire(wire_t *wire, const uint8_t *key);
int decrypt_wire(wire_t *wire, size_t *len, const uint8_t *key);

uint64_t wire_pack64(const uint8_t *src);
uint64_t wire_get_raw(uint8_t *src);
enum wire_type wire_get_type(wire_t *ctx);


enum ctrl_function wire_get_ctrl_function(struct wire_ctrl_message *ctrl);
void wire_set_ctrl_function(struct wire_ctrl_message *ctrl, enum ctrl_function function);

uint64_t wire_get_ctrl_args(struct wire_ctrl_message *ctrl);
void wire_set_ctrl_args(struct wire_ctrl_message *ctrl, uint64_t args);

void wire_set_ctrl_renewal(struct wire_ctrl_message *ctrl, const uint8_t *renewed_key);

void wire_unpack64(uint8_t *dst, uint64_t src);
void wire_set_raw(uint8_t *dst, uint64_t src);
