#ifndef VK_USERSPACE_LIBC_COMPILER_H
#define VK_USERSPACE_LIBC_COMPILER_H

#if defined(_MSC_VER)
#define VK_NORETURN __declspec(noreturn)
#define VK_UNREACHABLE() __assume(0)
#else
#define VK_NORETURN __attribute__((noreturn))
#define VK_UNREACHABLE() __builtin_unreachable()
#endif

#endif /* VK_USERSPACE_LIBC_COMPILER_H */
