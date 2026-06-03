/*
 * vkernel userspace - C runtime smoke test
 * Copyright (C) 2026 vkernel authors
 *
 * hello.c - Focused libc/runtime smoke target for the musl userspace stack.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "../include/vk.h"

#ifndef PROT_READ
#define PROT_READ 0x1
#endif
#ifndef PROT_WRITE
#define PROT_WRITE 0x2
#endif
#ifndef MAP_PRIVATE
#define MAP_PRIVATE 0x02
#endif
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif
#ifndef MAP_FAILED
#define MAP_FAILED ((void*)-1)
#endif
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void* addr, size_t length);
int clock_gettime(clockid_t clock_id, struct timespec* tp);

static int g_failures = 0;

static void report(const char* label, int ok)
{
    printf("  [%s] %s\n", ok ? "ok" : "fail", label);
    if (!ok) {
        ++g_failures;
    }
}

int main(int argc, char** argv)
{
    const char* self_path =
        (argc > 0 && argv != 0 && argv[0] != 0 && argv[0][0] != '\0')
            ? argv[0]
            : "hello.vbin";
    const char* temp_path = "runtime-smoke.tmp";

    printf("+-----------------------------------+\n");
    printf("|   vkernel C Runtime Smoke Test    |\n");
    printf("+-----------------------------------+\n\n");

    void* allocation = malloc(4096);
    report("malloc", allocation != 0);
    if (allocation != 0) {
        memset(allocation, 0xAB, 4096);
        free(allocation);
        report("free", 1);
    }

    errno = 0;
    report("errno/ENOENT",
           open("/definitely-missing.file", O_RDONLY) == -1 && errno == ENOENT);

    int fd = open(temp_path, O_CREAT | O_TRUNC | O_RDWR, 0666);
    report("open temp file", fd >= 0);
    if (fd >= 0) {
        static const char kMessage[] = "vkernel runtime smoke\n";
        const ssize_t wrote = write(fd, kMessage, sizeof(kMessage) - 1);
        report("write", wrote == (ssize_t)(sizeof(kMessage) - 1));

        const off_t end = lseek(fd, 0, SEEK_END);
        report("lseek end", end == (off_t)(sizeof(kMessage) - 1));

        report("lseek rewind", lseek(fd, 0, SEEK_SET) == 0);

        char read_back[sizeof(kMessage)] = {0};
        const ssize_t read_count = read(fd, read_back, sizeof(read_back) - 1);
        report("read", read_count == (ssize_t)(sizeof(kMessage) - 1));
        report("read/write round trip",
               memcmp(read_back, kMessage, sizeof(kMessage) - 1) == 0);

        struct stat st = {0};
        report("fstat", fstat(fd, &st) == 0 && st.st_size == (off_t)(sizeof(kMessage) - 1));
        report("close", close(fd) == 0);
        report("stat", stat(temp_path, &st) == 0 && st.st_size == (off_t)(sizeof(kMessage) - 1));
        report("unlink", unlink(temp_path) == 0);
    }

    struct stat self_st = {0};
    report("stat self", stat(self_path, &self_st) == 0 && self_st.st_size > 0);

    struct timeval tv = {0};
    report("gettimeofday", gettimeofday(&tv, 0) == 0 && tv.tv_sec >= 0);

    struct timespec ts = {0};
    report("clock_gettime", clock_gettime(CLOCK_MONOTONIC, &ts) == 0 && ts.tv_sec >= 0);

    void* mapping = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    report("mmap", mapping != MAP_FAILED);
    if (mapping != MAP_FAILED) {
        memset(mapping, 0x5A, 4096);
        report("mmap touch", ((unsigned char*)mapping)[0] == 0x5A);
        report("munmap", munmap(mapping, 4096) == 0);
    }

    printf("\nSummary: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
