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

enum Wire
{
	KEY_LEN = 32,
	BLOCK_LEN = 16,
	MAX_PASSPHRASE_LENGTH = 64,
};

typedef struct wire_t
{
	uint8_t mac[16];
	uint8_t iv[16];
	uint8_t length[16];
	uint8_t type[16];
	uint8_t data[];
} wire_t;

enum WireType
{
	TYPE_TEXT = 't',
	TYPE_FILE = 'f',
};

wire_t *init_wire(void *data, uint8_t type, size_t *len);
void encrypt_wire(wire_t *wire, const uint8_t *key);
int decrypt_wire(wire_t *wire, const uint8_t *key);