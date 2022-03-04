/**
 * @file key-exchange.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Parcel Client-Server Key Exchange
 * @version 0.1
 * @date 2022-02-01
 *
 * @copyright Copyright (c) 2022 Jason Conway. All rights reserved.
 *
 */

#include "key-exchange.h"

// fprint (finger-print) - Print the fingerprint
void fprint(const char *str, const uint8_t *fingerprint)
{
	printf("\033[33m%s", str); // In yellow :D
	for (size_t i = 0; i < 16; i += 4) {
		uint64_t chunk = (((uint64_t)fingerprint[i + 0] << 0x18) |
						  ((uint64_t)fingerprint[i + 1] << 0x10) |
						  ((uint64_t)fingerprint[i + 2] << 0x08) |
						  ((uint64_t)fingerprint[i + 3] << 0x00));

		printf("%s%" PRIx64, i ? "-" : "", chunk);
	}
	printf("\033[0m\n");
}

static void generate_secret(uint8_t *dest)
{
	xgetrandom(dest, KEY_LEN);
	dest[0] &= 248;
	dest[31] &= 127;
	dest[31] |= 64;
}

static void compute_public(const uint8_t *secret_key, uint8_t *public_key, uint8_t *fingerprint)
{
	const uint8_t basepoint[KEY_LEN] = { 9 };
	x25519(public_key, secret_key, basepoint);
	if (fingerprint != NULL) {
		sha256_digest(public_key, fingerprint);
	}
}

static void compute_shared(uint8_t *shared_key, const uint8_t *secret_key, const uint8_t *public_key)
{
	uint8_t shared_secret[KEY_LEN] = { 0 };
	x25519(shared_secret, secret_key, public_key);
	sha256_digest(shared_secret, shared_key);
}


int master_key_exchange(const sock_t socket, uint8_t *key, uint8_t *fingerprint)
{

}

int slave_key_exchange(const sock_t socket, uint8_t *key, uint8_t *fingerprint)
{
	uint8_t secret_key[KEY_LEN];
	uint8_t public_key[KEY_LEN];
	generate_secret(secret_key);
	compute_public(secret_key, public_key, fingerprint);
}

int client_key_exchange(const sock_t socket, uint8_t *key, uint8_t *fingerprint)
{
	// Diffie-Hellman keys
	uint8_t secret_key[KEY_LEN];
	uint8_t public_key[KEY_LEN];
	generate_secret(secret_key);
	compute_public(secret_key, public_key, fingerprint);

	// Send public key to begin
	if (xsend(socket, public_key, KEY_LEN, 0) < 0) {
		return -1;
	}

	uint8_t server_public_key[KEY_LEN];
	if (xrecv(socket, server_public_key, KEY_LEN, 0) != KEY_LEN) {
		return -1;
	}

	uint8_t shared_secret[KEY_LEN] = { 0 };
	compute_shared(shared_secret, secret_key, server_public_key);

	ssize_t key_exchange_wire_length = sizeof(wire_t) + KEY_LEN;
	wire_t *wire = malloc(key_exchange_wire_length);
	if (!wire) {
		return -1;
	}
	memset(wire, 0, key_exchange_wire_length);

	if (xrecv(socket, wire, key_exchange_wire_length, 0) != key_exchange_wire_length) {
		return -1;
	}

	// Shared secret gets hashed in compute_shared()
	if (decrypt_wire(wire, shared_secret)) {
		return -1;
	}

	memcpy(key, wire->data, KEY_LEN);
	free(wire);
	return 0;
}

int server_key_exchange(const sock_t socket, const uint8_t *session_key)
{
	uint8_t shared_secret[KEY_LEN];
	uint8_t server_public_key[KEY_LEN];

	// Receive public key from the client
	uint8_t public_key[KEY_LEN];
	if (xrecv(socket, public_key, KEY_LEN, 0) != KEY_LEN) {
		return -1;
	}

	// Generate a single-use secret key for the key pair
	uint8_t secret_key[KEY_LEN];
	generate_secret(secret_key);
	// Compute a single-use public key and our shared secret with the client
	compute_public(secret_key, server_public_key, NULL);
	compute_shared(shared_secret, secret_key, public_key);

	// Send server public key to the client so they can compute the shared-secret
	if (xsend(socket, server_public_key, KEY_LEN, 0) < 0) {
		return -1;
	}

	size_t len = KEY_LEN;
	wire_t *wire = init_wire((char *)session_key, &len);
	encrypt_wire(wire, shared_secret);
	if (xsend(socket, wire, len, 0) < 0) {
		return -1;
	}

	free(wire);
	return 0;
}
