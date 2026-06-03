# Userspace Runtime Migration

This tree is the landing zone for the `musl-vkernel` and `libc++` migration.

Current status:
- The kernel exposes a raw `vk_syscall(...)` ABI in `include/vkernel/vk.h`.
- The process loader rejects `PT_INTERP` and records ELF/TLS metadata for userspace startup.
- `userspace/libc/crt0.c` can synthesize `argv`/`envp`/`auxv` for a future musl `__libc_start_main` path while still falling back to the current newlib flow.
- `userspace/libc/syscalls.c` now routes file, time, and memory-adjacent calls through the raw syscall ABI instead of rebuilding a separate fd abstraction.

Planned layout:
- `vendor/musl`
- `vendor/llvm-project`
- `patches/musl`
- `patches/libcxx`
- `patches/libcxxabi`
- `build`
- `sysroot`

Until those vendor sources are added, the existing app makefiles still use `userspace/sysroot` and the transitional newlib-based runtime.
