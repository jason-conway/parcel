/**
 * @file s8.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Arena based slice handling
 * @version 0.1
 * @date 2024-10-24
 * 
 * @copyright Copyright (c) 2022 - 2024 Jason Conway. All rights reserved.
 * 
 */

#include "s8.h"

void *__alloc(arena_t *a, ptrdiff_t objsize, ptrdiff_t align, ptrdiff_t count)
{
    ptrdiff_t avail = a->end - a->beg;
    ptrdiff_t padding = -(uintptr_t)a->beg & (align - 1);
    if (count > (avail - padding) / objsize) {
        exit(EXIT_FAILURE);
    }
    ptrdiff_t total = objsize * count;
    char *m = &a->beg[padding];
    a->beg += padding + total;
    for (ptrdiff_t i = 0; i < total; i++) {
        m[i] = 0;
    }
    return m;
}

arena_t alloc_local(arena_t *a)
{
    arena_t r = { 0 };
    ptrdiff_t cap = (a->end - a->beg) / 2;
    r.beg = alloc(a, char, cap);
    r.end = &r.beg[cap];
    return r;
}

s8_t s8_span(uint8_t *beg, uint8_t *end)
{
    return (s8_t) {
        .data = beg,
        .len = end - beg 
    };
}

bool s8_equal(s8_t a, s8_t b)
{
    if (a.len != b.len) {
        return false;
    }
    bool cmp = false;
    for (ptrdiff_t i = 0; i < a.len; i++) {
        cmp |= a.data[i] != b.data[i];
    }
    return !cmp;
}

uint64_t s8_hash(s8_t s)
{
    uint64_t h = 0x100;
    for (ptrdiff_t i = 0; i < s.len; i++) {
        h ^= s.data[i];
        h *= UINT64_C(1111111111111111111);
    }
    return h ^ h >> 32;
}

// Convert s8 ascii string to int32_t
// Return false on failure
bool s8_i32(int32_t *d, s8_t s)
{
    size_t i = 0;
    bool neg = false;
    uint32_t value = 0;
    uint32_t limit = 0x7fffffff;

    switch (*s.data) {
        case '-':
            i = 1;
            neg = true;
            limit = 0x80000000;
            break;
        case '+':
            i = 1;
            break;
    }

    for (; i < (size_t)s.len; i++) {
        int32_t d = s.data[i] - '0';
        if (value > (limit - d) / 10) {
            return false;
        }
        value = value * 10 + d;
    }

    *d = neg ? -value : value;
    return true;
}

static bool whitespace(uint8_t c)
{
    return c == '\t' || c == '\n' || c == '\r' || c == ' ';
}

s8_t s8_trim(s8_t s)
{
    if (!s.len) {
        return s;
    }
    uint8_t *c = s.data;
    uint8_t *e = &s.data[s.len];
    for (; c < e && whitespace(*c); c++);
    s.data = c;
    s.len = e - c;
    return s;
}

void s8_print(output_t *o, s8_t s)
{
    ptrdiff_t avail = o->cap - o->len;
    ptrdiff_t count = s.len < avail ? s.len : avail;
    uint8_t *dst = o->data + o->len;
    for (ptrdiff_t i = 0; i < count; i++) {
        dst[i] = s.data[i];
    }
    o->len += count;
    o->err |= count != s.len;
}
