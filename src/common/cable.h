#pragma once

#include "xplatform.h"
#include "xutils.h"
#include "wire-util.h"
#include "log.h"
#include <stdint.h>

typedef struct wire_t wire_t;

typedef struct cable_header_t {
    uint8_t magic[6]; // ".cable"
    uint8_t len[8];
} __attribute__((packed)) cable_header_t;

typedef struct cable_t {
    cable_header_t hdr;
    uint8_t data[];
} __attribute__((packed)) cable_t;



cable_t *init_cable(wire_t *wire, size_t *len);

cable_t *new_cable(void);

bool cable_recv_header(sock_t sock, cable_t **cable);
size_t cable_get_payload_len(cable_t *cable);
size_t cable_get_total_len(cable_t *cable);
size_t cable_recv_data(sock_t sock, cable_t **cable);
void free_cabled_wire(wire_t *wire);
cable_t *recv_cable(sock_t sock, size_t *cable_length);
wire_t *get_cabled_wire(cable_t *cable, size_t *len);
bool transmit_cabled_wire(sock_t sock, const uint8_t *key, wire_t *wire);
void *cable_get_data(cable_t *cable);


