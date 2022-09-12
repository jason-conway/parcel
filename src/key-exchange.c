/**
 * @file key-exchange.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Parcel Client-Server Key Exchange
 * @version 0.9.2
 * @date 2022-02-01
 *
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 *
 */

#include "key-exchange.h"

// ECDH private key ( {d ∈ ℕ | d < n} )
static void point_d(uint8_t *dest)
{
	(void)xgetrandom(dest, KEY_LEN);
	dest[0] &= 248;
	dest[31] &= 127;
	dest[31] |= 64;
}

// ECDH public key (Q = d * G)
static void point_q(const uint8_t *secret_key, uint8_t *public_key, uint8_t *fingerprint)
{
	const uint8_t basepoint[KEY_LEN] = { 9 };
	x25519(public_key, secret_key, basepoint);
	if (fingerprint != NULL) {
		uint8_t hash[KEY_LEN];
		sha256_digest(public_key, hash);
		memcpy(fingerprint, hash, 16);
	}
}

// EDCH shared secret
static void point_kx(uint8_t *shared_key, const uint8_t *secret_key, const uint8_t *public_key)
{
	x25519(shared_key, secret_key, public_key);
}

int two_party_client(sock_t socket, uint8_t *ctrl_key)
{
	// Diffie-Hellman keys
	uint8_t secret_key[KEY_LEN];
	uint8_t public_key[KEY_LEN];
	point_d(secret_key);
	point_q(secret_key, public_key, NULL);

	// Send public key to begin
	if (xsend(socket, public_key, KEY_LEN, 0) < 0) {
		return -1;
	}

	uint8_t server_public_key[KEY_LEN];
	if (xrecv(socket, server_public_key, KEY_LEN, 0) != KEY_LEN) {
		return -1;
	}

	uint8_t shared_secret[KEY_LEN];
	point_kx(shared_secret, secret_key, server_public_key);

	ssize_t key_exchange_wire_length = sizeof(wire_t) + KEY_LEN;
	wire_t *wire = xcalloc(key_exchange_wire_length);
	if (!wire) {
		return -1;
	}
	
	if (xrecv(socket, wire, key_exchange_wire_length, 0) != key_exchange_wire_length) {
		return -1;
	}

	// Shared secret gets hashed in point_kx()
	size_t data_length = 0;
	if (decrypt_wire(wire, &data_length, shared_secret)) {
		return -1;
	}

	memcpy(ctrl_key, wire->data, KEY_LEN);
	xfree(wire);
	return 0;
}

int two_party_server(sock_t socket, uint8_t *session_key)
{
	// Receive public key from the client
	uint8_t public_key[KEY_LEN];
	if (xrecv(socket, public_key, KEY_LEN, 0) != KEY_LEN) {
		return -1;
	}

	// Generate a single-use secret key for the key pair
	uint8_t secret_key[KEY_LEN];
	point_d(secret_key);

	// Compute a single-use public key and our shared secret with the client
	uint8_t server_public_key[KEY_LEN];
	point_q(secret_key, server_public_key, NULL);

	if (xsend(socket, server_public_key, KEY_LEN, 0) < 0) {
		return -1;
	}

	uint8_t shared_secret[KEY_LEN];
	point_kx(shared_secret, secret_key, public_key);

	size_t len = KEY_LEN;
	wire_t *wire = init_wire(session_key, TYPE_TEXT, &len);
	encrypt_wire(wire, shared_secret);
	if (xsend(socket, wire, len, 0) < 0) {
		return -1;
	}

	xfree(wire);
	return 0;
}

static int send_ctrl_key(sock_t *sockets, size_t count, uint8_t *ctrl_key)
{
	struct wire_ctrl_message ctrl_message;
	size_t len = sizeof(struct wire_ctrl_message);

	memset(&ctrl_message, 0, len);
	wire_set_ctrl_function(&ctrl_message, CTRL_DHKE);
	wire_set_ctrl_args(&ctrl_message, count - 1);
	
	uint8_t renewed_key[32];
	if (xgetrandom(renewed_key, KEY_LEN) < 0) {
		return -1;
	}
	wire_set_ctrl_renewal(&ctrl_message, renewed_key);

	wire_t *wire = init_wire(&ctrl_message, TYPE_CTRL, &len);
	encrypt_wire(wire, ctrl_key);

	// Update key
	memcpy(ctrl_key, renewed_key, KEY_LEN);

	for (size_t i = 1; i <= count; i++) {
		// debug_print("Sending CTRL key to socket %zu\n", i);
		if (xsend(sockets[i], wire, len, 0) < 0) {
			xfree(wire);
			return -1;
		}
	}

	xfree(wire);
	return 0;
}

static int rotate_intermediates(sock_t *sockets, size_t count)
{
	for (size_t i = 1; i <= count; i++) {
		uint8_t intermediate_key[KEY_LEN];
		// debug_print("Receiving intermediate key from socket %zu\n", i);
		if (xrecv(sockets[i], intermediate_key, KEY_LEN, 0) != KEY_LEN) {
			return -1;
		}

		const size_t next = (i == count) ? 1 : i + 1; // Rotate right, skip server's socket
		// debug_print("Sending intermediate key to socket %zu\n", next);
		if (xsend(sockets[next], intermediate_key, KEY_LEN, 0) < 0) {
			printf("\n> Error sending to slot %zu\n", next);
			return -1;
		}
	}
	return 0;
}

int n_party_server(sock_t *sockets, size_t connection_count, uint8_t *ctrl_key)
{
	if (connection_count < 2) {
		return 0;
	}

	// debug_print("%s\n", "Sending CTRL to signal start of sequence");
	if (send_ctrl_key(sockets, connection_count, ctrl_key)) {
		printf("> Error sending starting control keys\n");
		return -1;
	}
	// debug_print("%s\n", "CTRL signals sent");

	for (size_t i = 0; i < connection_count - 1; i++) {
		// debug_print("Starting exchange round %zu of %zu\n", i + 1, connection_count - 1);
		if (rotate_intermediates(sockets, connection_count)) {
			return -1;
		}
		// debug_print("Finished round %zu\n", i + 1);
	}

	return 0;
}

// An N-Party Diffie-Hellman Key Exchange
int n_party_client(sock_t socket, uint8_t *session_key, size_t rounds)
{
	uint8_t secret_key[KEY_LEN];
	uint8_t public_key[KEY_LEN];
	
	point_d(secret_key);
	point_q(secret_key, public_key, NULL);

	// Send our public key to the client on our right
	if (xsend(socket, public_key, KEY_LEN, 0) != KEY_LEN) {
		return -1;
	}

	for (size_t i = 0; i < rounds; i++) {
		uint8_t intermediate_public[KEY_LEN];
		if (xrecv(socket, intermediate_public, KEY_LEN, 0) != KEY_LEN) {
			return -1;
		}

		uint8_t shared_secret[KEY_LEN];
		point_kx(shared_secret, secret_key, intermediate_public);

		if (i == rounds - 1) {
			sha256_digest(shared_secret, session_key);
			return 0;
		}

		if (xsend(socket, shared_secret, KEY_LEN, 0) != KEY_LEN) {
			return -1;
		}
	}
	return 0;
}
