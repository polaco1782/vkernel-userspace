#include "frontend.h"
#include "spc_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

namespace vspcplay_frontend {

namespace {

constexpr int kAudioChannel = 0;
constexpr int kMemoryViewX = 16;
constexpr int kMemoryViewY = 40;
constexpr int kPortToolX = 540;
constexpr int kPortToolY = 380;

void pack_mask(unsigned char packed_mask[32])
{
    memset(packed_mask, 0, 32);
    for (int index = 0; index < 256; ++index) {
        if (used2[index])
            packed_mask[index / 8] |= static_cast<unsigned char>(128u >> (index % 8));
    }
}

void apply_block_mask(const char* filename, unsigned char filler)
{
    if (filename == nullptr)
        return;

    FILE* file = fopen(filename, "r+b");
    if (file == nullptr) {
        perror("fopen");
        return;
    }

    unsigned char filler_block[256];
    memset(filler_block, filler, sizeof(filler_block));
    for (int index = 0; index < 256; ++index) {
        if (used2[index])
            continue;
        fseek(file, 0x100 + (index * 256), SEEK_SET);
        fwrite(filler_block, sizeof(filler_block), 1, file);
    }

    fclose(file);
}

void write_mask_file(AppState* app)
{
    if (app == nullptr || app->loaded_path.empty())
        return;

    unsigned char packed_mask[32];
    pack_mask(packed_mask);

    std::string filename = app->loaded_path;
    const size_t dot = filename.rfind('.');
    if (dot != std::string::npos)
        filename.resize(dot);
    filename += ".msk";

    FILE* file = fopen(filename.c_str(), "wb");
    if (file == nullptr) {
        perror("fopen");
        return;
    }

    fwrite(packed_mask, sizeof(packed_mask), 1, file);
    for (int index = 0; index < 65536; index += 8) {
        unsigned char byte = 0;
        if (used[index + 0]) byte |= 0x80;
        if (used[index + 1]) byte |= 0x40;
        if (used[index + 2]) byte |= 0x20;
        if (used[index + 3]) byte |= 0x10;
        if (used[index + 4]) byte |= 0x08;
        if (used[index + 5]) byte |= 0x04;
        if (used[index + 6]) byte |= 0x02;
        if (used[index + 7]) byte |= 0x01;
        fwrite(&byte, 1, 1, file);
    }

    fclose(file);
    set_status(app, "WROTE MASK %s", path_basename(filename.c_str()));
}

void reset_track_state(AppState* app)
{
    app->audio.queue_read = 0;
    app->audio.queue_write = 0;
    app->audio.queue_count = 0;
    app->audio.play_block_samples = 0;
    app->audio.play_block_pending = false;
    app->audio.generated_frames = 0;
    memset(used, 0, sizeof(used));
    memset(used2, 0, sizeof(used2));
    last_pc = -1;
    if (!app->memory_surface.empty())
        memset(app->memory_surface.data(), 0, app->memory_surface.size());
    memsurface_data = app->memory_surface.empty() ? nullptr : app->memory_surface.data();
    if (!app->config.nosound)
        VK_CALL(snd_mix_stop, kAudioChannel);
}

void queue_samples(AppState* app, const int16_t* samples, vk_u32 sample_count)
{
    if (app == nullptr || samples == nullptr)
        return;

    const vk_u32 capacity = static_cast<vk_u32>(app->audio.queue.size());
    sample_count &= ~1u;
    for (vk_u32 index = 0; index < sample_count; ++index) {
        if (app->audio.queue_count >= capacity)
            return;

        app->audio.queue[app->audio.queue_write] = samples[index];
        app->audio.queue_write = (app->audio.queue_write + 1u) % capacity;
        ++app->audio.queue_count;
    }
}

auto queued_frames(const AppState* app) -> vk_u32
{
    if (app == nullptr)
        return 0;

    vk_u32 frames = app->audio.queue_count / 2u;
    if (app->audio.play_block_pending)
        frames += app->audio.play_block_samples / 2u;
    return frames;
}

auto pop_samples(AppState* app, int16_t* output, vk_u32 requested_samples) -> vk_u32
{
    const vk_u32 capacity = static_cast<vk_u32>(app->audio.queue.size());
    vk_u32 total_samples = requested_samples & ~1u;
    if (total_samples > (app->audio.queue_count & ~1u))
        total_samples = app->audio.queue_count & ~1u;

    for (vk_u32 index = 0; index < total_samples; ++index) {
        output[index] = app->audio.queue[app->audio.queue_read];
        app->audio.queue_read = (app->audio.queue_read + 1u) % capacity;
    }

    app->audio.queue_count -= total_samples;
    return total_samples;
}

auto stage_play_block(AppState* app) -> bool
{
    if (app->audio.play_block_pending || app->audio.queue_count < 2u)
        return false;

    app->audio.play_block_samples =
        pop_samples(app, app->audio.play_block.data(), static_cast<vk_u32>(app->audio.play_block.size()));
    app->audio.play_block_pending = app->audio.play_block_samples != 0;
    return app->audio.play_block_pending;
}

void audio_try_submit(AppState* app)
{
    if (app == nullptr || app->config.nosound)
        return;

    while (true) {
        if (!app->audio.play_block_pending && !stage_play_block(app))
            return;

        if (!VK_CALL(snd_mix_queue_play,
                     kAudioChannel,
                     app->audio.play_block.data(),
                     app->audio.play_block_samples / 2u,
                     VK_SND_FORMAT_SIGNED_16_STEREO,
                     kOutputSampleRate,
                     255,
                     255)) {
            return;
        }

        app->audio.play_block_pending = false;
        app->audio.play_block_samples = 0;
    }
}

auto current_playback_seconds(const AppState* app) -> vk_u64
{
    if (app == nullptr)
        return 0;

    if (app->config.nosound)
        return app->audio.generated_frames / kOutputSampleRate;

    vk_u64 now = VK_CALL(tick_count);
    vk_u64 paused_ticks = app->ui.paused_ticks;
    if (app->ui.paused && app->ui.pause_started_tick != 0 && now > app->ui.pause_started_tick)
        paused_ticks += now - app->ui.pause_started_tick;

    const vk_u64 elapsed_ticks = now > app->ui.track_start_tick
                               ? now - app->ui.track_start_tick
                               : 0;
    const vk_u64 active_ticks = elapsed_ticks > paused_ticks ? elapsed_ticks - paused_ticks : 0;
    const vk_u64 ticks_per_second = static_cast<vk_u64>(VK_CALL(ticks_per_sec));
    return ticks_per_second == 0 ? 0 : active_ticks / ticks_per_second;
}

void set_paused(AppState* app, bool paused)
{
    if (app == nullptr || app->ui.paused == paused)
        return;

    const vk_u64 now = VK_CALL(tick_count);
    app->ui.paused = paused;
    if (paused) {
        app->ui.pause_started_tick = now;
        set_status(app, "PAUSED");
        return;
    }

    if (app->ui.pause_started_tick != 0 && now > app->ui.pause_started_tick)
        app->ui.paused_ticks += now - app->ui.pause_started_tick;
    app->ui.pause_started_tick = 0;
    set_status(app, "RESUMED");
}

auto parse_song_time(const AppState* app, const id666_tag& tag) -> int
{
    int seconds = 0;
    if (!app->config.ignore_tag_time)
        seconds = atoi(tag.seconds_til_fadeout);
    if (seconds <= 0)
        seconds = app->config.default_song_time_seconds;
    seconds += app->config.extra_time_seconds;
    if (seconds <= 0)
        seconds = app->config.default_song_time_seconds;
    return seconds;
}

auto generate_audio_batch(AppState* app) -> bool
{
    if (app == nullptr || !app->loaded || app->ui.paused)
        return false;

    if (app->audio.spc_buffer_bytes <= 0)
        return false;

    SPC_update(app->audio.scratch_bytes.data());
    const vk_u32 total_samples = static_cast<vk_u32>(app->audio.spc_buffer_bytes / sizeof(int16_t));
    const vk_u32 frames = total_samples / 2u;
    app->audio.generated_frames += frames;

    if (app->wave_writer != nullptr)
        waveWriter_addSamples(app->wave_writer, app->audio.scratch_bytes.data(), static_cast<int>(frames));

    queue_samples(app,
                  reinterpret_cast<const int16_t*>(app->audio.scratch_bytes.data()),
                  total_samples);
    audio_try_submit(app);
    return true;
}

void print_status_line(AppState* app)
{
    if (app == nullptr || !app->config.show_status_line || !app->loaded)
        return;

    const vk_u64 seconds = current_playback_seconds(app);
    if (seconds == app->ui.last_status_second)
        return;

    app->ui.last_status_second = seconds;
    const int percent = app->song_time_seconds <= 0
                      ? 0
                      : static_cast<int>((seconds * 100u) / static_cast<vk_u64>(app->song_time_seconds));
    printf("%s  %llu / %d (%d%%)        \r",
           app->loaded_path.c_str(),
           static_cast<unsigned long long>(seconds),
           app->song_time_seconds,
           percent);
    fflush(stdout);
}

void advance_track(AppState* app)
{
    if (app == nullptr)
        return;

    if (app->config.auto_write_mask) {
        write_mask_file(app);
        if (app->config.apply_mask_block)
            apply_block_mask(app->loaded_path.c_str(), app->config.filler);
    }

    if (app->current_index + 1 >= static_cast<int>(app->playlist.size())) {
        app->quit_requested = true;
        return;
    }

    ++app->current_index;
    app->request_reload = true;
}

void handle_browser_key(AppState* app, const vk_key_event_t& event)
{
    if (event.pressed == 0)
        return;

    switch (event.scancode) {
        case 0x01:
            app->browser.open = false;
            break;
        case 0x48:
        case 0xC8:
            browser_select_relative(app, -1);
            break;
        case 0x50:
        case 0xD0:
            browser_select_relative(app, 1);
            break;
        case 0x49:
            browser_scroll_relative(app, -1);
            break;
        case 0x51:
            browser_scroll_relative(app, 1);
            break;
        case 0x0E:
            browser_navigate_to_parent(app);
            break;
        case 0x1C:
            (void)browser_activate_selection(app);
            break;
        default:
            break;
    }
}

void update_mouse_address(AppState* app)
{
    if (app == nullptr)
        return;

    const int local_x = app->ui.mouse_x - kMemoryViewX;
    const int local_y = app->ui.mouse_y - kMemoryViewY;
    if (local_x >= 0 && local_x < 512 && local_y >= 0 && local_y < 512) {
        app->ui.current_mouse_address = (local_y / 2) * 256 + (local_x / 2);
        if (!app->ui.hexdump_locked)
            app->ui.hexdump_address = app->ui.current_mouse_address;
    } else {
        app->ui.current_mouse_address = -1;
    }
}

void handle_mouse_click(AppState* app, vk_u32 previous_buttons, vk_u32 current_buttons)
{
    if (app == nullptr || (previous_buttons & 1u) != 0u || (current_buttons & 1u) == 0u)
        return;

    update_mouse_address(app);
    if (app->ui.current_mouse_address >= 0) {
        app->ui.hexdump_locked = !app->ui.hexdump_locked;
        if (app->ui.hexdump_locked)
            app->ui.hexdump_address = app->ui.current_mouse_address;
    }

    if (app->ui.mouse_x >= kPortToolX + (8 * 5) && app->ui.mouse_y >= kPortToolY) {
        const int x = (app->ui.mouse_x - (kPortToolX + (8 * 5))) / 8;
        const int y = (app->ui.mouse_y - kPortToolY) / 8;
        if (y == 1) {
            switch (x) {
                case 1: vspcplay_write_input_port(0, static_cast<unsigned char>(vspcplay_read_input_port(0) + 1u)); break;
                case 4: vspcplay_write_input_port(0, static_cast<unsigned char>(vspcplay_read_input_port(0) - 1u)); break;
                case 6: vspcplay_write_input_port(1, static_cast<unsigned char>(vspcplay_read_input_port(1) + 1u)); break;
                case 9: vspcplay_write_input_port(1, static_cast<unsigned char>(vspcplay_read_input_port(1) - 1u)); break;
                case 11: vspcplay_write_input_port(2, static_cast<unsigned char>(vspcplay_read_input_port(2) + 1u)); break;
                case 14: vspcplay_write_input_port(2, static_cast<unsigned char>(vspcplay_read_input_port(2) - 1u)); break;
                case 16: vspcplay_write_input_port(3, static_cast<unsigned char>(vspcplay_read_input_port(3) + 1u)); break;
                case 19: vspcplay_write_input_port(3, static_cast<unsigned char>(vspcplay_read_input_port(3) - 1u)); break;
                default: break;
            }
        }
    }

    if (app->ui.mouse_y >= static_cast<int>(app->framebuffer.height) - 16) {
        const int column = app->ui.mouse_x / 8;
        if (column >= 2 && column <= 5) {
            app->quit_requested = true;
        } else if (column >= 9 && column <= 13) {
            set_paused(app, !app->ui.paused);
        } else if (column >= 17 && column <= 23) {
            app->request_reload = true;
        } else if (column >= 27 && column <= 30) {
            request_track_offset(app, -1);
        } else if (column >= 34 && column <= 37) {
            request_track_offset(app, 1);
        } else if (column >= 41 && column <= 47) {
            browser_open(app);
        } else if (column >= 51 && column <= 54) {
            app->request_write_mask = true;
        } else if (column >= 58 && column <= 64) {
            app->ui.endless = !app->ui.endless;
        }
    }
}

} // namespace

auto init_app(AppState* app) -> bool
{
    if (app == nullptr)
        return false;

    app->spc_config.is_interpolation = app->config.interpolation ? 1 : 0;
    app->spc_config.is_echo = app->config.echo ? 1 : 0;
    app->audio.spc_buffer_bytes = SPC_init(&app->spc_config);
    if (app->audio.spc_buffer_bytes <= 0) {
        std::cout << "vspcplay: failed to initialize SPC core\n";
        return false;
    }

    app->audio.scratch_bytes.resize(static_cast<size_t>(app->audio.spc_buffer_bytes));
    app->memory_surface.resize(static_cast<size_t>(kMemorySurfaceWidth) * kMemorySurfaceHeight * 4u);
    memsurface_data = app->memory_surface.data();

    for (size_t index = 0; index < app->config.muted_at_startup.size(); ++index)
        vspcplay_set_voice_muted(static_cast<int>(index), app->config.muted_at_startup[index] != 0);

    if (!app->config.wave_output_path.empty()) {
        app->wave_writer = waveWriter_create(app->config.wave_output_path.c_str());
        if (app->wave_writer == nullptr)
            return false;
    }

    if (!app->config.novideo && !init_framebuffer(app)) {
        std::cout << "vspcplay: no framebuffer available\n";
        return false;
    }

    return true;
}

auto load_current_track(AppState* app) -> bool
{
    if (app == nullptr || app->playlist.empty())
        return false;

    if (app->current_index < 0 || app->current_index >= static_cast<int>(app->playlist.size()))
        app->current_index = 0;

    const std::string& path = app->playlist[static_cast<size_t>(app->current_index)];
    FILE* file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
        std::cout << "vspcplay: failed to open " << path << '\n';
        return false;
    }

