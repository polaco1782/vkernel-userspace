/*
 * vkernel userspace - C runtime startup
 * Copyright (C) 2026 vkernel authors
 *
 * crt0.c - Entry point that bridges the vkernel ABI to libc startup.
 *
 * The kernel calls _start(const vk_api_t* api).  We store the API
 * pointer, build a musl-style argc/argv/envp/auxv block, and then
 * hand control to libc startup.
 */

#include "../include/vk.h"

/* Provided by the runtime glue. C++ builds override it with a bridge object. */
extern int __vkernel_call_main(int argc, char** argv) __attribute__((weak));

/*
 * Constructor / destructor array fallbacks for the direct-entry path.
 * When libc startup is unavailable we still walk the ELF sections directly.
 */

typedef void (*_func_ptr)(void);

extern _func_ptr __preinit_array_start[] __attribute__((weak));
extern _func_ptr __preinit_array_end[]   __attribute__((weak));
extern _func_ptr __init_array_start[]    __attribute__((weak));
extern _func_ptr __init_array_end[]      __attribute__((weak));
extern _func_ptr __fini_array_start[]    __attribute__((weak));
extern _func_ptr __fini_array_end[]      __attribute__((weak));
extern void __cxa_finalize(void*) __attribute__((weak));

#define VK_CMDLINE_MAX 256
#define VK_ARGV_MAX    32
#define VK_ENVP_MAX    1
#define VK_AUX_PAIRS   12

enum {
    VK_AT_NULL   = 0,
    VK_AT_PHDR   = 3,
    VK_AT_PHENT  = 4,
    VK_AT_PHNUM  = 5,
    VK_AT_PAGESZ = 6,
    VK_AT_ENTRY  = 9,
    VK_AT_UID    = 11,
    VK_AT_EUID   = 12,
    VK_AT_GID    = 13,
    VK_AT_EGID   = 14,
    VK_AT_HWCAP  = 16,
    VK_AT_SECURE = 23,
    VK_AT_RANDOM = 25,
    VK_AT_EXECFN = 31,
};

extern void _init(void) __attribute__((weak));
extern void _fini(void) __attribute__((weak));
extern int __libc_start_main(int (*main_fn)(),
                             int argc,
                             char** argv,
                             void (*init_fn)(void),
                             void (*fini_fn)(void),
                             void (*ldso_fn)(void)) __attribute__((weak));

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

static void seed_aux_random(unsigned char out[16], const vk_process_image_info_t* info)
{
    vk_u64 seed = 0x9E3779B97F4A7C15ULL;

    if (vk_get_api() != (const vk_api_t*)0 && vk_get_api()->vk_tick_count != 0) {
        seed ^= vk_get_api()->vk_tick_count();
    }
    if (info != (const vk_process_image_info_t*)0) {
        seed ^= info->entry;
        seed ^= info->phdr_addr << 1;
        seed ^= info->tls_memsz << 7;
    }
    seed ^= (vk_u64)(vk_usize)out;

    for (int i = 0; i < 16; ++i) {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        out[i] = (unsigned char)(seed >> ((i & 7) * 8));
    }
}

static int call_main_fallback(int argc, char** argv)
{
    if (__vkernel_call_main != 0) {
        return __vkernel_call_main(argc, argv);
    }
    return 0;
}

void __libc_init_array(void)
{
    if (__preinit_array_start) {
        for (_func_ptr *f = __preinit_array_start; f < __preinit_array_end; ++f)
            (*f)();
    }
    if (__init_array_start) {
        for (_func_ptr *f = __init_array_start; f < __init_array_end; ++f)
            (*f)();
    }
}

