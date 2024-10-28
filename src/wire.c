/**
 * @file wire.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Wire is an encyption/decryption tool for parcel built on AES in CBC mode
 * @version 0.9.2
 * @date 2022-02-06
 *
 * @copyright Copyright (c) 2022 - 2024 Jason Conway. All rights reserved.
 *
 */

#include "wire.h"

// Pack byte array into 64-bit word
uint64_t wire_pack64(const uint8_t *src)
{
    return ((uint64_t)src[0] << 0x00) |
           ((uint64_t)src[1] << 0x08) |
           ((uint64_t)src[2] << 0x10) |
           ((uint64_t)src[3] << 0x18) |
           ((uint64_t)src[4] << 0x20) |
           ((uint64_t)src[5] << 0x28) |
           ((uint64_t)src[6] << 0x30) |
           ((uint64_t)src[7] << 0x38);
}

// Unpack a 64-bit word into a little-endian array of bytes
void wire_unpack64(uint8_t *dst, uint64_t src)
{
    dst[0] = (uint8_t)(src >> 0x00);
    dst[1] = (uint8_t)(src >> 0x08);
    dst[2] = (uint8_t)(src >> 0x10);
    dst[3] = (uint8_t)(src >> 0x18);
    dst[4] = (uint8_t)(src >> 0x20);
    dst[5] = (uint8_t)(src >> 0x28);
    dst[6] = (uint8_t)(src >> 0x30);
    dst[7] = (uint8_t)(src >> 0x38);
}

size_t get_aligned_len(size_t len)
{
    return BLOCK_LEN * ((len + 15) / BLOCK_LEN);
}

void wire_set_length(wire_t *wire, size_t len)
{
    wire_unpack64(wire->header.len, len);
}

size_t wire_get_length(wire_t *wire)
{
    return wire_pack64(wire->header.len);
}

void header_set_len(header_t *h, size_t len)
{
    wire_unpack64(h->len, len);
}

size_t header_get_len(header_t *h)
{
    return wire_pack64(h->len);
}

void wire_set_header(wire_t *wire, header_t *h)
{
    memcpy(&wire->header, h, sizeof(header_t));
}

void wire_set_file_msg_size(file_msg_t *f, size_t size)
{
    wire_unpack64(f->size, size);
}
size_t wire_get_file_msg_size(file_msg_t *f)
{
    return wire_pack64(f->size);
}

void wire_set_file_msg_filename(file_msg_t *f, const char *filename, size_t len)
{
    memcpy(f->filename, filename, len);
}
void wire_set_file_msg_data(file_msg_t *f, const void *data, size_t len)
{
    memcpy(f->data, data, len);
}

// MARK: msg_type

void wire_set_msg_type(wire_t *wire, msg_type_t msg_type)
{
    wire_unpack64(wire->header.type, msg_type);
}

msg_type_t wire_get_msg_type(wire_t *wire)
{
    return wire_pack64(wire->header.type);
}


// MARK: ctrl_msg_type

void wire_set_ctrl_msg_type(ctrl_msg_t *ctrl, ctrl_msg_type_t msg_type)
{
    wire_unpack64(ctrl->type, msg_type);
}

ctrl_msg_type_t wire_get_ctrl_msg_type(ctrl_msg_t *ctrl)
{
    return wire_pack64(ctrl->type);
}

// MARK: ctrl_msg_args

uint64_t wire_get_ctrl_msg_args(ctrl_msg_t *ctrl)
{
    return wire_pack64(ctrl->args);
}

void wire_set_ctrl_msg_args(ctrl_msg_t *ctrl, uint64_t args)
{
    wire_unpack64(ctrl->args, args);
}

// MARK: ctrl_msg_key

void wire_set_ctrl_msg_key(ctrl_msg_t *ctrl, const uint8_t *renewed_key)
{
    memcpy(ctrl->renewed_key, renewed_key, KEY_LEN);
}

// MARK: stat_msg

void wire_set_stat_msg_user(stat_msg_t *stat, const char *user)
{
    memcpy(stat->user, user, sizeof(stat->user));
}

void wire_set_stat_msg_data(stat_msg_t *stat, const void *data)
{
    memcpy(stat->data, data, sizeof(stat->data));
}

void wire_set_stat_msg_type(stat_msg_t *stat, stat_msg_type_t type)
{
    wire_unpack64(stat->type, type);
}

stat_msg_type_t wire_get_stat_msg_type(stat_msg_t *stat)
{
    return wire_pack64(stat->type);
}

// MARK: text_msg

void wire_set_text_msg_user(text_msg_t *text, const char *user)
{
    memcpy(text->user, user, sizeof(text->user));
}

