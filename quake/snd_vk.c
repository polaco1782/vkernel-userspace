/*
 * snd_vk.c - Sound DMA driver for Chocolate Quake on vkernel
 * Replaces src/sound/src/snd_sdl.c
 *
 * The Quake sound engine fills a ring buffer (shm->buffer) via S_PaintChannels.
 * We play chunks from that ring buffer through the VK mixer.
 *
 * Strategy: use a single VK mix channel as a "stream" channel.
 * Periodically check if playing; if not, submit the next ~100ms chunk.
 */

#include "sound.h"
#include "sys.h"

#include <stdlib.h>
#include <string.h>

#include "../include/vk.h"

/* The DMA structure filled by the engine - global defined in snd_dma.c */
/* extern volatile dma_t* shm; -- declared in sound.h */

/* VK mix channel we use as the audio stream */
#define STREAM_CHANNEL (VK_SND_MIX_CHANNELS - 1)

/* Keep Quake's DMA read cursor separate from the next ring region we queue. */
#define STREAM_CHUNK_FRAMES   2048
#define STREAM_TARGET_FRAMES  (STREAM_CHUNK_FRAMES * 2)

static vk_u64 playback_tick = 0;
static vk_u64 playback_residual = 0;
static int    played_frame_cursor = 0;
static int    queued_frame_cursor = 0;

/* ---------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------- */

static int round_up_pow2(int n)
{
    if (n <= 0) return 1;
    n--;
    n |= n >> 1; n |= n >> 2; n |= n >> 4;
    n |= n >> 8; n |= n >> 16;
    return n + 1;
}

static int ring_buffer_frames(void)
{
    if (!shm || shm->channels <= 0)
        return 0;

    return shm->samples / shm->channels;
}

static int bytes_per_frame(void)
{
    if (!shm || shm->samplebits <= 0)
        return 0;

    return shm->channels * (shm->samplebits / 8);
}

static void sync_samplepos(void)
{
    int frames = ring_buffer_frames();

    if (!shm || frames <= 0)
        return;

    shm->samplepos = (played_frame_cursor & (frames - 1)) * shm->channels;
}

static void reset_playback_clock(void)
{
    playback_tick = VK_CALL(tick_count);
    playback_residual = 0;
}

static void update_playback_cursor(void)
{
    if (!shm)
        return;

    int buffered_frames = queued_frame_cursor - played_frame_cursor;
    if (buffered_frames < 0) {
        played_frame_cursor = queued_frame_cursor;
        buffered_frames = 0;
        playback_residual = 0;
    }

    vk_u64 now = VK_CALL(tick_count);
    if (playback_tick == 0)
        playback_tick = now;

    if (buffered_frames == 0) {
        playback_tick = now;
        playback_residual = 0;
        sync_samplepos();
        return;
    }

    if (!VK_CALL(snd_mix_is_playing, STREAM_CHANNEL)) {
        played_frame_cursor = queued_frame_cursor;
        playback_tick = now;
        playback_residual = 0;
        sync_samplepos();
        return;
    }

    playback_residual += (now - playback_tick) * (vk_u64)shm->speed;
    playback_tick = now;

    vk_u32 ticks_per_second = VK_CALL(ticks_per_sec);
    int elapsed_frames = (int)(playback_residual / ticks_per_second);
    playback_residual %= ticks_per_second;

    if (elapsed_frames > buffered_frames) {
        elapsed_frames = buffered_frames;
        playback_residual = 0;
    }

    played_frame_cursor += elapsed_frames;
    sync_samplepos();
}

/* ---------------------------------------------------------------
 * SNDDMA interface
 * --------------------------------------------------------------- */

qboolean SNDDMA_Init(dma_t *dma)
{
    shm = dma;
    memset((void *)shm, 0, sizeof(*shm));

    shm->samplebits = 16;
    shm->signed8    = 0;
    shm->speed      = 44100;
    shm->channels   = 2;   /* stereo */
    shm->samplepos  = 0;
    shm->submission_chunk = 1;

    /* Ring buffer: must be a power-of-2 number of stereo 16-bit frames.
     * ~1s of audio at 44100 Hz stereo 16-bit = 44100 * 2 * 2 = 176400 bytes.
     * Round up to next power-of-2. */
    int frames   = round_up_pow2(shm->speed);
    int nbytes   = frames * shm->channels * (shm->samplebits / 8);

    shm->samples = frames * shm->channels; /* total samples (L+R interleaved) */
    shm->buffer  = (byte *)calloc(1, (size_t)nbytes);
    if (!shm->buffer) {
        shm = NULL;
        return false;
    }

    played_frame_cursor = 0;
    queued_frame_cursor = 0;
    sync_samplepos();
    reset_playback_clock();

    return true;
}

int SNDDMA_GetDMAPos(void)
{
    if (!shm)
        return 0;

    update_playback_cursor();
    return shm->samplepos;
}

void SNDDMA_Shutdown(void)
{
    if (!shm) return;
    VK_CALL(snd_mix_stop, STREAM_CHANNEL);
    if (shm->buffer) {
        free(shm->buffer);
        shm->buffer = NULL;
    }
    played_frame_cursor = 0;
    queued_frame_cursor = 0;
    playback_tick = 0;
    playback_residual = 0;
    shm = NULL;
}

void SNDDMA_LockBuffer(void)  {}
void SNDDMA_UnblockSound(void) {}
void SNDDMA_BlockSound(void)   {}

void SNDDMA_Submit(void)
{
    if (!shm || !shm->buffer)
        return;

    update_playback_cursor();

    if (queued_frame_cursor < played_frame_cursor)
        queued_frame_cursor = played_frame_cursor;

    const int fullsamples = ring_buffer_frames();
    const int frame_bytes = bytes_per_frame();
    if (fullsamples <= 0 || frame_bytes <= 0)
        return;

    while ((queued_frame_cursor - played_frame_cursor) < STREAM_TARGET_FRAMES) {
        int available_frames = paintedtime - queued_frame_cursor;
        if (available_frames <= 0)
            break;

        int chunk_frames = STREAM_CHUNK_FRAMES;
        if (chunk_frames > available_frames)
            chunk_frames = available_frames;

        int ring_frame = queued_frame_cursor & (fullsamples - 1);
        int frames_to_end = fullsamples - ring_frame;
        if (chunk_frames > frames_to_end)
            chunk_frames = frames_to_end;
        if (chunk_frames <= 0)
            break;

        const byte* chunk = shm->buffer + ring_frame * frame_bytes;
        if (!VK_CALL(snd_mix_queue_play,
                     STREAM_CHANNEL,
                     chunk,
                     chunk_frames,
                     VK_SND_FORMAT_SIGNED_16_STEREO,
                     (vk_u32)shm->speed,
                     255, 255)) {
            break;
        }

        queued_frame_cursor += chunk_frames;
        if (playback_tick == 0)
            reset_playback_clock();
    }
}
