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

	sha256_digest(wire->data, key);
	// memcpy(key, wire->data, KEY_LEN);
	free(wire);
	return 0;
}

int two_party_server(const sock_t socket, const uint8_t *session_key)
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

static int send_ctrl_key(sock_t server, fd_set *connections, size_t count, uint8_t *key)
{
	for (size_t i = 0; i < count + 1; i++) {
		sock_t fd; 
		if ((fd = xfd_inset(connections, i))) {
			if (fd != server) { 
				if (xsend(fd, key, KEY_LEN, 0) < 0) {
					return -1;
				}
			}
		}
	}
	return 0;
}

static int from_left_node(sock_t server, fd_set *connections, size_t max_in_set, uint8_t *intermediate_values, size_t *active)
{
	fd_set read_fds;
	FD_ZERO(&read_fds);

	size_t recv_count = 0;
	do {
		read_fds = *connections;
		if (select(max_in_set + 1, &read_fds, NULL, NULL, NULL) < 0) {
			return -1;
		}
		for (size_t i = 0; i < max_in_set + 1; i++) {
			sock_t fd;
			if ((fd = xfd_isset(connections, &read_fds, i))) {
				if (fd != server) {
					if (xrecv(fd, &intermediate_values[32 * recv_count], 32, 0) <= 0) {
						FD_CLR(fd, connections);
						(void)xclose(fd);
						(*active)--;
					}
					else {
						recv_count++;
					}

				}
			}
		}
	} while (recv_count < (*active));
	return 0;
}

static int to_right_node(sock_t server, fd_set *connections, size_t max_in_set, uint8_t *intermediate_values)
{
	for (size_t i = 0; i < max_in_set; i++) {
		sock_t fd; 
		if ((fd = xfd_inset(connections, (i + 1) % max_in_set))) {
			if (fd != server) { 
				if (xsend(fd, &intermediate_values[32 * i], 32, 0) < 0) {
					return -1;
				}
			}
		}
	}
	return 0;
}

int key_exchange_router(sock_t server, fd_set *connections, size_t max_in_set, size_t *active, uint8_t *key)
{
	uint8_t *intermediate_values = malloc((*active) * 32);
	if (!intermediate_values) {
		return -1;
	}
	memset(intermediate_values, 0, sizeof(*intermediate_values));
	
	// "Ratchet" hash forward once
	sha256_self_digest(key);
	printf("Sending control key (start)\n");
	if (send_ctrl_key(server, connections, max_in_set, key)) {
		return -1;
	}
	sha256_self_digest(key);
	printf("Server-Side Key-Exchange\n");
	printf("Active connections: %zu\n", *active);

	for (size_t i = 0; i < (*active); i++) {
		printf("Round: %zu\n", i);

		from_left_node(server, connections, max_in_set, intermediate_values, active);
		to_right_node(server, connections, max_in_set, intermediate_values);
	}
	
	printf("Sending control key (end)\n");
	send_ctrl_key(server, connections, max_in_set, key);
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

	uint8_t intermediate[KEY_LEN];
	while (1) {
		if (xrecv(socket, intermediate, KEY_LEN, 0) != KEY_LEN) {
			return -1;
		}
		// CTRL message received
		if (memcmp(intermediate, ctrl_key, 32)) {
			break;
		}
		
		uint8_t intermediate_shared[KEY_LEN];
		compute_shared(intermediate_shared, secret_key, intermediate);

		if (xsend(socket, intermediate_shared, KEY_LEN, 0) != KEY_LEN) {
			return -1;
		}
	}
	
	uint8_t shared_key[KEY_LEN];
	compute_shared(shared_key, secret_key, intermediate);
	sha256_digest(session_key, shared_key);
	sha256_self_digest(ctrl_key);
	return 0;
}
