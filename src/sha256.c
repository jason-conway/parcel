/**
 * @file sha256.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Exciting new implementation of SHA-256
 * @ref https://csrc.nist.gov/csrc/media/publications/fips/180/2/archive/2002-08-01/documents/fips180-2.pdf
 * @version 0.9.2
 * @date 2021-12-15
 *
 * @copyright Copyright (c) 2021-2022 Jason Conway. All rights reserved.
 *
 */

#include "sha256.h"

// New context
void sha256_init(sha256_t *ctx)
{
	memset(ctx, 0, sizeof(sha256_t));

	ctx->state[0] = 0x6a09e667; // sqrt(0x02)
	ctx->state[1] = 0xbb67ae85; // sqrt(0x03)
	ctx->state[2] = 0x3c6ef372; // sqrt(0x05)
	ctx->state[3] = 0xa54ff53a; // sqrt(0x07)
	ctx->state[4] = 0x510e527f; // sqrt(0x0b)
	ctx->state[5] = 0x9b05688c; // sqrt(0x0d)
	ctx->state[6] = 0x1f83d9ab; // sqrt(0x11)
	ctx->state[7] = 0x5be0cd19; // sqrt(0x13)
}

static void sha256_hash(sha256_t *ctx, const uint8_t *data)
{
	static const uint32_t round_consts[64] = {
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
	};

	// Prepare the message schedule 
	uint32_t msg_schedule[64]; // W_0, W_1, ..., W_63
	for (size_t i = 0; i < 64; ++i) {
		if (i < 16) {
			// W_t = M_t^(i)
			msg_schedule[i] = (((uint32_t)data[4 * i + 0] << 0x18) |
							   ((uint32_t)data[4 * i + 1] << 0x10) |
							   ((uint32_t)data[4 * i + 2] << 0x08) |
							   ((uint32_t)data[4 * i + 3] << 0x00));
			continue;
		}

		msg_schedule[i] = ((((msg_schedule[i - 0x02] >> 0x11) |
							 (msg_schedule[i - 0x02] << 0x0f)) ^
							((msg_schedule[i - 0x02] >> 0x13) |
							 (msg_schedule[i - 0x02] << 0x0d)) ^
							 (msg_schedule[i - 0x02] >> 0x0a)) +
						   (((msg_schedule[i - 0x0f] >> 0x07) |
							 (msg_schedule[i - 0x0f] << 0x19)) ^
							((msg_schedule[i - 0x0f] >> 0x12) |
							 (msg_schedule[i - 0x0f] << 0x0e)) ^
							 (msg_schedule[i - 0x0f] >> 0x03)) +
							  msg_schedule[i - 0x10] +
							  msg_schedule[i - 0x07]);
	}

	uint32_t state[10] = {
		ctx->state[0], // a
		ctx->state[1], // b
		ctx->state[2], // c
		ctx->state[3], // d
		ctx->state[4], // e
		ctx->state[5], // f
		ctx->state[6], // g
		ctx->state[7], // h
		0x00000000,    // T_1
		0x00000000     // T_2
	};

	// Compression function
	for (size_t i = 0; i < 64; ++i) {
		state[8] = (state[7] + round_consts[i] + msg_schedule[i] +
					(((state[4] >> 0x06) | (state[4] << 0x1a)) ^
					 ((state[4] >> 0x0b) | (state[4] << 0x15)) ^
					 ((state[4] >> 0x19) | (state[4] << 0x07))) +
					((state[4] & state[5]) ^ (~state[4] & state[6])));

		state[9] = ((((state[0] >> 0x02) | (state[0] << 0x1e)) ^
					 ((state[0] >> 0x0d) | (state[0] << 0x13)) ^
					 ((state[0] >> 0x16) | (state[0] << 0x0a))) +
					((state[0] & state[1]) ^ (state[0] & state[2]) ^ (state[1] & state[2])));

		state[7] = state[6];
		state[6] = state[5];
		state[5] = state[4];
		state[4] = state[3] + state[8];
		state[3] = state[2];
		state[2] = state[1];
		state[1] = state[0];
		state[0] = state[8] + state[9];
	}

	// Intermediate hash H^(i)
	ctx->state[0] += state[0];
	ctx->state[1] += state[1];
	ctx->state[2] += state[2];
	ctx->state[3] += state[3];
	ctx->state[4] += state[4];
	ctx->state[5] += state[5];
	ctx->state[6] += state[6];
	ctx->state[7] += state[7];
}

void sha256_append(sha256_t *ctx, const uint8_t *data, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		ctx->data[ctx->data_length] = data[i];
		ctx->data_length++;

		if (ctx->data_length == 64) {
			sha256_hash(ctx, ctx->data);
			ctx->bit_length += 512;
			ctx->data_length = 0;
		}
	}
}

void sha256_finish(sha256_t *ctx, uint8_t *hash)
{
	uint32_t length = ctx->data_length;

	if (ctx->data_length < 56) {
		ctx->data[length++] = 128;
		while (length < 56) {
			ctx->data[length++] = 0;
		}
	}
	else {
		ctx->data[length++] = 128;
		while (length < 64) {
			ctx->data[length++] = 0;
		}
		sha256_hash(ctx, ctx->data);
		memset(ctx->data, 0, 56);
	}

	// Append to the padding the total message's length in bits and transform.
	ctx->bit_length += 8 * ctx->data_length;

	// Message length stored as the last 8 octets
	for (size_t i = 0; i < 8; i++) {
		ctx->data[0x3f - i] = (uint8_t)(ctx->bit_length >> (uint8_t)(8 * i));
	}

	sha256_hash(ctx, ctx->data);

	// Reverse bytes
	for (size_t i = 0; i < 4; i++) {
		for (size_t j = 0; j < 8; j++) {
			hash[(4 * j) + i] = (ctx->state[j] >> (24 - 8 * i)) & 0xff;
		}
	}
}

void sha256_digest(const uint8_t *key, uint8_t *hash)
{
	sha256_t ctx;
	sha256_init(&ctx);
	sha256_append(&ctx, key, 32);
	sha256_finish(&ctx, hash);
}

void sha256_self_digest(uint8_t *digest)
{
	uint8_t hash[32];
	memcpy(hash, digest, 32);
	sha256_digest(hash, digest);
}
