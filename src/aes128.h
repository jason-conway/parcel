/**
 * @file aes128.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief A fresh implementation of AES and AES-CMAC following FIPS 197 and RFC4493
 * @version 0.9.3
 * @date 2022-02-06
 *
 * @copyright Copyright (c) 2022 - 2023 Jason Conway.
 *
 */

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

enum AES128 {
	AES_WORD_COUNT = 4,
	AES_ROUNDS = 10,
	AES_BLOCK_SIZE = 16,
	AES_KEY_LEN = 16,
	CMAC_KEY_LEN = 16,
	AES_KEY_BITS = 8 * AES_KEY_LEN,
};

typedef struct aes128_t {
	uint8_t round_key[AES_BLOCK_SIZE * (AES_ROUNDS + 1)];
	uint8_t iv[AES_BLOCK_SIZE];
} aes128_t;

/**
 * @brief Initiate a new aes128_t context for encryption / decryption
 *
 * @param[inout] ctx aes128 instance
 * @param[in] iv initialization vector for the context
 * @param[in] key 128-bit symmetric key
 */
void aes128_init(aes128_t *ctx, const uint8_t *iv, const uint8_t *key);

/**
 * @brief Initiate a new aes128_t context for CMAC
 *
 * @param[inout] ctx aes128 instance
 * @param[in] key 128-bit key
 */
void aes128_init_cmac(aes128_t *ctx, const uint8_t *key);

/**
 * @brief Encrypt contents in-place
 *
 * @param[inout] ctx aes128 instance
 * @param[inout] chunk pointer to plaintext/ciphertext
 * @param[in] length number of bytes to encrypt
 */
void aes128_encrypt(aes128_t *ctx, uint8_t *chunk, size_t length);

/**
 * @brief Decrypt contents in-place
 *
 * @param[inout] ctx aes128 instance
 * @param[inout] chunk pointer to ciphertext/plaintext
 * @param[in] length number of bytes to decrypt
 */
void aes128_decrypt(aes128_t *ctx, uint8_t *chunk, size_t length);

/**
 * @brief Cipher-based Message Authentication Code (OMAC1)
 * 
 * @param[inout] ctx CMAC-specific aes128 instance
 * @param[in] msg pointer to ciphertext/plaintext message
 * @param[in] length number of bytes to process
 * @param[out] mac 16-byte generated tag
 */
void aes128_cmac(const aes128_t *ctx, const uint8_t *msg, size_t length, uint8_t *mac);
