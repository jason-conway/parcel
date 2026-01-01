/**
 * @file wire.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Wire is an encyption/decryption tool for parcel built on AES in CBC mode
 * @version 0.9.2
 * @date 2022-02-06
 *
 * @copyright Copyright (c) 2022 - 2026 Jason Conway. All rights reserved.
 *
 */

#include "wire.h"
#include "log.h"
#include "wire-util.h"
#include "xplatform.h"
#include "xutils.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

size_t get_aligned_len(size_t len)
{
    return round_up(len, BLOCK_LEN);
}

size_t header_get_alignment(const header_t *header)
{
    return header->alignment;
}

size_t wire_get_alignment(const wire_t *wire)
{
    return header_get_alignment(&wire->header);
}

size_t header_get_length(header_t *header)
{
    return wire_pack64(header->wire_len);
}

// length of entire wire including header
size_t wire_get_length(wire_t *wire)
{
    return header_get_length(&wire->header);
}

size_t header_get_data_length(header_t *header)
{
    size_t wire_length = header_get_length(header);
    size_t alignment = header_get_alignment(header);
    return wire_length - sizeof(wire_t) - alignment;
}

size_t header_get_aligned_data_length(header_t *header)
{
    const size_t wire_length = header_get_length(header);
    return wire_length - sizeof(wire_t);
}

size_t wire_get_data_length(wire_t *wire)
{
    return header_get_data_length(&wire->header);
}

size_t wire_get_aligned_data_length(wire_t *wire)
{
    return header_get_aligned_data_length(&wire->header);
}

void wire_set_header(wire_t *wire, header_t *hdr)
{
    memcpy(&wire->header, hdr, sizeof(header_t));
}

void wire_set_length(wire_t *wire, size_t len)
{
    wire_unpack64(wire->header.wire_len, len);
}

void wire_set_alignment(wire_t *wire, size_t alignment)
{
    wire->header.alignment = alignment;
}

void header_set_signature(header_t *header)
{
    memcpy(header->signature, "-wire-", sizeof(header->signature));
}

void wire_set_signature(wire_t *wire)
{
    header_set_signature(&wire->header);
}

bool header_check_signature(header_t *header)
{
    return !memcmp(header->signature, "-wire-", sizeof(header->signature));
}

bool wire_check_signature(wire_t *wire)
{
    return header_check_signature(&wire->header);
}

void wire_set_type(wire_t *wire, wire_type_t type)
{
    wire->header.type = (uint8_t)type;
}

wire_type_t wire_get_type(const wire_t *wire)
{
    return wire->header.type;
}


header_t wire_decrypt_header(aes128_t *aes128, wire_t *wire)
{
    _Static_assert(sizeof(header_t) == BLOCK_LEN, "header_t must be equal to BLOCK_LEN");

    header_t h = { 0 };
    memcpy(&h, &wire->header, sizeof(header_t));
    aes128_decrypt(aes128, (void *)&h, BLOCK_LEN);
    if (!header_check_signature(&h)) {
        log_error("header signature invalid");
        return (header_t) { 0 };
    }
    return h;
}

// `len` is the aligned data length
void wire_decrypt_data(aes128_t *aes128, wire_t *wire, size_t len)
{
    aes128_decrypt(aes128, wire->data, len);
}

bool wire_verify_outer_mac(aes128_t *aes128, wire_t *wire, size_t wire_len)
{
    uint8_t cmac[BLOCK_LEN] = { 0 };
    aes128_cmac(aes128, wire->auth.mac_inner, wire_len - WIRE_OFFSET_MAC_INNER, cmac);
    return !memcmp(&wire->auth.mac_outer[0], cmac, BLOCK_LEN);
}

bool wire_verify_inner_mac(aes128_t *aes128, wire_t *wire)
{
    uint8_t cmac[BLOCK_LEN] = { 0 };
    aes128_cmac(aes128, (uint8_t *)&wire->header, BLOCK_LEN, cmac);
    return !memcmp(&wire->auth.mac_inner[0], cmac, BLOCK_LEN);
}

wire_t *alloc_wire(void)
{
    return xcalloc(RECV_MAX_BYTES);
}

static bool wire_auth_init_iv(wire_t *wire)
{
    return xgetrandom(wire->auth.iv, BLOCK_LEN) == BLOCK_LEN;
}

static void wire_set_data(wire_t *wire, const void *data, size_t len)
{
    memcpy(wire->data, data, len);
}


