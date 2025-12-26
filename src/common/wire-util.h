#pragma once

#include <stdint.h>

uint8_t wire_pack8(const uint8_t *src);
uint16_t wire_pack16(const uint8_t *src);
uint32_t wire_pack32(const uint8_t *src);
uint64_t wire_pack64(const uint8_t *src);

void wire_unpack64(uint8_t *dst, uint64_t src);
void wire_unpack32(uint8_t *dst, uint32_t src);
void wire_unpack16(uint8_t *dst, uint16_t src);
void wire_unpack8(uint8_t *dst, uint8_t src);
