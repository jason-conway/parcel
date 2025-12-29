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

typedef bool (*slice_append_fn_t)(slice_t *, const void *, size_t);

static bool slice_append_static(slice_t *s, const void *data, size_t len)
{
    if (s->cap < s->len + len) {
        return false;
    }
    memcpy(&s->data[s->len], data, len);
    s->len += len;
    return true;
}

static bool slice_append_dynamic(slice_t *s, const void *data, size_t len)
{
    if (s->cap < s->len + len) {
        size_t cap = s->cap ? 2 * s->cap : 16;
        while (cap < s->len + len) {
            cap *= 2;
        }
        s->cap = cap;
        s->data = xrealloc(s->data, cap);
    }
    memcpy(&s->data[s->len], data, len);
    s->len += len;
    return true;
}

bool slice_append(slice_t *s, const void *data, size_t len)
{
    slice_append_fn_t fns[] = { slice_append_static, slice_append_dynamic };
    return fns[s->dynamic](s, data, len);
}
