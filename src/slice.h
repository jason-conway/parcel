/**
 * @file slice.h
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Opaque data structure for a contiguous sequence of bytes
 * @version 0.1
 * @date 2024-10-13
 *
 * @copyright Copyright (c) 2022 - 2024 Jason Conway. All rights reserved.
 *
 */

#pragma once

#include "xplatform.h"

/**
 * @brief Static slice
 *  Backed by a statically allocated array of bytes
 *  Fixed capacity
 *  
 * @brief Dynamic slice
 *  Handles it's own memory allocation
 *  Variable capacity
 */


typedef struct slice_t {
    uint8_t *data;
    size_t cap;
    size_t len;
    bool dynamic;
} slice_t;

bool slice_append(slice_t *s, void *data, size_t len);

#define STATIC_SLICE(mem)   { (mem), sizeof(mem), 0, false }
#define DYNAMIC_SLICE()     { NULL, 0, 0, true }


#define SLICE_APPEND_CONST_STR(s, str) slice_append(s, str, sizeof(str) - 1)

// `str` must be a null terminated string
#define SLICE_APPEND_STR(s, str) slice_append(s, str, strlen(str))

#define SLICE_APPEND_U8(s, c) slice_append(s, (uint8_t []){(c)}, 1)
