/**
 * @file x25519.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Curve25519 Elliptic-Curve-Diffie-Hellman Implementation
 * @version 255.19
 * @date 2022-02-01
 *
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 *
 */

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief X25519 Elliptic-Curve Diffie-Hellman
 *
 * @param[out] public 32-byte public key / shared secret
 * @param[in] secret 32-byte secret key
 * @param[in] basepoint { 9 } / their_public
 *
 */
void x25519(uint8_t *public, const uint8_t *secret, const uint8_t *basepoint);
