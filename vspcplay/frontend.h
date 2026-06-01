#pragma once

#include <array>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

extern "C" {
#include "../include/vk.h"
#include "id666.h"
#include "libspc.h"
#include "wavewriter.h"
}

namespace vspcplay_frontend {

inline constexpr vk_u32 kOutputSampleRate = 44100;
inline constexpr vk_u32 kUiWidth = 800;
inline constexpr vk_u32 kUiHeight = 600;
inline constexpr vk_u32 kMemorySurfaceWidth = 512;
inline constexpr vk_u32 kMemorySurfaceHeight = 512;
inline constexpr vk_u32 kPlayBlockFrames = 4096;
inline constexpr vk_u32 kQueueCapacityFrames = kPlayBlockFrames * 4;
inline constexpr vk_u32 kQueueLeadFrames = kPlayBlockFrames * 2;
inline constexpr vk_u32 kMaxQueuedSampleCount = kQueueCapacityFrames * 2;
inline constexpr vk_u32 kBrowserMaxEntries = 512;
inline constexpr vk_u32 kBrowserItemMax = 128;
inline constexpr vk_u32 kBrowserResponseMax = 65536;
inline constexpr vk_u32 kStatusMax = 160;
inline constexpr vk_u32 kScratchMaxBytes = 4096;
inline constexpr vk_u32 kTicksPerRedraw = 60;

struct Config {
    bool nosound = false;
    bool novideo = false;
    bool show_status_line = false;
    bool interpolation = true;
    bool echo = true;
    bool ignore_tag_time = false;
    bool auto_write_mask = false;
    bool apply_mask_block = false;
    bool show_id666 = false;
    bool low_rate_ui = false;
    int default_song_time_seconds = 300;
    int extra_time_seconds = 0;
    unsigned char filler = 0x00;
    std::array<unsigned char, 8> muted_at_startup {};
    std::string wave_output_path;
};

struct AudioState {
    std::array<int16_t, kMaxQueuedSampleCount> queue {};
    std::array<int16_t, kPlayBlockFrames * 2> play_block {};
    std::vector<unsigned char> scratch_bytes;
    vk_u32 queue_read = 0;
    vk_u32 queue_write = 0;
    vk_u32 queue_count = 0;
    vk_u32 play_block_samples = 0;
    bool play_block_pending = false;
    vk_u64 generated_frames = 0;
    int spc_buffer_bytes = 0;
};

struct UiState {
    int mouse_x = 0;
    int mouse_y = 0;
    int current_mouse_address = -1;
    int hexdump_address = 0;
    bool hexdump_locked = false;
    bool paused = false;
    bool endless = false;
    vk_u32 mouse_buttons = 0;
    vk_u64 track_start_tick = 0;
    vk_u64 paused_ticks = 0;
    vk_u64 pause_started_tick = 0;
    vk_u64 next_redraw_tick = 0;
    vk_u64 last_status_second = static_cast<vk_u64>(-1);
    vk_u64 last_short_wait_tick = 0;
};

struct BrowserEntry {
    std::string name;
    vk_u64 size_bytes = 0;
    bool is_directory = false;
};

struct BrowserState {
    bool open = false;
    std::string current_path;
    std::string status;
    std::array<char, kBrowserResponseMax> response {};
    std::array<std::array<char, kBrowserItemMax>, kBrowserMaxEntries> raw_items {};
    std::array<BrowserEntry, kBrowserMaxEntries> entries {};
    vk_u32 entry_count = 0;
    vk_u32 selected_index = 0;
    vk_u32 scroll_index = 0;
};

struct AppState {
    Config config {};
    SPC_Config spc_config { kOutputSampleRate, 16, 2, 0, 0 };
    vk_framebuffer_info_t framebuffer {};
    std::vector<vk_u32> present_buffer;
    std::vector<unsigned char> memory_surface;
    std::vector<std::string> playlist;
    std::string loaded_path;
    std::string status_message;
    std::string now_playing;
    std::string current_filename;
    id666_tag current_tag {};
    AudioState audio {};
    UiState ui {};
    BrowserState browser {};
    WaveWriter* wave_writer = nullptr;
    int current_index = 0;
    int song_time_seconds = 0;
    bool quit_requested = false;
    bool loaded = false;
    bool request_reload = false;
    int pending_track_delta = 0;
    bool request_write_mask = false;
};

extern "C" unsigned char* memsurface_data;
extern "C" unsigned char used[0x10006];
extern "C" unsigned char used2[0x101];
extern "C" int last_pc;

auto pack_pixel(unsigned char r, unsigned char g, unsigned char b, vk_pixel_format_t format) -> vk_u32;
auto path_basename(const char* path) -> const char*;
auto path_parent(const std::string& path) -> std::string;
auto path_join(const std::string& parent, const std::string& child) -> std::string;
auto strip_extension(const std::string& path) -> std::string;
auto ends_with_casefolded(const char* text, const char* suffix) -> bool;
auto parse_u64_decimal(const char* text) -> vk_u64;

void set_status(AppState* app, const char* format, ...);
void print_help();
auto parse_args(AppState* app, int argc, char** argv) -> bool;
void print_id666_info(const AppState& app);

auto refresh_framebuffer(AppState* app) -> bool;
auto init_framebuffer(AppState* app) -> bool;
void begin_frame(AppState* app);
void present_frame(const AppState* app);
void fill_rect(AppState* app, int x, int y, int width, int height, vk_u32 color);
auto draw_text(AppState* app, int x, int y, const char* text, vk_u32 color, int scale = 1) -> int;
void draw_text_clipped(AppState* app, int x, int y, const char* text, vk_u32 color, int scale, int max_chars);

void browser_open(AppState* app);
auto browser_refresh_listing(AppState* app) -> bool;
void browser_navigate_to_parent(AppState* app);
auto browser_activate_selection(AppState* app) -> bool;
void browser_select_relative(AppState* app, int delta);
void browser_scroll_relative(AppState* app, int delta);

auto init_app(AppState* app) -> bool;
auto load_current_track(AppState* app) -> bool;
void request_track_offset(AppState* app, int delta);
void process_requests(AppState* app);
void pump_input(AppState* app);
void update_playback(AppState* app);
void idle_until_next_work(AppState* app);
void destroy_app(AppState* app);
void render(AppState* app);

} // namespace vspcplay_frontend
