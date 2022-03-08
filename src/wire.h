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

uint8_t CTRL[] = { 0x06, 0xb9, 0xa6, 0xea, 0xcd, 0x7a, 0x77, 0xb9, 0x36, 0x11, 0x23, 0xfd, 0x19, 0x77, 0x64, 0x55 };
uint8_t FILE[] = { 0xdc, 0xfb, 0xc4, 0xa5, 0xec, 0x21, 0xe2, 0xe4, 0x66, 0x58, 0x04, 0x4f, 0x93, 0x1c, 0xc3, 0xd9 };
uint8_t TEXT[] = { 0x79, 0x57, 0xbd, 0xf8, 0xea, 0x97, 0xbe, 0xa3, 0x30, 0xa7, 0xe0, 0x33, 0x61, 0x61, 0x7f, 0x09 };

typedef struct wire_t
{
	uint8_t mac[16];
	uint8_t iv[16];
	uint8_t length[16];
	uint8_t data[];
} wire_t;

wire_t *init_wire(char *plaintext, size_t *len);
void encrypt_wire(wire_t *wire, const uint8_t *key);
int decrypt_wire(wire_t *wire, const uint8_t *key);