#pragma once

#include "xplatform.h"
#include "xutils.h"
#include "wire-util.h"
#include "log.h"
#include <stdint.h>

typedef struct wire_t wire_t;

typedef struct cable_header_t {
    uint8_t signature[6]; // "parcel"
    uint8_t len[8];
} __attribute__((packed)) cable_header_t;

typedef struct cable_t {
    cable_header_t hdr;
    uint8_t data[];
} __attribute__((packed)) cable_t;

// Allocate an empty cable, only large enough to receive the cable header
cable_t *alloc_cable(void);

// Create a new cable containing the provided `*len`-byte `wire`
// `wire` should be encrypted prior to calling this function
cable_t *init_cable(wire_t *wire, size_t *len);

// Return the length of the variably-sized cable payload
size_t cable_get_payload_len(cable_t *cable);

// Return the total length of the `cable`, including headers
size_t cable_get_total_len(cable_t *cable);

// Receive the remainding "payload" section of a cable from the provided socket
// `*cable` should point to received but unverified cable header
// Returns the total cable length or `0` on failure
size_t cable_recv_data(sock_t sock, cable_t **cable);


// Called after receiving a complete cable. Returns a pointer to the wire 
// encapsulated by the cable. Sets `len` to the *total* length of the returned
// wire
wire_t *get_cabled_wire(cable_t *cable, size_t *len);

// Free a wire returned from `get_cabled_wire(...)`
// [note] the cable containing `wire` gets freed
void free_cabled_wire(wire_t *wire);

// Receive a cable from the provided socket, setting `cable_length` 
// Returns `NULL` on failure
cable_t *recv_cable(sock_t sock, size_t *cable_length);

// Encrypts `wire` using the provided `key`, encapsulates the encrypted wire in
// a cable, and transmits the cable to provided socket `sock`
bool transmit_cabled_wire(sock_t sock, wire_t *wire, const uint8_t *key);
