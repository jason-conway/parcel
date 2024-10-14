/**
 * @file slice.c
 * @author Jason Conway (jpc@jasonconway.dev)
 * @brief Opaque data structure for a contiguous sequence of bytes
 * @version 0.1
 * @date 2024-10-13
 *
 * @copyright Copyright (c) 2022 - 2024 Jason Conway. All rights reserved.
 *
 */

#include "slice.h"

static bool slice_append_static(slice_t *s, void *data, size_t len)
{
    if (s->cap < s->len + len) {
        return false;
    }
    memcpy(&s->data[s->len], data, len);
    s->len += len;
    return true;
}

static bool slice_append_dynamic(slice_t *s, void *data, size_t len)
{
    if (s->cap < s->len + len) {
        size_t cap = s->cap ? 2 * s->cap : 16;
        while (cap < s->len + len) {
            cap *= 2;
        }
        s->cap = cap;
        s->data = xrealloc(s->data, cap);
        if (!s->data) {
            return false;
        }
    }
    memcpy(&s->data[s->len], data, len);
    s->len += len;
    return true;
}

bool slice_append(slice_t *s, void *data, size_t len)
{
    if (s->dynamic) {
        return slice_append_dynamic(s, data, len);
    }
    return slice_append_static(s, data, len);
}