// len should point to the length of the data to be added
// len is updated to the total wire length
wire_t *init_wire(wire_type_t type, const void *data, size_t *len)
{
    static const char *types[] = {
        [TYPE_NONE] = "TYPE_NONE",
        [TYPE_TEXT] = "TYPE_TEXT",
        [TYPE_FILE] = "TYPE_FILE",
        [TYPE_CTRL] = "TYPE_CTRL",
        [TYPE_STAT] = "TYPE_STAT",
        [TYPE_SESSION_KEY] = "TYPE_SESSION_KEY",
    };
    log_trace("init_wire(%s)", types[type]);

    const size_t data_length = get_aligned_len(*len);
    const size_t alignment = data_length - *len;
    const size_t wire_length = sizeof(wire_t) + data_length;

    log_trace("  payload length: %zu bytes (%zu bytes aligned)", *len, data_length);
    log_trace("  padding required: %zu bytes", alignment);
    log_trace("  total wire length: %zu bytes", wire_length);

    wire_t *wire = xcalloc(wire_length);

    if (!wire_auth_init_iv(wire)) {
        xfree(wire);
        return NULL;
    }

    wire_set_signature(wire);
    wire_set_alignment(wire, alignment);
    wire_set_length(wire, wire_length);
    wire_set_type(wire, type);
    wire_set_data(wire, data, *len);

    *len = wire_length;
    return wire;
}

// [note] `len` must be passed explicitly here since `wire` is already encrypted
static void wire_gen_cmacs(const aes128_t *cmac, wire_t *wire, size_t len)
{
    // Generate inner MAC
    aes128_cmac(cmac, (uint8_t *)&wire->header, BLOCK_LEN, wire->auth.mac_inner);
    
    // Generate outer MAC over
    //  - inner MAC
    //  - IV
    //  - header
    //  - data
    aes128_cmac(cmac, wire->auth.mac_inner, len - WIRE_OFFSET_MAC_INNER, wire->auth.mac_outer);
}

bool encrypt_wire(wire_t *wire, const uint8_t *key)
{
    if (!wire || !key) {
        return false;
    }

    aes128_t cipher = { 0 };
    aes128_t cmac = { 0 };

    aes128_init(&cipher, wire->auth.iv, &key[CIPHER_OFFSET]);
    aes128_init_cmac(&cmac, &key[CMAC_OFFSET]);

    // Grab block-aligned data length from wire
    size_t data_len = wire_get_aligned_data_length(wire);
    size_t wire_len = wire_get_length(wire);

    // Encrypt chunks
    aes128_encrypt(&cipher, (uint8_t *)&wire->header, BLOCK_LEN);
    aes128_encrypt(&cipher, wire->data, data_len);

    // Generate inner and outer CMAC
    wire_gen_cmacs(&cmac, wire, wire_len);
    
    return true;
}

bool decrypt_wire(wire_t *wire, size_t len, const uint8_t *key1, const uint8_t *key2)
{
    aes128_t cipher = { 0 };
    aes128_t cmac = { 0 };

    const uint8_t *key = key1;
init:
    aes128_init(&cipher, wire->auth.iv, &key[CIPHER_OFFSET]);
    aes128_init_cmac(&cmac, &key[CMAC_OFFSET]);

    // Decrypt only the length
    if (!wire_verify_inner_mac(&cmac, wire)) {
        if (key == key1) {
            if (!key2) {
                log_fatal("inner mac verification failure");
                return false;
            }
            log_trace("key1 inner mac verification failure, trying key2");
            key = key2;
            goto init;
        }
        log_fatal("inner mac verification failure");
        log_fatal("session and ctrl keys out of sync");
        return false;
    }

    header_t h = wire_decrypt_header(&cipher, wire);
    size_t wire_len = header_get_length(&h);
    if (len != wire_len) {
        log_error("wire length (%zu bytes) doesn't match received length (%zu bytes)", wire_len, len);
        return false;
    }

    size_t aligned_len = header_get_aligned_data_length(&h);

    // Verify MAC prior to decrypting in full
    if (!wire_verify_outer_mac(&cmac, wire, wire_len)) {
        log_fatal("outer mac verification failure");
        return false;
    }

    wire_decrypt_data(&cipher, wire, aligned_len);

    // copy the decrypted header back into the wire
    wire_set_header(wire, &h);

    return true;
}
