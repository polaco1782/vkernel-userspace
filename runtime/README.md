# Userspace Runtime Migration

This tree is the landing zone for the `musl-vkernel` and `libc++` migration.

Current status:
- The kernel exposes a raw `vk_syscall(...)` ABI in `include/vkernel/vk.h`.
- The process loader rejects `PT_INTERP` and records ELF/TLS metadata for userspace startup.
- `userspace/libc/crt0.c` now synthesizes the `argv`/`envp`/`auxv` block expected by musl `__libc_start_main`.
- `userspace/libc/runtime_glue.c` is the always-linked runtime object that provides musl's raw syscall bridge and the shared `__dso_handle` used by libc++ destructors.
- `userspace/runtime_config.mk` provides the shared `USERSPACE_RUNTIME` selector used by the current migration targets.
- `userspace/cxx_runtime_config.mk` now centralizes the libc++ selection surface so migration targets do not hardcode runtime-local include or archive paths.
- `scripts/setup_userspace_runtime.sh` now stages the vendored musl headers and libc archive, and it also configures, builds, and installs `libc++abi.a`, `libc++.a`, and `libc++experimental.a` when `llvm-project` is vendored.
- `scripts/import_llvm_project.sh` bootstraps the vendored LLVM runtime source tree under `userspace/runtime/vendor/llvm-project`.
- The musl vendor tree now carries a first vkernel port layer for syscall transport, TLS/thread-pointer setup, and stat/mmap adaptation.
- The old in-tree `userspace/cpp` shim runtime has been removed; libc++ is now the only supported C++ runtime path.
- The C/C++ smoke binaries plus `cppcompat`, `vkobj`, `shell`, `vkgui`, `doom`, `vnes`, `vspcplay`, `snes9x`, `clownmdemu`, `MODPlay`, and `minimp3` now build under `USERSPACE_RUNTIME=musl`.
- `cppcompat`, `tls_smoke`, `vkobj`, `shell`, `vkgui`, `vnes`, `vspcplay`, and `snes9x` now also link successfully with `USERSPACE_CXX_RUNTIME=libcxx`.
- The remaining app makefiles no longer hardwire `../sysroot`; they follow the runtime selector and run the ELF guard that rejects interpreter-linked outputs.

Planned layout:
- `vendor/musl`
- `vendor/llvm-project`
- `patches/musl`
- `patches/libcxx`
- `patches/libcxxabi`
- `build`
- `sysroot`

Current selector values:
- `USERSPACE_RUNTIME=musl` uses `userspace/runtime/sysroot` for headers, libc archives, and crt objects
- `USERSPACE_CXX_RUNTIME=libcxx` now points at `userspace/runtime/sysroot/include/c++/v1` and links against the vendored `libc++abi.a` and `libc++.a` staged into the same sysroot
