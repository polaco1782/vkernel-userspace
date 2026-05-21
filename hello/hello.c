/*
 * vkernel userspace - hello world
 * Copyright (C) 2026 vkernel authors
 *
 * hello.c - Minimal userspace program for vkernel (newlib C runtime).
 *
 * Build: see Makefile (Linux) or hello.vcxproj (Visual Studio).
 * Run:   vk> run hello.vbin
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/vk.h"       /* vkernel-specific APIs */

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    void *p[10] = {0};

    const vk_api_t* api = vk_get_api();

    printf("+---------------------------------+\n");
    printf("|   Hello from vkernel userspace! |\n");
    printf("+---------------------------------+\n");
    printf("\n");
    printf("  Kernel API version : %llu\n", (unsigned long long)api->api_version);
    printf("  Architecture       : x86-64\n");
    printf("  Runtime            : newlib\n");
    printf("\n");

    /* Test memory allocation */
    printf("  Allocating 128 bytes... ");

    for(int i = 0; i < 10; i++) {
        p[i] = malloc(1024*1024*1024); /* Allocate 1 MiB blocks */
        VK_CALL(sleep, 50); /* Sleep for 50 ticks (0.5 seconds) */

        if (p[i]) {
            printf("malloc OK at %p\n", p[i]);
            memset(p[i], 0xAB, 128);
            printf("  Freed.\n");
        } else {
            printf("FAILED\n");
        }
    }

    for(int i = 0; i < 10; i++) {
        if (p[i]) {
            free(p[i]);
        }
    }

    FILE *f = fopen("hello.vbin", "r");
    if (f) {
        printf("  fopen hello.vbin: success\n");
        fclose(f);
    } else {
        printf("  fopen hello.vbin: failed\n");
    }

    for (int i = 0; i < 10; i++) {
        printf("  Tick %d\n", i);
        VK_CALL(sleep, 100); /* Sleep for 100 ticks (1 second) */
    }

    printf("\nGoodbye!\n");
    return 0;
}
