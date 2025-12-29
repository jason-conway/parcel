#include "cable.h"
#include "log.h"
#include "xplatform.h"
#include "wire.h"
#include "xutils.h"
#include <stddef.h>


cable_t *alloc_cable(void)
{
    return xcalloc(sizeof(cable_header_t));
}

void cable_set_signature(cable_t *cable)
{
    memcpy(cable->hdr.signature, "parcel", sizeof(cable->hdr.signature));
}

void cable_set_len(cable_t *cable, size_t len)
{
    wire_unpack64(cable->hdr.len, len);
}

void cable_set_data(cable_t *cable, const void *data, size_t len)
{
    memcpy(&cable->data, data, len);
}

size_t cable_get_total_len(cable_t *cable)
{
    return wire_pack64(cable->hdr.len);
}

size_t cable_get_payload_len(cable_t *cable)
{
    return cable_get_total_len(cable) - sizeof(cable_t);
}

static bool cable_check_signature(cable_t *cable)
{
    return !memcmp(cable->hdr.signature, "parcel", sizeof(cable->hdr.signature));
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
    if (!cable_check_signature(*cable)) {
        log_error("cable signature is invalid");
        return 0;
    }
    if (!cable_recv_remaining(sock, cable)) {
        size_t exp = cable_get_payload_len(*cable);
        log_error("failed to receive cable data (%zu bytes)", exp);
        return 0;
    }

    size_t payload_len = cable_get_payload_len(*cable);
    size_t len = cable_get_total_len(*cable);
    log_trace("cable_recv_data() len: %zu bytes (payload: %zu bytes)", len, payload_len);
    return len;
}

bool transmit_cabled_wire(sock_t sock, wire_t *wire, const uint8_t *key)
{
    size_t len = wire_get_length(wire);
    encrypt_wire(wire, key);
    cable_t *cable = init_cable(wire, &len);
    bool ok = xsendall(sock, cable, len);
    xfree(cable);
    return ok;
}

static bool cable_recv_header(sock_t sock, cable_t **cable)
{
    return xrecvall(sock, &(*cable)->hdr, sizeof(cable_header_t));
}

cable_t *recv_cable(sock_t sock, size_t *cable_length)
{
    cable_t *cable = alloc_cable();
    if (!cable_recv_header(sock, &cable)) {
        log_error("failed to receive cable header ");
        goto err;
    }
    if (!cable_check_signature(cable)) {
        log_error("cable signature is invalid");
        goto err;
    }
    if (!cable_recv_remaining(sock, &cable)) {
        size_t exp = cable_get_payload_len(cable);
        log_error("failed to receive cable data (%zu bytes)", exp);
        goto err;
    }

    *cable_length = cable_get_total_len(cable);
    size_t payload_len = cable_get_payload_len(cable);
    log_trace("recv_cable() len: %zu bytes (payload: %zu bytes)", *cable_length, payload_len);

    if (0) {
err:
        cable = xfree(cable);
    }

    return cable;
}

cable_t *init_cable(wire_t *wire, size_t *len)
{
    const size_t cable_length = sizeof(cable_t) + *len;

    cable_t *cable = xcalloc(cable_length);

    cable_set_signature(cable);
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
