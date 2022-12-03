/**
 * @file sha256.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Exciting new implementation of SHA-256
 * @ref https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf
 * @version 2022.11
 * @date 2021-12-15
 *
 * @copyright Copyright (c) 2021-2022 Jason Conway. All rights reserved.
 *
 */

#pragma once

#include <stdint.h>
#include <string.h>

typedef struct sha256_t {
	uint8_t data[64];
	uint64_t block_bytes;
	uint64_t bits_total;
	uint32_t state[8];
} sha256_t;

/**
 * @brief Initialize a new context or reset an old context
 * 
 * @param[inout] ctx sha256_t instance
 */
void sha256_init(sha256_t *ctx);

/**
 * @brief Append data to hash
 * 
 * @param[inout] ctx sha256_t context
 * @param[in] src data to be processed into this hash context
 * @param[in] len number of bytes to process
 */
void sha256_append(sha256_t *ctx, const void *src, size_t len);

/**
 * @brief Finish computation and output hash
 * 
 * @param[inout] ctx sha256_t context
 * @param[out] dst destination
 */
void sha256_finish(sha256_t *ctx, void *dst);
