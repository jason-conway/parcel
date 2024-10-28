/**
 * @file s8.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Arena based slice handling
 * @version 0.1
 * @date 2024-10-24
 * 
 * @copyright Copyright (c) 2022 - 2024 Jason Conway. All rights reserved.
 * 
 */

#pragma once

#include "xplatform.h"

#define alignof(x)   _Alignof(x)
#define countof(a)   (sizeof(a) / sizeof(*(a)))
#define lengthof(s)  (countof(s) - 1)
#define alloc(a, t, n) __alloc(a, sizeof(t), alignof(t), n)

typedef struct s8_t {
    uint8_t *data;
    ptrdiff_t len;
} s8_t;

typedef struct arena_t {
    char *beg;
    char *end;
} arena_t;

typedef struct output_t {
    uint8_t *data;
    ptrdiff_t len;
    ptrdiff_t cap;
    bool err;
} output_t;

typedef struct status_t {
    s8_t out;
    s8_t err;
    int32_t status;
} status_t;

#define S(s) (s8_t) { (uint8_t *)s, lengthof(s) }

bool s8_equal(s8_t a, s8_t b);
