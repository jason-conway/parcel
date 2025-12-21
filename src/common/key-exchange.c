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
    uint8_t secret_key[KEY_LEN] = { 0 };
    uint8_t public_key[KEY_LEN] = { 0 };

    point_d(secret_key);
    point_q(secret_key, public_key, NULL);

    // Send public key to begin
    if (!xsendall(socket, public_key, KEY_LEN)) {
        log_fatal("failed to send public key to server");
        return false;
    }

    uint8_t server_public_key[KEY_LEN] = { 0 };
    if (!xrecvall(socket, server_public_key, KEY_LEN)) {
        log_fatal("failed to receive server's public key");
        return false;
    }

    uint8_t shared_secret[KEY_LEN] = { 0 };
    point_kx(shared_secret, secret_key, server_public_key);

    ssize_t key_exchange_wire_length = sizeof(wire_t) + KEY_LEN;
    wire_t *wire = xcalloc(key_exchange_wire_length);

    if (!xrecvall(socket, wire, key_exchange_wire_length)) {
        log_fatal("failed to receive wire from server");
        return false;
    }

    // Shared secret gets hashed in point_kx()
    size_t data_length = 0;
    if (decrypt_wire(wire, &data_length, shared_secret)) {
        log_fatal("decryption failure");
        return false;
    }

    memcpy(ctrl_key, wire->data, KEY_LEN);
    xfree(wire);
    return true;
}

bool two_party_server(sock_t socket, uint8_t *session_key)
{
    // Receive public key from the client
    uint8_t public_key[KEY_LEN] = { 0 };
    if (!xrecvall(socket, public_key, KEY_LEN)) {
        log_fatal("failed to receive public key from client");
        return false;
    }

    // Generate a single-use secret key for the key pair
    uint8_t secret_key[KEY_LEN] = { 0 };
    point_d(secret_key);

    // Compute a single-use public key and our shared secret with the client
    uint8_t server_public_key[KEY_LEN] = { 0 };
    point_q(secret_key, server_public_key, NULL);

    if (!xsendall(socket, server_public_key, KEY_LEN)) {
        log_fatal("did not send full key length");
        return false;
    }

    uint8_t shared_secret[KEY_LEN] = { 0 };
    point_kx(shared_secret, secret_key, public_key);

    size_t len = KEY_LEN;
    wire_t *wire = init_wire(session_key, TYPE_TEXT, &len);
    encrypt_wire(wire, shared_secret);
    if (!xsendall(socket, wire, len)) {
        log_fatal("failed to send session key to client");
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

    uint8_t renewed_key[32] = { 0 };
    if (xgetrandom(renewed_key, KEY_LEN) < 0) {
        return false;
    }
    wire_set_ctrl_msg_key(&ctrl_message, renewed_key);

    wire_t *wire = init_wire(&ctrl_message, TYPE_CTRL, &len);
    encrypt_wire(wire, ctrl_key);

    // Update key
    memcpy(ctrl_key, renewed_key, KEY_LEN);

    for (size_t i = 1; i <= count; i++) {
        log_trace("sending control key to socket %zu", i);
        if (!xsendall(sockets[i], wire, len)) {
            log_fatal("failed to send control key to socket %zu", i);
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
        uint8_t intermediate_key[KEY_LEN] = { 0 };
        log_trace("receiving intermediate key from socket %zu", i);
        if (!xrecvall(sockets[i], intermediate_key, KEY_LEN)) {
            log_fatal("failed to receive intermediate key from socket %zu", i);
            return false;
        }

        // Rotate right, skip server's socket
        const size_t next = (i == count) ? 1 : i + 1; 
        log_trace("sending intermediate key to socket %zu", next);
        if (!xsendall(sockets[next], intermediate_key, KEY_LEN)) {
            log_fatal("failed to send intermediate key to socket %zu", next);
            return false;
        }
    }
    return true;
}

bool n_party_server(sock_t *sockets, size_t connection_count, uint8_t *ctrl_key)
{
    assert(connection_count != 0);

    const size_t rounds = connection_count - 1;
    if (rounds < 1) {
        log_info("skipping `n_party_server`");
        return true;
    }

    log_trace("sending CTRL to signal start of sequence");
    if (!send_ctrl_key(sockets, connection_count, ctrl_key)) {
        log_fatal("failed to send control key");
        return false;
    }
    log_trace("all control keys sent");
    log_debug("starting %zu-party DHKE sequence", connection_count);
    log_debug("%zu-round%s required", rounds, rounds > 1 ? "s" : "");

    for (size_t i = 0; i < rounds; i++) {
        log_trace("starting exchange round %zu of %zu", i + 1, rounds);
        if (!rotate_intermediates(sockets, connection_count)) {
            return false;
        }
        log_trace("round %zu complete", i + 1);
    }

    return true;
}

// An N-Party Diffie-Hellman Key Exchange
bool n_party_client(sock_t socket, uint8_t *session_key, size_t rounds)
{
    uint8_t secret_key[KEY_LEN] = { 0 };
    uint8_t public_key[KEY_LEN] = { 0 };

    point_d(secret_key);
    point_q(secret_key, public_key, NULL);

    // Send our public key to the client on our right
    if (!xsendall(socket, public_key, KEY_LEN)) {
        log_fatal("failed to send public key (round 0)");
        return false;
    }

    log_debug("sent public key");

    for (size_t i = 0; i < rounds; i++) {
        log_trace("starting round %zu", i + 1);
        uint8_t intermediate_public[KEY_LEN] = { 0 };
        if (!xrecvall(socket, intermediate_public, KEY_LEN)) {
            log_fatal("failed to receive intermediate key (round %zu)", i + 1);
            return false;
        }

        log_trace("received key for round %zu", i + 1);
        uint8_t shared_secret[KEY_LEN] = { 0 };
        point_kx(shared_secret, secret_key, intermediate_public);

        if (i == rounds - 1) {
            sha256_key_digest(shared_secret, session_key);
            log_debug("key exchange complete");
            return true;
        }
        if (!xsendall(socket, shared_secret, KEY_LEN)) {
            log_fatal("failed to send intermediate key (round %zu)", i);
            return false;
        }
        log_trace("sent intermediate key (round %zu)", i + 1);
    }
    return true;
}
