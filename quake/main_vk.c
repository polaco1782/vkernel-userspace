/*
 * main_vk.c - Chocolate Quake entry point for vkernel
 * Replaces main.c (which uses SDL_main.h).
 */

/* Must be defined before any header that pulls in SDL.h */
#define SDL_GETTICKS_PROVIDED

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "host.h"
#include "sys.h"
#include "../include/vk.h"

int main(int argc, char *argv[])
{
    const uint64_t ticks_per_second = VK_CALL(ticks_per_sec);
    const uint64_t min_frame_ticks =
        ticks_per_second > 72 ? (ticks_per_second / 72) : 1;
    uint64_t last_frame_tick = VK_CALL(tick_count);

    printf("Chocolate Quake " PACKAGE_VERSION " - vkernel build\n");

    quakeparms_t *parms = Sys_Init(argc, argv);
    Host_Init(parms);

    double old_time = Sys_FloatTime();
    while (1) {
        double new_time = Sys_FloatTime();
        double dt = new_time - old_time;
        int previous_framecount = host_framecount;
        Host_Frame((float)dt);
        old_time = new_time;

        if (host_framecount != previous_framecount) {
            last_frame_tick = VK_CALL(tick_count);
        } else {
            uint64_t now_tick = VK_CALL(tick_count);
            uint64_t next_frame_tick = last_frame_tick + min_frame_ticks;

            if (now_tick + 1 < next_frame_tick) {
                VK_CALL(sleep, next_frame_tick - now_tick - 1);
            } else {
                VK_CALL(yield);
            }
        }
    }
    return 0;
}

/* SDL_GetTicks - real implementation (replaces inline stub in SDL.h) */
uint32_t SDL_GetTicks(void)
{
    uint64_t ticks = VK_CALL(tick_count);
    uint32_t tps   = VK_CALL(ticks_per_sec);
    return (uint32_t)((ticks * 1000ULL) / tps);
}

void SDL_Delay(uint32_t ms)
{
    if (!ms) return;
    uint32_t tps = VK_CALL(ticks_per_sec);
    uint64_t n   = ((uint64_t)ms * tps) / 1000;
    if (!n) n = 1;
    while (n-- > 0)
        VK_CALL(sleep, 1);
}
