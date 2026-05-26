/*
 * vkernel userspace - C runtime startup
 * Copyright (C) 2026 vkernel authors
 *
 * crt0.c - Entry point that bridges the vkernel ABI to standard main().
 *
 * The kernel calls _start(const vk_api_t* api).  We store the API
 * pointer, perform minimal C runtime initialization (newlib init_array),
 * then call the user's main().
 */

#include "../include/vk.h"

#include "libc_compiler.h"

/* Provided by the user program */
extern int main(int argc, char** argv);

/*
 * newlib constructor / destructor arrays.
 * __libc_init_array walks .preinit_array, .init, .init_array.
 * __libc_fini_array walks .fini_array, .fini.
 * These are defined in newlib's libc/misc/init.c and fini.c.
 *
 * When building against a bare-metal newlib (x86_64-elf) that does not
 * ship crtbegin/crtend objects, these symbols may be absent from libc.a.
 * We provide our own implementations that walk the ELF sections directly.
 */

typedef void (*_func_ptr)(void);

#if defined(_MSC_VER)

/*
 * MSVC stores startup hooks in .CRT$XI* (C initializers) and .CRT$XC*
 * (C++ initializers). We provide our own sentinels because vkernel
 * userspace binaries do not link the hosted CRT.
 */
#pragma section(".CRT$XIA", long, read)
#pragma section(".CRT$XIZ", long, read)
#pragma section(".CRT$XCA", long, read)
#pragma section(".CRT$XCZ", long, read)
#pragma section(".CRT$XPA", long, read)
#pragma section(".CRT$XPZ", long, read)
#pragma section(".CRT$XTA", long, read)
#pragma section(".CRT$XTZ", long, read)

__declspec(allocate(".CRT$XIA")) _func_ptr __xi_a[] = { 0 };
__declspec(allocate(".CRT$XIZ")) _func_ptr __xi_z[] = { 0 };
__declspec(allocate(".CRT$XCA")) _func_ptr __xc_a[] = { 0 };
__declspec(allocate(".CRT$XCZ")) _func_ptr __xc_z[] = { 0 };
__declspec(allocate(".CRT$XPA")) _func_ptr __xp_a[] = { 0 };
__declspec(allocate(".CRT$XPZ")) _func_ptr __xp_z[] = { 0 };
__declspec(allocate(".CRT$XTA")) _func_ptr __xt_a[] = { 0 };
__declspec(allocate(".CRT$XTZ")) _func_ptr __xt_z[] = { 0 };

#else

extern _func_ptr __preinit_array_start[] __attribute__((weak));
extern _func_ptr __preinit_array_end[]   __attribute__((weak));
extern _func_ptr __init_array_start[]    __attribute__((weak));
extern _func_ptr __init_array_end[]      __attribute__((weak));
extern _func_ptr __fini_array_start[]    __attribute__((weak));
extern _func_ptr __fini_array_end[]      __attribute__((weak));

#endif

#define VK_CMDLINE_MAX 256
#define VK_ARGV_MAX    32

static int is_ascii_space(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

static int parse_argv(char* cmdline, char** argv, int max_args)
{
    int argc = 0;
    char* read = cmdline;

    while (*read != '\0' && argc + 1 < max_args) {
        while (is_ascii_space(*read))
            ++read;
        if (*read == '\0')
            break;

        char* token = read;
        char* write = read;
        char quote = '\0';

        while (*read != '\0') {
            char ch = *read;

            if (quote != '\0') {
                if (ch == quote) {
                    quote = '\0';
                    ++read;
                    continue;
                }

                if (ch == '\\' && read[1] == quote) {
                    *write++ = quote;
                    read += 2;
                    continue;
                }

                *write++ = *read++;
                continue;
            }

            if (ch == '"' || ch == '\'') {
                quote = ch;
                ++read;
                continue;
            }

            if (is_ascii_space(ch))
                break;

            *write++ = *read++;
        }

        /* Preserve the next token start before we overwrite the delimiter. */
        while (is_ascii_space(*read))
            ++read;

        *write = '\0';
        argv[argc++] = token;
    }

    argv[argc] = (char*)0;
    return argc;
}

void __libc_init_array(void)
{
#if defined(_MSC_VER)
    for (_func_ptr *f = __xi_a + 1; f < __xi_z; ++f) {
        if (*f != (_func_ptr)0)
            (*f)();
    }
    for (_func_ptr *f = __xc_a + 1; f < __xc_z; ++f) {
        if (*f != (_func_ptr)0)
            (*f)();
    }
#else
    if (__preinit_array_start && __preinit_array_end
            && __preinit_array_end > __preinit_array_start) {
        for (_func_ptr *f = __preinit_array_start; f < __preinit_array_end; ++f)
            (*f)();
    }
    if (__init_array_start && __init_array_end
            && __init_array_end > __init_array_start) {
        for (_func_ptr *f = __init_array_start; f < __init_array_end; ++f)
            (*f)();
    }
#endif
}

void __libc_fini_array(void)
{
#if defined(_MSC_VER)
    for (_func_ptr *f = __xp_a + 1; f < __xp_z; ++f) {
        if (*f != (_func_ptr)0)
            (*f)();
    }
    for (_func_ptr *f = __xt_a + 1; f < __xt_z; ++f) {
        if (*f != (_func_ptr)0)
            (*f)();
    }
#else
    if (__fini_array_start && __fini_array_end
            && __fini_array_end > __fini_array_start) {
        for (_func_ptr *f = __fini_array_end - 1; f >= __fini_array_start; --f)
            (*f)();
    }
#endif
}

/*
 * _start — true entry point for every vkernel userspace binary.
 *
 * The kernel passes the API table pointer in the first argument
 * register (RDI on System V, RCX on MSVC x64).
 */
int _start(const vk_api_t* api)
{
    char cmdline[VK_CMDLINE_MAX] = {0};
    char* argv[VK_ARGV_MAX] = {0};
    int argc = 0;

    /* 1. Store the kernel API pointer for all translation units. */
    _vk_api_ptr = api;

    /* 2. Run global constructors (C++ static init, newlib internals). */
    __libc_init_array();

    /* 3. Build argc/argv from the command line provided by the kernel. */
    if (api != (const vk_api_t*)0 && api->vk_get_cmdline != 0) {
        api->vk_get_cmdline(cmdline, (vk_usize)sizeof(cmdline));
        argc = parse_argv(cmdline, argv, VK_ARGV_MAX);
    }

    int ret = main(argc, argv);

    /* 4. Run global destructors. */
    __libc_fini_array();

    /* 5. Terminate the process via the kernel. */
    VK_CALL(exit, ret);

    /* Never reached — silence compiler warnings. */
    return ret;
}
