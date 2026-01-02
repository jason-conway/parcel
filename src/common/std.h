#pragma once

#ifndef __has_builtin
    #define __has_builtin(x)
#endif

#ifndef __has_attribute
    #define __has_attribute(x)
#endif

#ifndef unlikely
    #if __has_builtin(__builtin_expect)
        #define unlikely(x) __builtin_expect(!!(x), 0)
    #else
        #define unlikely(x)
    #endif
#endif

#ifndef likely
    #if __has_builtin(__builtin_expect)
        #define likely(x) __builtin_expect(!!(x), 1)
    #else
        #define likely(x)
    #endif
#endif

#ifndef trap
    #if __has_builtin(__builtin_verbose_trap)
        #define trap(s) __builtin_verbose_trap(s, s)
    #elif __has_builtin(__builtin_trap)
        #define trap(s) __builtin_trap()
    #else
        #define trap(s)
    #endif
#endif

#ifndef assume
    #if __has_builtin(__builtin_assume)
        #define assume(x) __builtin_assume(!!(x))
    #else
        #define assume(x) \
            do { \
                if (unlikely(!(x))) { \
                    __builtin_unreachable(); \
                } \
            } while (0)
    #endif
#endif

#ifndef __nonstring
    #if __has_attribute(nonstring)
        #define __nonstring __attribute__((nonstring))
    #else
        #define __nonstring
    #endif
#endif

#ifndef __packed
    #if __has_attribute(packed)
        #define __packed __attribute__((packed))
    #else
        #define __packed
    #endif
#endif

#ifndef STR
    #define _STR(...) #__VA_ARGS__
    #define STR(...) _STR(__VA_ARGS__)
#endif

#ifndef round_up
    #ifndef round_down
        #define round_down(n, d) ((n) & -(0 ? (n) : (d)))
    #endif
    #define round_up(n, d) round_down((n) + (d) - 1, (d))
#endif

#ifndef lengthof
    #ifndef countof
        #define countof(a)   (sizeof(a) / sizeof(*(a)))
    #endif
    #define lengthof(s)  (countof(s) - 1)
#endif

#ifndef pointer_offset
    #define pointer_offset(ptr, ofs)    (void *)((char *)(ptr) + (ptrdiff_t)(ofs))
#endif

#ifndef pointer_diff
    #define pointer_diff(first, second) (ptrdiff_t)((uint8_t *)(first) - (uint8_t *)(second))
#endif
