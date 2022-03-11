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
	x25519(shared_key, secret_key, public_key);
}


/**
 * @brief Client-side DHKE with server
 *
 * @param socket
 * @param key
 * @param fingerprint
 * @return int
 */
int two_party_client(const sock_t socket, uint8_t *key, uint8_t *fingerprint)
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

	// sha256_digest(wire->data, key);
	memcpy(key, wire->data, KEY_LEN);
	free(wire);
	return 0;
}

int two_party_server(const sock_t socket, const uint8_t *session_key)
{
	printf("> Establishing new secure channel\n");
	uint8_t shared_secret[KEY_LEN];
	uint8_t server_public_key[KEY_LEN];

	// Receive public key from the client
	uint8_t public_key[KEY_LEN];
	if (xrecv(socket, public_key, KEY_LEN, 0) != KEY_LEN) {
		return -1;
	}
	printf("> Received client's public key\n");
	// Generate a single-use secret key for the key pair
	uint8_t secret_key[KEY_LEN];
	generate_secret(secret_key);
	printf("> Generated one-time secret key\n");
	// Compute a single-use public key and our shared secret with the client
	compute_public(secret_key, server_public_key, NULL);
	printf("> Computed one-time public key\n");
	if (xsend(socket, server_public_key, KEY_LEN, 0) < 0) {
		return -1;
	}
	printf("> Sent public key to client\n");

	compute_shared(shared_secret, secret_key, public_key);
	printf("> Computed shared-secret\n");
	
	size_t len = KEY_LEN;
	wire_t *wire = init_wire((char *)session_key, &len);
	printf("> Created new wire\n");
	encrypt_wire(wire, shared_secret);
	printf("> Encrypted wire\n");
	if (xsend(socket, wire, len, 0) < 0) {
		return -1;
	}
	printf("> Sent wire to client\n");
	free(wire);
	return 0;
}

static int send_ctrl_key(sock_t *sockets, size_t count, uint8_t *key)
{
	for (size_t i = 1; i < count; i++) {
		if (xsend(sockets[i], key, KEY_LEN, 0) < 0) {
			return -1;
		}
	}
	return 0;
}

static int rotate_intermediates(sock_t *sockets, size_t count)
{
	uint8_t intermediate_key[32];
	for (size_t i = 1; i <= count; i++) {
		size_t next = (i == count) ? 1 : i + 1;
		
		if (xrecv(sockets[i], intermediate_key, 32, 0) <= 0) {
			return -1;
		}
		printf("> xrecv from slot %zu", i);
		if (xsend(sockets[next], intermediate_key, 32, 0) < 0) {
			printf("\n> Error sending to slot %zu\n", next);
			return -1;
		}
		printf(" | xsend to slot %zu\n", next);
	}
	return 0;
}

int key_exchange_router(sock_t *sockets, size_t connection_count, uint8_t *key)
{
	if (connection_count < 2) {
		return 0;
	}

	printf("> Starting GDH1 sequence\n");

	// "Ratchet" hash forward once
	// sha256_self_digest(key);

	if (send_ctrl_key(sockets, connection_count, key)) {
		printf("> Error sending starting control keys\n");
		return -1;
	}

	printf("> Start messages sent\n");
	
	for (size_t i = 0; i < connection_count; i++) {
		printf("> GDH1 round %zu of %zu\n", i + 1, connection_count);
		rotate_intermediates(sockets, connection_count);
	}

	if (send_ctrl_key(sockets, connection_count, key)) {
		printf("> Error sending stopping control keys\n");
		return -1;
	}
	printf("> Stop messages sent\n");
	
	return 0;
}

// An N-Party Diffie-Hellman Key Exchange
int node_key_exchange(const sock_t socket, uint8_t *ctrl_key, uint8_t *session_key, uint8_t *fingerprint)
{
	uint8_t secret_key[KEY_LEN];
	generate_secret(secret_key);

	uint8_t public_key[KEY_LEN];
	compute_public(secret_key, public_key, fingerprint);

	// Send our public key to the client on our right
	if (xsend(socket, public_key, KEY_LEN, 0) != KEY_LEN) {
		return -1;
	}

	uint8_t intermediate_shared[KEY_LEN];
	while (1) {
		uint8_t intermediate[KEY_LEN];
		if (xrecv(socket, intermediate, KEY_LEN, 0) != KEY_LEN) {
			return -1;
		}
		// CTRL message received
		if (!memcmp(intermediate, ctrl_key, 32)) {
			break;
		}

		
		compute_shared(intermediate_shared, secret_key, intermediate);

		if (xsend(socket, intermediate_shared, KEY_LEN, 0) != KEY_LEN) {
			return -1;
		}
	}

	uint8_t shared_key[KEY_LEN];
	compute_shared(shared_key, secret_key, intermediate_shared);
	sha256_digest(session_key, shared_key);
	// sha256_self_digest(ctrl_key);
	return 0;
}