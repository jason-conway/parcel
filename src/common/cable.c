#include "cable.h"
#include "log.h"
#include "xplatform.h"
#include "wire.h"
#include "xutils.h"
#include <stddef.h>


cable_t *new_cable(void)
{
    return xcalloc(sizeof(cable_header_t));
}


void cable_set_magic(cable_t *cable)
{
    memcpy(cable->hdr.magic, ".cable", sizeof(cable->hdr.magic));
}

void cable_set_len(cable_t *cable, size_t len)
{
    wire_unpack64(cable->hdr.len, len);
}

void cable_set_data(cable_t *cable, const void *data, size_t len)
{
    memcpy(&cable->data, data, len);
}

void *cable_get_data(cable_t *cable)
{
    return cable->data;
}

size_t cable_get_total_len(cable_t *cable)
{
    return wire_pack64(cable->hdr.len);
}

size_t cable_get_payload_len(cable_t *cable)
{
    return cable_get_total_len(cable) - sizeof(cable_t);
}

bool cable_recv_header(sock_t sock, cable_t **cable)
{
    return xrecvall(sock, &(*cable)->hdr, sizeof(cable_header_t));
}

static bool cable_check_magic(cable_t *cable)
{
    return !memcmp(cable->hdr.magic, ".cable", sizeof(cable->hdr.magic));
}

static bool cable_recv_remaining(sock_t sock, cable_t **cable)
{
    size_t cable_size = cable_get_total_len(*cable);
    *cable = xrealloc(*cable, cable_size);
    size_t remaining = cable_size - sizeof(cable_header_t);
    return xrecvall(sock, (*cable)->data, remaining);
}

size_t cable_recv_data(sock_t sock, cable_t **cable)
{
    if (!cable_check_magic(*cable)) {
        log_error("invalid magic in cable header");
        return 0;
    }
    if (!cable_recv_remaining(sock, cable)) {
        log_error("failed to receive remainder of cable");
        return 0;
    }
    size_t len = cable_get_total_len(*cable);
    log_info("got cable: %zu bytes", len);
    return len;
}

bool transmit_cabled_wire(sock_t sock, const uint8_t *key, wire_t *wire)
{
    size_t len = wire_get_length(wire);
    log_info("transmitting cable containing %zu byte wire", len);
    encrypt_wire(wire, key);
    cable_t *cable = init_cable(wire, &len);
    log_info("cable size: %zu", len);
    xhexdump(cable, len);
    bool ok = xsendall(sock, cable, len);
    xfree(cable);
    return ok;
}


cable_t *recv_cable(sock_t sock, size_t *cable_length)
{
    cable_t *cable = new_cable();
    if (!cable_recv_header(sock, &cable)) {
        log_error("failed to receive cable header");
        return xfree(cable);
    }
    if (!cable_check_magic(cable)) {
        log_error("invalid header");
        return xfree(cable);
    }
    if (!cable_recv_remaining(sock, &cable)) {
        log_error("failed to receive remainder of cable");
        return xfree(cable);
    }
    *cable_length = cable_get_total_len(cable);
    log_info("got cable: %zu bytes", *cable_length);
    xhexdump(cable, *cable_length);
    return cable;
}

cable_t *init_cable(wire_t *wire, size_t *len)
{
    const size_t cable_length = sizeof(cable_t) + *len;

    cable_t *cable = xcalloc(cable_length);

    cable_set_magic(cable);
    cable_set_len(cable, cable_length);
    cable_set_data(cable, wire, *len);
    *len = cable_length;
    return cable;
}

void free_cabled_wire(wire_t *wire)
{
    xfree(pointer_offset(wire, -(ptrdiff_t)(sizeof(cable_header_t))));
}

wire_t *get_cabled_wire(cable_t *cable, size_t *len)
{
    *len = cable_get_payload_len(cable);
    return (wire_t *)cable->data;
}
