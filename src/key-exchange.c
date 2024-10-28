/**
 * @file key-exchange.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Parcel Client-Server Key Exchange
 * @version 0.9.2
 * @date 2022-02-01
 *
 * @copyright Copyright (c) 2022 - 2024 Jason Conway. All rights reserved.
 *
 */

#include "key-exchange.h"

void sha256_key_digest(const uint8_t *key, uint8_t *hash)
{
    sha256_t ctx;
    sha256_init(&ctx);
    sha256_append(&ctx, key, 32);
    sha256_finish(&ctx, hash);
}

// ECDH private key ( {d ∈ ℕ | d < n} )
static void point_d(uint8_t *dst)
{
    (void)xgetrandom(dst, KEY_LEN);
    dst[0x00] &= 0xf8;
    dst[0x1f] &= 0x7f;
    dst[0x1f] |= 0x40;
}

// ECDH public key (Q = d * G)
static void point_q(const uint8_t *secret_key, uint8_t *public_key, uint8_t *fingerprint)
{
    const uint8_t basepoint[KEY_LEN] = { 9 };
    x25519(public_key, secret_key, basepoint);
    if (fingerprint) {
        uint8_t hash[KEY_LEN];
        sha256_key_digest(public_key, hash);
        memcpy(fingerprint, hash, 16);
    }
}

// EDCH shared secret
static void point_kx(uint8_t *shared_key, const uint8_t *secret_key, const uint8_t *public_key)
{
    x25519(shared_key, secret_key, public_key);
}

bool two_party_client(sock_t socket, uint8_t *ctrl_key)
{
    // Diffie-Hellman keys
    uint8_t secret_key[KEY_LEN];
    uint8_t public_key[KEY_LEN];
    point_d(secret_key);
    point_q(secret_key, public_key, NULL);

    // Send public key to begin
    if (xsend(socket, public_key, KEY_LEN, 0) != KEY_LEN) {
        debug_print("%s\n", "Did not send full key length");
        return false;
    }

    uint8_t server_public_key[KEY_LEN];
    if (xrecv(socket, server_public_key, KEY_LEN, 0) != KEY_LEN) {
        debug_print("%s\n", "Did not receive full key length");
        return false;
    }

    uint8_t shared_secret[KEY_LEN];
    point_kx(shared_secret, secret_key, server_public_key);

    ssize_t key_exchange_wire_length = sizeof(wire_t) + KEY_LEN;
    wire_t *wire = xcalloc(key_exchange_wire_length);

    if (xrecv(socket, wire, key_exchange_wire_length, 0) != key_exchange_wire_length) {
        debug_print("Did not receive full key_exchange_wire_length (%zi)\n", key_exchange_wire_length);
        return false;
    }

    // Shared secret gets hashed in point_kx()
    size_t data_length = 0;
    if (decrypt_wire(wire, &data_length, shared_secret)) {
        return false;
    }

    memcpy(ctrl_key, wire->data, KEY_LEN);
    xfree(wire);
    return true;
}

bool two_party_server(sock_t socket, uint8_t *session_key)
{
    // Receive public key from the client
    uint8_t public_key[KEY_LEN];
    if (xrecv(socket, public_key, KEY_LEN, 0) != KEY_LEN) {
        debug_print("%s\n", "Did not receive full key length");
        return false;
    }

    // Generate a single-use secret key for the key pair
    uint8_t secret_key[KEY_LEN];
    point_d(secret_key);

    // Compute a single-use public key and our shared secret with the client
    uint8_t server_public_key[KEY_LEN];
    point_q(secret_key, server_public_key, NULL);

    if (xsend(socket, server_public_key, KEY_LEN, 0) != KEY_LEN) {
        debug_print("%s\n", "Did not send full key length");
        return false;
    }

    uint8_t shared_secret[KEY_LEN];
    point_kx(shared_secret, secret_key, public_key);

    size_t len = KEY_LEN;
    wire_t *wire = init_wire(session_key, TYPE_TEXT, &len);
    encrypt_wire(wire, shared_secret);
    if (xsend(socket, wire, len, 0) != (ssize_t)len) {
        debug_print("Did not send full session_key (%zu)\n", len);
        return false;
    }

    xfree(wire);
    return true;
}

static bool send_ctrl_key(sock_t *sockets, size_t count, uint8_t *ctrl_key)
{
    ctrl_msg_t ctrl_message;
    size_t len = sizeof(ctrl_msg_t);

    memset(&ctrl_message, 0, len);

    wire_set_ctrl_msg_type(&ctrl_message, CTRL_DHKE);
    wire_set_ctrl_msg_args(&ctrl_message, count - 1);

    uint8_t renewed_key[32];
    if (xgetrandom(renewed_key, KEY_LEN) < 0) {
        return false;
    }
    wire_set_ctrl_msg_key(&ctrl_message, renewed_key);

    wire_t *wire = init_wire(&ctrl_message, TYPE_CTRL, &len);
    encrypt_wire(wire, ctrl_key);

    // Update key
    memcpy(ctrl_key, renewed_key, KEY_LEN);

    for (size_t i = 1; i <= count; i++) {
        debug_print("Sending CTRL key to socket %zu\n", i);
        if (xsend(sockets[i], wire, len, 0) < 0) {
            xfree(wire);
            return false;
        }
    }

    xfree(wire);
    return true;
}

