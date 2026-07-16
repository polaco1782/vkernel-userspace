/*
 * dsl_vk.c - vkernel audio backend for Duke's digital sound mixer
 *
 * Duke's audiolib fills ring pages through MV_ServiceVoc(). We reuse those
 * pages directly and queue them into the kernel mixer as a simple audio
 * stream.
 */

#include <stdint.h>
#include <string.h>

#include "../include/vk.h"

#include "../duke3d-src/Game/src/audiolib/dsl.h"

#define VK_DUKE_STREAM_CHANNEL (VK_SND_MIX_CHANNELS - 1)

static int dsl_error_code = DSL_Ok;
static int dsl_initialized;
static void (*dsl_callback)(void);
static volatile char *dsl_buffer_start;
static int dsl_page_bytes;
static int dsl_num_pages;
static unsigned dsl_sample_rate;
static int dsl_mix_mode;
static vk_u32 dsl_format;
static int dsl_frame_bytes;
static int dsl_page_frames;
static int dsl_submit_index;
static int dsl_submitted_frames;
static int dsl_played_frames;
static vk_u64 dsl_playback_tick;
static vk_u64 dsl_playback_residual;

static void dsl_set_error(int error_code)
{
    dsl_error_code = error_code;
}

static int dsl_frames_buffered(void)
{
    return dsl_submitted_frames - dsl_played_frames;
}

static void dsl_sync_playback_clock(void)
{
    if (!dsl_initialized || dsl_sample_rate == 0) {
        return;
    }

    const vk_u64 now = VK_CALL(tick_count);
    if (dsl_playback_tick == 0) {
        dsl_playback_tick = now;
    }

    if (dsl_frames_buffered() <= 0) {
        dsl_played_frames = dsl_submitted_frames;
        dsl_playback_tick = now;
        dsl_playback_residual = 0;
        return;
    }

    if (!VK_CALL(snd_mix_is_playing, VK_DUKE_STREAM_CHANNEL)) {
        dsl_played_frames = dsl_submitted_frames;
        dsl_playback_tick = now;
        dsl_playback_residual = 0;
        return;
    }

    dsl_playback_residual += (now - dsl_playback_tick) * (vk_u64)dsl_sample_rate;
    dsl_playback_tick = now;

    const vk_u32 ticks_per_sec = VK_CALL(ticks_per_sec);
    if (ticks_per_sec == 0) {
        return;
    }

    int elapsed_frames = (int)(dsl_playback_residual / ticks_per_sec);
    dsl_playback_residual %= ticks_per_sec;

    if (elapsed_frames > dsl_frames_buffered()) {
        elapsed_frames = dsl_frames_buffered();
        dsl_playback_residual = 0;
    }

    dsl_played_frames += elapsed_frames;
}

static void dsl_prime_page(void)
{
    if (dsl_callback == NULL || dsl_page_frames <= 0) {
        return;
    }

    dsl_callback();
    const char *page = (const char *)dsl_buffer_start + (dsl_submit_index * dsl_page_bytes);
    if (VK_CALL(snd_mix_queue_play,
                VK_DUKE_STREAM_CHANNEL,
                page,
                (vk_u32)dsl_page_frames,
                dsl_format,
                (vk_u32)dsl_sample_rate,
                255,
                255)) {
        dsl_submit_index = (dsl_submit_index + 1) % dsl_num_pages;
        dsl_submitted_frames += dsl_page_frames;
    }
}

char *DSL_ErrorString(int error_number)
{
    switch (error_number) {
        case DSL_Warning:
        case DSL_Error:
            return DSL_ErrorString(dsl_error_code);
        case DSL_Ok:
            return "vkernel DSL backend ok.";
        case DSL_SDLInitFailure:
            return "Audio initialization failed.";
        case DSL_MixerActive:
            return "Audio backend already initialized.";
        case DSL_MixerInitFailure:
            return "Audio backend could not start.";
        default:
            return "Unknown DSL error.";
    }
}

int DSL_Init(void)
{
    dsl_set_error(DSL_Ok);
    return DSL_Ok;
}

void DSL_StopPlayback(void)
{
    VK_CALL(snd_mix_stop, VK_DUKE_STREAM_CHANNEL);
    dsl_initialized = 0;
    dsl_callback = NULL;
    dsl_buffer_start = NULL;
    dsl_page_bytes = 0;
    dsl_num_pages = 0;
    dsl_sample_rate = 0;
    dsl_page_frames = 0;
    dsl_submit_index = 0;
    dsl_submitted_frames = 0;
    dsl_played_frames = 0;
    dsl_playback_tick = 0;
    dsl_playback_residual = 0;
}

unsigned DSL_GetPlaybackRate(void)
{
    return dsl_sample_rate;
}

int DSL_BeginBufferedPlayback(char *buffer_start,
                              int buffer_size,
                              int num_divisions,
                              unsigned sample_rate,
                              int mix_mode,
                              void (*callback)(void))
{
    if (dsl_initialized) {
        dsl_set_error(DSL_MixerActive);
        return DSL_Error;
    }

    dsl_callback = callback;
    dsl_buffer_start = buffer_start;
    dsl_num_pages = num_divisions;
    dsl_page_bytes = num_divisions > 0 ? (buffer_size / num_divisions) : 0;
    dsl_sample_rate = sample_rate;
    dsl_mix_mode = mix_mode;
    dsl_submit_index = 0;
    dsl_submitted_frames = 0;
    dsl_played_frames = 0;
    dsl_playback_tick = VK_CALL(tick_count);
    dsl_playback_residual = 0;

    if ((dsl_mix_mode & SIXTEEN_BIT) != 0) {
        dsl_frame_bytes = (dsl_mix_mode & STEREO) ? 4 : 2;
        dsl_format = (dsl_mix_mode & STEREO)
            ? VK_SND_FORMAT_SIGNED_16_STEREO
            : VK_SND_FORMAT_SIGNED_16;
    } else {
        dsl_frame_bytes = 1;
        dsl_format = VK_SND_FORMAT_UNSIGNED_8;
    }

    dsl_page_frames = dsl_frame_bytes > 0 ? (dsl_page_bytes / dsl_frame_bytes) : 0;
    if (dsl_page_frames <= 0 || dsl_num_pages <= 0 || dsl_buffer_start == NULL) {
        dsl_set_error(DSL_MixerInitFailure);
        return DSL_Error;
    }

    dsl_initialized = 1;
    for (int i = 0; i < dsl_num_pages && i < 3; ++i) {
        dsl_prime_page();
    }

    return DSL_Ok;
}

void DSL_Shutdown(void)
{
    DSL_StopPlayback();
}

uint32_t DisableInterrupts(void)
{
    return 0;
}

void RestoreInterrupts(uint32_t flags)
{
    (void)flags;
}

void DSL_VK_Service(void)
{
    if (!dsl_initialized) {
        return;
    }

    dsl_sync_playback_clock();
    while (dsl_frames_buffered() < dsl_page_frames * 2) {
        dsl_prime_page();
        if (dsl_callback == NULL) {
            break;
        }
    }
}
