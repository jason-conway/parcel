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
uint64_t wire_pack64(const uint8_t *src)
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
void wire_unpack64(uint8_t *dest, uint64_t src)
{
	dest[0] = (uint8_t)(src >> 0x00);
	dest[1] = (uint8_t)(src >> 0x08);
	dest[2] = (uint8_t)(src >> 0x10);
	dest[3] = (uint8_t)(src >> 0x18);
	dest[4] = (uint8_t)(src >> 0x20);
	dest[5] = (uint8_t)(src >> 0x28);
	dest[6] = (uint8_t)(src >> 0x30);
	dest[7] = (uint8_t)(src >> 0x38);
}

uint64_t wire_get_raw(uint8_t *src)
{
	return wire_pack64(src);
}

void wire_set_raw(uint8_t *dest, uint64_t src)
{
	wire_unpack64(dest, src);
}

wire_t *new_wire(void)
{
	wire_t *wire = xcalloc(RECV_MAX_BYTES);
	return wire ? wire : NULL;
}

wire_t *init_wire(uint8_t *data, uint64_t type, size_t *len)
{
	const uint64_t data_length = BLOCK_LEN * ((*len + 15) / BLOCK_LEN);
	const size_t wire_length = sizeof(wire_t) + data_length;
	wire_t *wire = xcalloc(wire_length);
	if (!wire) {
		return NULL;
	}

	xgetrandom(wire->iv, BLOCK_LEN);
	wire_unpack64(wire->length, data_length);
	wire_unpack64(wire->type, type);
	memcpy(wire->data, data, *len);
	*len = wire_length;
	return wire;
}

size_t encrypt_wire(wire_t *wire, const uint8_t *key)
{
	aes128_t ctxs[2]; // ctxs[0] for encryption, ctxs[1] for CMAC
	aes128_init(&ctxs[0], wire->iv, &key[CIPHER_OFFSET]);
	aes128_init_cmac(&ctxs[1], &key[CMAC_OFFSET]);

	// Grab length from wire
	const size_t data_length = wire_pack64(wire->length);

	// Encrypt chunks
	aes128_encrypt(&ctxs[0], wire->length, data_length + BASE_ENC_LEN);

	// MAC for length only (LAC)
	aes128_cmac(&ctxs[1], wire->length, BLOCK_LEN, wire->lac);

	// MAC for LAC, IV, length, type, and chunks into the wire
	aes128_cmac(&ctxs[1], wire->lac, data_length + BASE_AUTH_LEN, wire->mac);
	return data_length;
}

int _decrypt_wire(wire_t *wire, size_t *len, const uint8_t *key)
{
	aes128_t ctxs[2];
	aes128_init(&ctxs[0], wire->iv, &key[CIPHER_OFFSET]);
	aes128_init_cmac(&ctxs[1], &key[CMAC_OFFSET]);

	// Decrypt only the length
	uint8_t verification_cmac[16];
	aes128_cmac(&ctxs[1], wire->length, BLOCK_LEN, verification_cmac);
	if (memcmp(&wire->lac[0], verification_cmac, BLOCK_LEN)) {
		return WIRE_INVALID_KEY;
	}

	uint8_t length[16];
	memcpy(length, wire->length, BLOCK_LEN);

	aes128_decrypt(&ctxs[0], length, BLOCK_LEN);
	const size_t data_length = wire_pack64(length);
	size_t wire_length = data_length + sizeof(wire_t);
	if (*len && *len != wire_length) {
		size_t received = *len;
		*len = wire_length - received; // Update with bytes remaining
		return WIRE_PARTIAL;
	}
	
	*len = data_length;

	// Verify MAC prior to decrypting in full
	aes128_cmac(&ctxs[1], wire->lac, data_length + BASE_AUTH_LEN, verification_cmac);
	if (memcmp(&wire->mac[0], verification_cmac, BLOCK_LEN)) {
		fprintf(stderr, "> internal: CMAC does not match\n");
		return WIRE_CMAC_ERROR;
	}

	aes128_decrypt(&ctxs[0], wire->type, data_length + BASE_DEC_LEN);
	return WIRE_OK;
}

int decrypt_wire(wire_t *wire, const uint8_t *key)
{
	size_t length = 0;
	return _decrypt_wire(wire, &length, key);
}
