/**
 * @file sha256.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Exciting new implementation of SHA-256
 * @ref https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.180-4.pdf
 * @version 2022.11
 * @date 2021-12-15
 *
 * @copyright Copyright (c) 2021-2024 Jason Conway. All rights reserved.
 *
 */

#include "sha256.h"

// SHR(x, n) (3.2.3)
static inline uint32_t shr(uint32_t x, uint32_t n)
{
    return x >> n;
}

// ROTR(x, n) (3.2.4)
static inline uint32_t rotr(uint32_t x, uint32_t n)
{
    return (x >> n) | (x << (0x20 - n));
}

// Ch(x,y,z) (4.2)
static inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z)
{
    return (x & y) ^ (~x & z);
}

// Maj(x,y,z) (4.3)
static inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z)
{
    return (x & y) ^ (x & z) ^ (y & z);
}

// Calling these `E` didn't feel right
// Σ0(x) (4.4)
static inline uint32_t sum0(uint32_t x)
{
    return rotr(x, 0x02) ^ rotr(x, 0x0d) ^ rotr(x, 0x16);
}

// Σ1(x) (4.5)
static inline uint32_t sum1(uint32_t x)
{
    return rotr(x, 0x06) ^ rotr(x, 0x0b) ^ rotr(x, 0x19);
}

// σ0(x) (4.6)
static inline uint32_t sig0(uint32_t x)
{
    return rotr(x, 0x07) ^ rotr(x, 0x12) ^ shr(x, 0x03);
}

// σ1(x) (4.7)
static inline uint32_t sig1(uint32_t x)
{
    return rotr(x, 0x11) ^ rotr(x, 0x13) ^ shr(x, 0x0a);
}

void sha256_init(sha256_t *ctx)
{
    memset(ctx, 0, sizeof(sha256_t));

    // (5.3.2)
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

static void sha256_hash(sha256_t *ctx, const uint8_t *data)
{
    // (4.2.2)
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    // Message schedule (6.2.2.1)
    uint32_t w[64];
    for (size_t i = 0; i < 64; i++) {
        if (i < 16) {
            w[i] = (uint32_t)data[4 * i + 0] << 0x18 |
                   (uint32_t)data[4 * i + 1] << 0x10 |
                   (uint32_t)data[4 * i + 2] << 0x08 |
                   (uint32_t)data[4 * i + 3] << 0x00;
        }
        else {
            w[i] = sig1(w[i - 0x02]) +
                   sig0(w[i - 0x0f]) +
                   w[i - 0x07] +
                   w[i - 0x10];
        }
    }

    // (6.2.2.2)
    uint32_t s[8] = {
        ctx->state[0], // a
        ctx->state[1], // b
        ctx->state[2], // c
        ctx->state[3], // d
        ctx->state[4], // e
        ctx->state[5], // f
        ctx->state[6], // g
        ctx->state[7], // h
    };

    // Offsets for indexing into `s` in a way the resembles the spec
    // Elements are initialized to their index + 1 and get decremented prior computation
    enum vars {
        a, b, c, d, e, f, g, h
    };
    int32_t idx[] = {
        [a] = 1, [b] = 2, [c] = 3, [d] = 4, [e] = 5, [f] = 6, [g] = 7, [h] = 8
    };

    // pragma gcc pls unroll
    for (size_t i = 0; i < 64; i++) {
        for (size_t j = 0; j < 8; j++) {
            idx[j] = idx[j] ? idx[j] - 1 : 7;
        }
        // (6.2.2.3)
        s[idx[h]] += sum1(s[idx[e]]);
        s[idx[h]] += ch(s[idx[e]], s[idx[f]], s[idx[g]]);
        s[idx[h]] += k[i];
        s[idx[h]] += w[i];
        s[idx[d]] += s[idx[h]];
        s[idx[h]] += maj(s[idx[a]], s[idx[b]], s[idx[c]]);
        s[idx[h]] += sum0(s[idx[a]]);
    }
    // (6.2.2.4)
    for (size_t i = 0; i < 8; i++) {
        ctx->state[i] += s[i];
    }
}

void sha256_append(sha256_t *ctx, const void *src, size_t len)
{
    const uint8_t *data = (const uint8_t *)src;
    for (size_t i = 0; i < len; i++) {
        // Copy byte over to the context block and hash
        // in 512-bit increments
        ctx->data[ctx->block_bytes++] = data[i];
        if (ctx->block_bytes == 64) {
            sha256_hash(ctx, ctx->data);
            ctx->bits_total += 512;
            ctx->block_bytes = 0;
        }
    }
}

void sha256_finish(sha256_t *ctx, void *dst)
{
    uint8_t *hash = (uint8_t *)dst;
    uint32_t block = ctx->block_bytes;

    ctx->data[block++] = 128;
    const uint32_t end = ctx->block_bytes < 56 ? 56 : 64;
    while (block < end) {
        ctx->data[block++] = 0;
    }
    if (end == 64) {
        sha256_hash(ctx, ctx->data);
        memset(ctx->data, 0, 64);
    }

    // Add total length to the last 8 bytes
    ctx->bits_total += 8 * ctx->block_bytes;
    for (size_t i = 0; i < 8; i++) {
        ctx->data[63 - i] = (uint8_t)(ctx->bits_total >> (uint8_t)(8 * i));
    }
    sha256_hash(ctx, ctx->data);

    // Copy into destination in reverse order
    for (size_t i = 0; i < 4; i++) {
        for (size_t j = 0; j < 8; j++) {
            hash[(4 * j) + i] = (ctx->state[j] >> (24 - (8 * i))) & 0xff;
        }
    }
}