void wire_set_text_msg_data(text_msg_t *text, const void *data, size_t len)
{
    memcpy(text->data, data, len);
}

// MARK: decrypt_header

header_t wire_decrypt_header(aes128_t *aes128, wire_t *wire)
{
    header_t h = { 0 };
    memcpy(h.len, (uint8_t *)&wire->header, BLOCK_LEN);
    aes128_decrypt(aes128, h.len, BLOCK_LEN);
    return h;
}

void wire_decrypt_data(aes128_t *aes128, wire_t *wire, size_t data_length)
{
    aes128_decrypt(aes128, wire->data, data_length);
}

bool wire_verify_mac(aes128_t *aes128, wire_t *wire, size_t data_length)
{
    uint8_t cmac[BLOCK_LEN] = { 0 };
    aes128_cmac(aes128, wire->lac, data_length + BASE_AUTH_LEN, cmac);
    return !memcmp(&wire->mac[0], cmac, BLOCK_LEN);
}

bool wire_verify_header(aes128_t *aes128, wire_t *wire)
{
    uint8_t cmac[BLOCK_LEN] = { 0 };
    aes128_cmac(aes128, (uint8_t *)&wire->header, BLOCK_LEN, cmac);
    return !memcmp(&wire->lac[0], cmac, BLOCK_LEN);
}

wire_t *new_wire(void)
{
    return xcalloc(RECV_MAX_BYTES);
}

static bool wire_init_iv(wire_t *wire)
{
    return xgetrandom(wire->iv, BLOCK_LEN) == BLOCK_LEN;
}

static void wire_set_data(wire_t *wire, const void *data, size_t len)
{
    memcpy(wire->data, data, len);
}


// len should point to the length of the data to be added
// len is updated to the total wire length
wire_t *init_wire(void *data, uint64_t type, size_t *len)
{
    const uint64_t data_length = get_aligned_len(*len);
    const size_t wire_length = sizeof(wire_t) + data_length;
    wire_t *wire = xcalloc(wire_length);

    if (!wire_init_iv(wire)) {
        xfree(wire);
        return NULL;
    }

    wire_set_length(wire, *len);
    wire_set_msg_type(wire, type);
    wire_set_data(wire, data, *len);

    *len = wire_length;
    return wire;
}

size_t encrypt_wire(wire_t *wire, const uint8_t *key)
{
    aes128_t cipher = { 0 };
    aes128_t cmac = { 0 };
    
    aes128_init(&cipher, wire->iv, &key[CIPHER_OFFSET]);
    aes128_init_cmac(&cmac, &key[CMAC_OFFSET]);

    // Grab length from wire
    const size_t data_len = wire_get_length(wire);
    size_t aligned_len = get_aligned_len(data_len);

    // Encrypt chunks
    aes128_encrypt(&cipher, (uint8_t *)&wire->header, BLOCK_LEN);
    aes128_encrypt(&cipher, wire->data, aligned_len);

    // Header MAC
    aes128_cmac(&cmac, (uint8_t *)&wire->header, BLOCK_LEN, wire->lac);

    // MAC for LAC, IV, length, type, and chunks into the wire
    aes128_cmac(&cmac, wire->lac, aligned_len + BASE_AUTH_LEN, wire->mac);
    return aligned_len;
}

int decrypt_wire(wire_t *wire, size_t *len, const uint8_t *key)
{
    aes128_t cipher = { 0 };
    aes128_t cmac = { 0 };

    aes128_init(&cipher, wire->iv, &key[CIPHER_OFFSET]);
    aes128_init_cmac(&cmac, &key[CMAC_OFFSET]);

    // Decrypt only the length
    if (!wire_verify_header(&cmac, wire)) {
        debug_print("%s", "error: header verification failed\n");
        return WIRE_INVALID_KEY;
    }

    header_t h = wire_decrypt_header(&cipher, wire);
    size_t data_len = header_get_len(&h);
    size_t aligned_len = get_aligned_len(data_len);

    size_t wire_length = aligned_len + sizeof(wire_t);
    if (*len && *len != wire_length) {
        const size_t received = *len;
        *len = wire_length - received; // Update with bytes remaining
        return WIRE_PARTIAL;
    }

    *len = data_len;

    // Verify MAC prior to decrypting in full
    if (!wire_verify_mac(&cmac, wire, aligned_len)) {
        debug_print("%s", "error: mac verification failed\n");
        return WIRE_CMAC_ERROR;
    }

    wire_decrypt_data(&cipher, wire, aligned_len);
    wire_set_header(wire, &h);

    debug_print("%s", "decrypt okay\n");
    return WIRE_OK;
}