void __libc_fini_array(void)
{
    if (__cxa_finalize) {
        __cxa_finalize((void*)0);
    }
    if (__fini_array_start) {
        for (_func_ptr *f = __fini_array_end - 1; f >= __fini_array_start; --f)
            (*f)();
    }
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
    char* env_literals[VK_ENVP_MAX] = {0};
    char* bootstrap_block[VK_ARGV_MAX + 1 + VK_ENVP_MAX + 1 + VK_AUX_PAIRS * 2 + 2] = {0};
    unsigned char aux_random[16] = {0};
    vk_process_image_info_t image_info = {0};
    int argc = 0;

    /* 1. Store the kernel API pointer for all translation units. */
    _vk_api_ptr = api;

    /* 2. Build argc/argv from the command line provided by the kernel. */
    if (api != (const vk_api_t*)0 && api->vk_get_cmdline != 0) {
        api->vk_get_cmdline(cmdline, (vk_usize)sizeof(cmdline));
        argc = parse_argv(cmdline, argv, VK_ARGV_MAX);
    }

    if (argc == 0) {
        argv[0] = (char*)"";
        argv[1] = (char*)0;
    }

    if (api != (const vk_api_t*)0) {
        (void)vk_syscall(VK_SYS_PROCESS_IMAGE_INFO,
                         (vk_u64)(vk_usize)&image_info,
                         0,
                         0,
                         0,
                         0,
                         0);
    }

    seed_aux_random(aux_random, &image_info);

    if (__libc_start_main != 0) {
        char** runtime_argv = bootstrap_block;
        char** runtime_envp = runtime_argv + argc + 1;
        vk_u64* auxv = (vk_u64*)(runtime_envp + VK_ENVP_MAX + 1);
        int aux_index = 0;

        for (int i = 0; i < argc; ++i) {
            runtime_argv[i] = argv[i];
        }
        runtime_argv[argc] = (char*)0;

        runtime_envp[0] = env_literals[0];
        runtime_envp[1] = (char*)0;

        auxv[aux_index++] = VK_AT_PAGESZ;
        auxv[aux_index++] = image_info.page_size != 0 ? image_info.page_size : 4096;
        auxv[aux_index++] = VK_AT_ENTRY;
        auxv[aux_index++] = image_info.entry;
        auxv[aux_index++] = VK_AT_PHDR;
        auxv[aux_index++] = image_info.phdr_addr;
        auxv[aux_index++] = VK_AT_PHENT;
        auxv[aux_index++] = image_info.phent;
        auxv[aux_index++] = VK_AT_PHNUM;
        auxv[aux_index++] = image_info.phnum;
        auxv[aux_index++] = VK_AT_UID;
        auxv[aux_index++] = 0;
        auxv[aux_index++] = VK_AT_EUID;
        auxv[aux_index++] = 0;
        auxv[aux_index++] = VK_AT_GID;
        auxv[aux_index++] = 0;
        auxv[aux_index++] = VK_AT_EGID;
        auxv[aux_index++] = 0;
        auxv[aux_index++] = VK_AT_HWCAP;
        auxv[aux_index++] = 0;
        auxv[aux_index++] = VK_AT_SECURE;
        auxv[aux_index++] = 0;
        auxv[aux_index++] = VK_AT_RANDOM;
        auxv[aux_index++] = (vk_u64)(vk_usize)aux_random;
        auxv[aux_index++] = VK_AT_EXECFN;
        auxv[aux_index++] = (vk_u64)(vk_usize)(argc > 0 ? argv[0] : (char*)"");
        auxv[aux_index++] = VK_AT_NULL;
        auxv[aux_index++] = 0;

        return __libc_start_main((int (*)())__vkernel_call_main, argc, runtime_argv, _init, _fini, 0);
    }

    /* 3. Fallback path for freestanding bring-up and debug builds. */
    __libc_init_array();

    int ret = call_main_fallback(argc, argv);

    /* 4. Run global destructors. */
    __libc_fini_array();

    /* 5. Terminate the process via the kernel. */
    VK_CALL(exit, ret);

    /* Never reached — silence compiler warnings. */
    return ret;
}
