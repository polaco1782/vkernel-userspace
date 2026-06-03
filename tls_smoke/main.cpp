/*
 * vkernel userspace - TLS/runtime smoke test
 * Copyright (C) 2026 vkernel authors
 *
 * Exercises the loader/runtime pieces that must keep working as we
 * continue the musl + libc++ userspace migration.
 */

#include <stdio.h>

#include "../include/vk.h"

namespace {

int g_failures = 0;
int g_ctor_count = 0;
int g_relocated_value = 23;
int* g_relocated_ptr = &g_relocated_value;
thread_local int g_tls_value = 17;

void report(const char* label, bool ok)
{
    printf("  [%s] %s\n", ok ? "ok" : "fail", label);
    if (!ok) {
        ++g_failures;
    }
}

struct lifetime_probe {
    lifetime_probe()
    {
        ++g_ctor_count;
    }

    ~lifetime_probe()
    {
        puts("  [ok] global destructor");
    }
};

lifetime_probe g_lifetime_probe;

auto local_static_value() -> int&
{
    static int value = 31;
    return value;
}

} // namespace

int main(int /*argc*/, char** /*argv*/)
{
    printf("+-----------------------------------+\n");
    printf("|   vkernel TLS Runtime Smoke Test  |\n");
    printf("+-----------------------------------+\n\n");

    report("global constructor", g_ctor_count == 1);
    report("thread_local initial value", g_tls_value == 17);

    g_tls_value += 5;
    report("thread_local mutation", g_tls_value == 22);

    report("PIE relocation", g_relocated_ptr == &g_relocated_value && *g_relocated_ptr == 23);

    int& local_static = local_static_value();
    report("guarded local static init", local_static == 31);
    local_static += 1;
    report("guarded local static reuse", local_static_value() == 32);

    printf("\nSummary: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
