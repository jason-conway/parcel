/**
 * @file sha256.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Exciting new implementation of SHA-256
 * @version 0.1
 * @date 2021-12-15
 *
 * @copyright Copyright (c) 2021-2022 Jason Conway. All rights reserved.
 *
 */

#pragma once

#include <stdint.h>
#include <string.h>

typedef struct sha256_t
{
	uint8_t data[64];
	uint32_t data_length;
	uint64_t bit_length;
	uint32_t state[8];
} sha256_t;

/**
 * @brief Initialize a new context or reset an old context
 * 
 * @param ctx sha256_t instance
 */
void sha256_init(sha256_t *ctx);

/**
 * @brief Append data to hash
 * 
 * @param ctx sha256_t context
 * @param data data to be processed into this hash context
 * @param len number of bytes to process
 */
void sha256_append(sha256_t *ctx, const uint8_t *data, size_t len);

/**
 * @brief Finish computation and output hash
 * 
 * @param ctx sha256_t context
 * @param[out] hash destination
 */
void sha256_finish(sha256_t *ctx, uint8_t *hash);

/**
 * @brief One-shot hash of 32-byte key
 * 
 * @param[in] key source
 * @param[out] hash destination
 */
void sha256_digest(const uint8_t *key, uint8_t *hash);

/**
 * @brief In-place variant of sha256_digest()
 * 
 * @param[inout] digest 32-byte key to hash, contents are overwritten
 */
void sha256_self_digest(uint8_t *digest);