    id666_tag tag {};
    read_id666(file, &tag);
    fclose(file);

    if (!SPC_load(path.c_str())) {
        std::cout << "vspcplay: failed to load " << path << '\n';
        return false;
    }

    reset_track_state(app);
    app->loaded = true;
    app->loaded_path = path;
    app->current_filename = path_basename(path.c_str());
    app->current_tag = tag;
    app->song_time_seconds = parse_song_time(app, tag);
    app->now_playing = tag.title[0] != '\0' ? tag.title : app->current_filename;
    app->ui.track_start_tick = VK_CALL(tick_count);
    app->ui.paused_ticks = 0;
    app->ui.pause_started_tick = 0;
    app->ui.last_status_second = static_cast<vk_u64>(-1);
    app->ui.hexdump_locked = false;
    app->ui.hexdump_address = 0;
    app->ui.current_mouse_address = -1;
    set_paused(app, false);
    set_status(app, "LOADED %s", app->now_playing.c_str());
    return true;
}

void request_track_offset(AppState* app, int delta)
{
    if (app == nullptr || delta == 0)
        return;
    app->pending_track_delta += delta;
}

void process_requests(AppState* app)
{
    if (app == nullptr)
        return;

    if (app->request_write_mask) {
        write_mask_file(app);
        app->request_write_mask = false;
    }

    if (app->pending_track_delta != 0 && !app->playlist.empty()) {
        int next_index = app->current_index + app->pending_track_delta;
        while (next_index < 0)
            next_index += static_cast<int>(app->playlist.size());
        while (next_index >= static_cast<int>(app->playlist.size()))
            next_index -= static_cast<int>(app->playlist.size());
        app->current_index = next_index;
        app->request_reload = true;
        app->pending_track_delta = 0;
    }

    if (!app->request_reload)
        return;

    app->request_reload = false;
    if (!load_current_track(app) && app->browser.open == false)
        set_status(app, "FAILED TO LOAD TRACK");
}

