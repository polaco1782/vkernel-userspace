#ifndef VK_NEWLIB_MSVC_COMPAT_H
#define VK_NEWLIB_MSVC_COMPAT_H

#include <stddef.h>

#if defined(_MSC_VER)

#ifdef __packed
#undef __packed
#endif
#define __packed

#endif

#if defined(_MSC_VER) && !defined(__clang__)

#ifndef __attribute__
#define __attribute__(x)
#endif

#ifndef __builtin_expect
#define __builtin_expect(x, y) (x)
#endif

#ifndef __builtin_unreachable
#define __builtin_unreachable() __assume(0)
#endif

static __inline int vk_newlib_builtin_mul_overflow_size(size_t left,
                                                        size_t right,
                                                        size_t* out)
{
    if (!out) {
        return 1;
    }

    if (left != 0 && right > (((size_t)-1) / left)) {
        return 1;
    }

    *out = left * right;
    return 0;
}

#ifndef __builtin_mul_overflow
#define __builtin_mul_overflow(a, b, out) \
    vk_newlib_builtin_mul_overflow_size((size_t)(a), (size_t)(b), (size_t*)(out))
#endif

#endif

#endif /* VK_NEWLIB_MSVC_COMPAT_H */
