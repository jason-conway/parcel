/**
 * @file wire.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Wire is an encyption/decryption tool for parcel built on AES in CBC mode
 * @version 0.1
 * @date 2022-02-06
 *
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 *
 */

#include "wire.h"

// Pack byte array into 64-bit word
static uint64_t pack64_le(const uint8_t *src)
{
	return (((uint64_t)src[0] << 0x00) |
			((uint64_t)src[1] << 0x08) |
			((uint64_t)src[2] << 0x10) |
			((uint64_t)src[3] << 0x18) |
			((uint64_t)src[4] << 0x20) |
			((uint64_t)src[5] << 0x28) |
			((uint64_t)src[6] << 0x30) |
			((uint64_t)src[7] << 0x38));
}

// Unpack a 64-bit word into a little-endian array of bytes
static void unpack64_le(uint8_t *dest, uint64_t word)
{
	dest[0] = (uint8_t)(word >> 0x00);
	dest[1] = (uint8_t)(word >> 0x08);
	dest[2] = (uint8_t)(word >> 0x10);
	dest[3] = (uint8_t)(word >> 0x18);
	dest[4] = (uint8_t)(word >> 0x20);
	dest[5] = (uint8_t)(word >> 0x28);
	dest[6] = (uint8_t)(word >> 0x30);
	dest[7] = (uint8_t)(word >> 0x38);
}

wire_t *init_wire(void *data, uint8_t type, size_t *len)
{
	const uint64_t data_length = BLOCK_LEN * ((*len + 15) / BLOCK_LEN);
	const size_t wire_length = sizeof(wire_t) + data_length;
	wire_t *wire = malloc(wire_length);
	if (!wire) {
		return NULL;
	}
	memset(wire, 0, wire_length);

	xgetrandom(wire->iv, BLOCK_LEN);
	unpack64_le(wire->length, data_length);
	unpack64_le(wire->type, type);
	memcpy(wire->data, data, *len);
	*len = wire_length;
	return wire;
}

static inline size_t get_length(aes128_t *ctx, uint8_t *length_section, bool decrypt)
{
	if (decrypt) {
		uint8_t length[16] = { 0 };
		memcpy(length, length_section, BLOCK_LEN);
		aes128_decrypt(ctx, length, BLOCK_LEN);
		return pack64_le(length);
	}

	const size_t data_length = pack64_le(length_section);
	aes128_encrypt(ctx, length_section, BLOCK_LEN);
	return data_length;
}

void encrypt_wire(wire_t *wire, const uint8_t *key)
{
	aes128_t ctxs[2]; // ctxs[0] for encryption, ctxs[1] for CMAC
	aes128_init(&ctxs[0], wire->iv, &key[0]);
	aes128_init_cmac(&ctxs[1], &key[16]);

	// Unpack and encrypt length
	// const size_t data_length = get_length(&ctxs[0], wire->length, false);
	const size_t data_length = pack64_le(wire->length);
	
	// Encrypt chunks
	aes128_encrypt(&ctxs[0], wire->length, data_length + 32);

	// MAC for IV, length, type, and chunks into the wire
	aes128_cmac(&ctxs[1], wire->iv, data_length + (sizeof(wire_t) - 16), wire->mac);
}

int decrypt_wire(wire_t *wire, const uint8_t *key)
{
	aes128_t ctxs[2];
	aes128_init(&ctxs[0], wire->iv, &key[0]);
	aes128_init_cmac(&ctxs[1], &key[16]);

	// Decrypt and unpack data length
	const size_t data_length = get_length(&ctxs[0], wire->length, true);

	// Verify MAC prior to decrypting in full
	uint8_t verification_cmac[16] = { 0 };
	aes128_cmac(&ctxs[1], wire->iv, data_length + (sizeof(wire_t) - 16), verification_cmac);
	if (memcmp(&wire->mac[0], verification_cmac, BLOCK_LEN)) {
		fprintf(stderr, "> internal: CMAC does not match\n");
		return -1;
	}

	aes128_decrypt(&ctxs[0], wire->type, data_length + 16);
	return 0;
}