void pump_input(AppState* app)
{
    if (app == nullptr)
        return;

    vk_key_event_t key_event {};
    while (VK_CALL(poll_key, &key_event)) {
        if (app->browser.open) {
            handle_browser_key(app, key_event);
            continue;
        }

        if (key_event.pressed == 0)
            continue;

        switch (key_event.scancode) {
            case 0x01:
                app->quit_requested = true;
                break;
            case 0x0F:
                browser_open(app);
                break;
            case 0x39:
                set_paused(app, !app->ui.paused);
                break;
            case 0x31:
                request_track_offset(app, 1);
                break;
            case 0x19:
                request_track_offset(app, -1);
                break;
            case 0x13:
                app->request_reload = true;
                break;
            default:
                if (key_event.ascii >= '1' && key_event.ascii <= '8') {
                    const int channel = key_event.ascii - '1';
                    vspcplay_set_voice_muted(channel, !vspcplay_is_voice_muted(channel));
                } else if (key_event.ascii == '0') {
                    vspcplay_set_all_voices_muted(!vspcplay_is_voice_muted(0));
                } else if (key_event.ascii == 'e' || key_event.ascii == 'E') {
                    app->ui.endless = !app->ui.endless;
                } else if (key_event.ascii == 'm' || key_event.ascii == 'M') {
                    app->request_write_mask = true;
                }
                break;
        }
    }

    if (app->config.novideo)
        return;

    vk_mouse_event_t mouse_event {};
    while (VK_CALL(poll_mouse, &mouse_event)) {
        const vk_u32 previous_buttons = app->ui.mouse_buttons;
        app->ui.mouse_x += mouse_event.dx;
        app->ui.mouse_y += mouse_event.dy;
        app->ui.mouse_buttons = mouse_event.buttons;
        if (app->ui.mouse_x < 0)
            app->ui.mouse_x = 0;
        if (app->ui.mouse_y < 0)
            app->ui.mouse_y = 0;
        if (app->framebuffer.width != 0 && app->ui.mouse_x >= static_cast<int>(app->framebuffer.width))
            app->ui.mouse_x = static_cast<int>(app->framebuffer.width - 1u);
        if (app->framebuffer.height != 0 && app->ui.mouse_y >= static_cast<int>(app->framebuffer.height))
            app->ui.mouse_y = static_cast<int>(app->framebuffer.height - 1u);
        handle_mouse_click(app, previous_buttons, app->ui.mouse_buttons);
    }
}

