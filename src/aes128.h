/**
 * @file aes128.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Advanced Encryption Standard (AES) implementation with 128-bit key
 * @version 0.1
 * @date 2022-02-06
 *
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 *
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>

enum AES128 
{ 
	ROUNDS = 10,
	BLOCK_SIZE = 16
};

typedef struct aes128_t
{
	uint8_t round_key[176];
	uint8_t iv[16];
} aes128_t;

/**
 * @brief Initiate a new aes128_t context for encryption / decryption
 *
 * @param ctx aes128 instance
 * @param iv initialization vector for the context
 * @param key 128-bit symmetric key
 */
void aes128_init(aes128_t *ctx, const uint8_t *iv, const uint8_t *key);

/**
 * @brief Initiate a new aes128_t context for CMAC
 *
 * @param ctx aes128 instance
 * @param key 128-bit key
 */
void aes128_init_cmac(aes128_t *ctx, const uint8_t *key);

/**
 * @brief Encrypt contents in-place
 *
 * @param ctx aes128 instance
 * @param chunk pointer to plaintext/ciphertext
 * @param length number of bytes to encrypt
 */
void aes128_encrypt(aes128_t *ctx, uint8_t *chunk, size_t length);

/**
 * @brief Decrypt contents in-place
 *
 * @param ctx aes128 instance
 * @param chunk pointer to ciphertext/plaintext
 * @param length number of bytes to decrypt
 */
void aes128_decrypt(aes128_t *ctx, uint8_t *chunk, size_t length);

/**
 * @brief Cipher-based Message Authentication Code (OMAC1)
 * 
 * @param ctx CMAC-specific aes128 instance
 * @param[in] msg pointer to ciphertext/plaintext message
 * @param[in] length number of bytes to process
 * @param[out] mac 16-byte generated tag
 */
void aes128_cmac(const aes128_t *ctx, const uint8_t *msg, size_t length, uint8_t *mac);