/*
 *    A          B          C          D          E          F
 *    |          |          |          |          |          |
 *    |          |          |          |          |          |
 *    |-Qa------>|-Qb------>|-Qc------>|-Qd------>|-Qe------>|
 *    |          |          |          |          |          |
 *    |<--------------------------------------------------Qf-|
 *    |          |          |          |          |          |
 *    |          |          |          |          |          |
 *    |-Qfa----->|-Qab----->|-Qbc----->|-Qcd----->|-Qde----->|
 *    |          |          |          |          |          |
 *    |<-------------------------------------------------Qef-|
 *    |          |          |          |          |          |
 *    |          |          |          |          |          |
 *    |-Qefa---->|-Qfab---->|-Qabc---->|-Qbcd---->|-Qcde---->|
 *    |          |          |          |          |          |
 *    |<------------------------------------------------Qdef-|
 *    |          |          |          |          |          |
 *    |          |          |          |          |          |
 *    |-Qdefa--->|-Qefab--->|-Qfabc--->|-Qabcd--->|-Qbcde--->|
 *    |          |          |          |          |          |
 *    |<-----------------------------------------------Qcdef-|
 *    |          |          |          |          |          |
 *    |          |          |          |          |          |
 *    |-Qcdefa-->|-Qdefab-->|-Qefabc-->|-Qfabcd-->|-Qabcde-->|
 *    |          |          |          |          |          |
 *    |<----------------------------------------------Qbcdef-|
 *    |          |          |          |          |          |
 */
static bool rotate_intermediates(sock_t *sockets, size_t count)
{
    for (size_t i = 1; i <= count; i++) {
        uint8_t intermediate_key[KEY_LEN];
        debug_print("Receiving intermediate key from socket %zu\n", i);
        ssize_t r = xrecv(sockets[i], intermediate_key, KEY_LEN, 0);
        if (r != KEY_LEN) {
            debug_print("error: received %zi bytes from socket %zu\n", r, i);
            return false;
        }

        const size_t next = (i == count) ? 1 : i + 1; // Rotate right, skip server's socket
        debug_print("Sending intermediate key to socket %zu\n", next);
        ssize_t s = xsend(sockets[next], intermediate_key, KEY_LEN, 0);
        if (s != KEY_LEN) {
            debug_print("error: sent %zi bytes to socket %zu\n", s, next);
            return false;
        }
    }
    return true;
}

bool n_party_server(sock_t *sockets, size_t connection_count, uint8_t *ctrl_key)
{
    if (connection_count < 2) {
        return true;
    }

    debug_print("%s\n", "Sending CTRL to signal start of sequence");
    if (!send_ctrl_key(sockets, connection_count, ctrl_key)) {
        printf("> Error sending starting control keys\n");
        return false;
    }
    debug_print("%s\n", "CTRL signals sent");

    for (size_t i = 0; i < connection_count - 1; i++) {
        debug_print("Starting exchange round %zu of %zu\n", i + 1, connection_count - 1);
        if (!rotate_intermediates(sockets, connection_count)) {
            return false;
        }
        debug_print("Finished round %zu\n", i + 1);
    }

    return true;
}

// An N-Party Diffie-Hellman Key Exchange
bool n_party_client(sock_t socket, uint8_t *session_key, size_t rounds)
{
    uint8_t secret_key[KEY_LEN];
    uint8_t public_key[KEY_LEN];

    point_d(secret_key);
    point_q(secret_key, public_key, NULL);

    // Send our public key to the client on our right
    if (xsend(socket, public_key, KEY_LEN, 0) != KEY_LEN) {
        debug_print("%s\n", "Did not send full key length");
        return false;
    }
    debug_print("%s\n", "Public key sent");

    for (size_t i = 0; i < rounds; i++) {
        debug_print("Starting round %zu\n", i + 1);
        uint8_t intermediate_public[KEY_LEN];
        if (xrecv(socket, intermediate_public, KEY_LEN, 0) != KEY_LEN) {
            debug_print("Did not receive full key during round %zu\n", i + 1);
            return false;
        }
        debug_print("Received key for round %zu\n", i + 1);
        uint8_t shared_secret[KEY_LEN];
        point_kx(shared_secret, secret_key, intermediate_public);

        if (i == rounds - 1) {
            sha256_key_digest(shared_secret, session_key);
            debug_print("%s\n", "Exchange complete");
            return true;
        }

        if (xsend(socket, shared_secret, KEY_LEN, 0) != KEY_LEN) {
            debug_print("Did not send full key during round %zu\n", i);
            return false;
        }
        debug_print("Sent intermediate for round %zu\n", i + 1);
    }
    return true;
}
