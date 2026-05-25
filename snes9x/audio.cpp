#include "frontend.h"

namespace snes9x_frontend {

namespace {

constexpr int kAudioChannel = 0;

void audio_queue_push(AppState* app, const int16_t* samples, vk_u32 sample_count)
{
    if (app == nullptr || samples == nullptr)
        return;

    sample_count &= ~1u;
    if (sample_count == 0)
        return;

    const vk_u32 capacity = kQueueCapacityFrames * 2u;
    for (vk_u32 index = 0; index < sample_count; ++index) {
        if (app->audio.queue_count >= capacity)
            return;

        app->audio.queue[app->audio.queue_write] = samples[index];
        app->audio.queue_write = (app->audio.queue_write + 1u) % capacity;
        ++app->audio.queue_count;
    }
}

vk_u32 audio_pop_samples(AppState* app, int16_t* output, vk_u32 requested_samples)
{
    const vk_u32 capacity = kQueueCapacityFrames * 2u;
    vk_u32 total_samples = min_u32(requested_samples & ~1u, app->audio.queue_count & ~1u);

    for (vk_u32 index = 0; index < total_samples; ++index) {
        output[index] = app->audio.queue[app->audio.queue_read];
        app->audio.queue_read = (app->audio.queue_read + 1u) % capacity;
    }

    app->audio.queue_count -= total_samples;
    return total_samples;
}

bool audio_stage_play_block(AppState* app)
{
    if (app->audio.play_block_pending || app->audio.queue_count < 2u)
        return false;

    app->audio.play_block_samples =
        audio_pop_samples(app, app->audio.play_block, kPlayBlockFrames * 2u);
    app->audio.play_block_pending = app->audio.play_block_samples != 0;
    return app->audio.play_block_pending;
}

} // namespace

void snes_audio_samples_available(AppState* app)
{
    if (app == nullptr)
        return;

    int available_samples = S9xGetSampleCount();
    if (available_samples <= 0)
        return;

    available_samples &= ~1;
    if (available_samples <= 0)
        return;

    app->audio.scratch.resize(static_cast<size_t>(available_samples));
    if (!S9xMixSamples(reinterpret_cast<uint8*>(app->audio.scratch.data()), available_samples))
        return;

    audio_queue_push(app,
                     app->audio.scratch.data(),
                     static_cast<vk_u32>(available_samples));
}

void audio_try_submit(AppState* app)
{
    while (true) {
        if (!app->audio.play_block_pending && !audio_stage_play_block(app))
            return;

        if (!VK_CALL(snd_mix_queue_play,
                     kAudioChannel,
                     app->audio.play_block,
                     app->audio.play_block_samples / 2u,
                     VK_SND_FORMAT_SIGNED_16_STEREO,
                     kOutputSampleRate,
                     255, 255)) {
            return;
        }

        app->audio.play_block_samples = 0;
        app->audio.play_block_pending = false;
    }
}

void audio_reset(AppState* app)
{
    if (app == nullptr)
        return;

    VK_CALL(snd_mix_stop, kAudioChannel);
    app->audio.queue_read = 0;
    app->audio.queue_write = 0;
    app->audio.queue_count = 0;
    app->audio.play_block_samples = 0;
    app->audio.play_block_pending = false;
    app->audio.scratch.clear();
    S9xClearSamples();
}

} // namespace snes9x_frontend