void update_playback(AppState* app)
{
    if (app == nullptr || !app->loaded)
        return;

    print_status_line(app);
    if (app->ui.paused)
        return;

    if (app->config.nosound) {
        const int batches = app->config.novideo ? 128 : 8;
        for (int index = 0; index < batches; ++index) {
            if (!generate_audio_batch(app))
                break;
        }
    } else {
        int guard = 0;
        while (queued_frames(app) < kQueueLeadFrames && guard < 64) {
            if (!generate_audio_batch(app))
                break;
            ++guard;
        }
        audio_try_submit(app);
    }

    if (current_playback_seconds(app) >= static_cast<vk_u64>(app->song_time_seconds) && !app->ui.endless)
        advance_track(app);
}

void idle_until_next_work(AppState* app)
{
    if (app == nullptr)
        return;

    if (app->config.novideo && app->config.nosound) {
        VK_CALL(yield);
        return;
    }

    const vk_u64 now = VK_CALL(tick_count);
    const vk_u64 ticks_per_second = static_cast<vk_u64>(VK_CALL(ticks_per_sec));
    const vk_u64 redraw_step = ticks_per_second == 0 ? 1 : (ticks_per_second / (app->config.low_rate_ui ? 15u : kTicksPerRedraw));
    const vk_u64 next_redraw = app->ui.next_redraw_tick == 0 ? now : app->ui.next_redraw_tick;

    if (now >= next_redraw) {
        app->ui.next_redraw_tick = now + (redraw_step == 0 ? 1 : redraw_step);
        app->ui.last_short_wait_tick = 0;
        VK_CALL(yield);
        return;
    }

    if (!app->config.nosound
        && VK_CALL(snd_mix_is_playing, kAudioChannel)
        && now + 1 < next_redraw) {
        app->ui.last_short_wait_tick = 0;
        VK_CALL(sleep, 1);
        return;
    }

    if (app->ui.last_short_wait_tick == now) {
        app->ui.last_short_wait_tick = 0;
        VK_CALL(sleep, 1);
        return;
    }

    app->ui.last_short_wait_tick = now;
    VK_CALL(yield);
}

void destroy_app(AppState* app)
{
    if (app == nullptr)
        return;

    if (!app->config.nosound)
        VK_CALL(snd_mix_stop, kAudioChannel);
    SPC_close();
    if (app->wave_writer != nullptr) {
        waveWriter_close(app->wave_writer);
        waveWriter_free(app->wave_writer);
        app->wave_writer = nullptr;
    }
    if (app->config.show_status_line)
        printf("\n");
}

} // namespace vspcplay_frontend
