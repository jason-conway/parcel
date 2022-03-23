/**
 * @file wire.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Wire is an encyption/decryption tool for Parcel built on AES in CBC mode
 * @version 0.1
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
	uint8_t iv[16];
	uint8_t length[16];
	uint8_t data_type[16];
	uint8_t data[];
} wire_t;

enum Wire
{
	KEY_LEN = 1 << 5,
	BLOCK_LEN = 1 << 4,
	DATA_LEN_MAX = 1 << 20,
	RECV_MAX_BYTES = sizeof(wire_t) + DATA_LEN_MAX,
};

enum SectionLengths
{
	BASE_AUTH_LEN = sizeof(wire_t) - BLOCK_LEN,
	BASE_ENC_LEN = sizeof(((wire_t *)0)->length) + sizeof(((wire_t *)0)->data_type),
	BASE_DEC_LEN = sizeof(((wire_t *)0)->data_type)
};

enum WireType
{
	TYPE_TEXT = 0x74657874,
	TYPE_FILE = 0x66696c65,
	TYPE_CTRL = 0x6374726c,
};

enum DecryptionStatus
{
	WIRE_OK,
	CMAC_ERROR,
	INCORRECT_KEY,
	GDH
};

wire_t *new_wire(void);
wire_t *init_wire(uint8_t *data, uint64_t type, size_t *len);
size_t encrypt_wire(wire_t *wire, const uint8_t *key);
int decrypt_wire(wire_t *wire, const uint8_t *key);
int _decrypt_wire(wire_t *wire, size_t *len, const uint8_t *key);

void wire_set_raw(uint8_t *dest, uint64_t src);
uint64_t wire_get_raw(uint8_t *src);
