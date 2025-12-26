#include "wire.h"


// Pack byte array into 64-bit word
uint64_t wire_pack64(const uint8_t *src)
{
    return ((uint64_t)src[0] << 0x00) |
           ((uint64_t)src[1] << 0x08) |
           ((uint64_t)src[2] << 0x10) |
           ((uint64_t)src[3] << 0x18) |
           ((uint64_t)src[4] << 0x20) |
           ((uint64_t)src[5] << 0x28) |
           ((uint64_t)src[6] << 0x30) |
           ((uint64_t)src[7] << 0x38);
}

// Pack byte array into 32-bit word
uint32_t wire_pack32(const uint8_t *src)
{
    return ((uint32_t)src[0] << 0x00) |
           ((uint32_t)src[1] << 0x08) |
           ((uint32_t)src[2] << 0x10) |
           ((uint32_t)src[3] << 0x18);
}

// Pack byte array into 16-bit word
uint16_t wire_pack16(const uint8_t *src)
{
    return ((uint16_t)src[0] << 0x00) |
           ((uint16_t)src[1] << 0x08);
}

// Pack byte array into 8-bit "word"
uint8_t wire_pack8(const uint8_t *src)
{
    return src[0];
}

// Unpack a 64-bit word into a little-endian array of bytes
void wire_unpack64(uint8_t *dst, uint64_t src)
{
    dst[0] = (uint8_t)(src >> 0x00);
    dst[1] = (uint8_t)(src >> 0x08);
    dst[2] = (uint8_t)(src >> 0x10);
    dst[3] = (uint8_t)(src >> 0x18);
    dst[4] = (uint8_t)(src >> 0x20);
    dst[5] = (uint8_t)(src >> 0x28);
    dst[6] = (uint8_t)(src >> 0x30);
    dst[7] = (uint8_t)(src >> 0x38);
}

// Unpack a 32-bit word into a little-endian array of bytes
void wire_unpack32(uint8_t *dst, uint32_t src)
{
    dst[0] = (uint8_t)(src >> 0x00);
    dst[1] = (uint8_t)(src >> 0x08);
    dst[2] = (uint8_t)(src >> 0x10);
    dst[3] = (uint8_t)(src >> 0x18);
}

// Unpack a 16-bit word into a little-endian array of bytes
void wire_unpack16(uint8_t *dst, uint16_t src)
{
    dst[0] = (uint8_t)(src >> 0x00);
    dst[1] = (uint8_t)(src >> 0x08);
}

// "Unpack" an 8-bit "word" into a little-endian array of bytes
void wire_unpack8(uint8_t *dst, uint8_t src)
{
    dst[0] = src;
}
