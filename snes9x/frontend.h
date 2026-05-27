#pragma once

#include <array>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

extern "C" {
#include "../include/vk.h"
}

#include "../snes9x-src/display.h"
#include "../snes9x-src/memmap.h"
#include "../snes9x-src/gfx.h"
#include "../snes9x-src/apu/apu.h"
#include "../snes9x-src/controls.h"

namespace snes9x_frontend {

inline constexpr vk_u32 kOutputSampleRate = 44100;
inline constexpr vk_u32 kPlayBlockFrames = 4096;
inline constexpr vk_u32 kQueueCapacityFrames = kPlayBlockFrames * 4;
inline constexpr vk_u32 kQueuePrimeFrames = kPlayBlockFrames * 2;
inline constexpr vk_u32 kMaxCatchupFrames = 8;
inline constexpr vk_u32 kMaxLogMessage = 512;
inline constexpr size_t kMaxRomBytes = CMemory::MAX_ROM_SIZE + 512u;

inline constexpr vk_u32 kRomBrowserMaxEntries = 128;
inline constexpr vk_u32 kRomBrowserNameMax = 96;
inline constexpr vk_u32 kRomBrowserPathMax = 256;
inline constexpr vk_u32 kRomBrowserStatusMax = 128;
inline constexpr vk_u32 kRomBrowserResponseMax = 16384;
inline constexpr vk_u32 kRomBrowserItemMax = 128;

inline constexpr vk_u32 kUiGlyphWidth = 5;
inline constexpr vk_u32 kUiGlyphHeight = 7;
inline constexpr vk_u32 kUiFontScale = 2;
inline constexpr vk_u32 kUiFontAdvance = (kUiGlyphWidth + 1u) * kUiFontScale;
inline constexpr vk_u32 kUiRowHeight = (kUiGlyphHeight * kUiFontScale) + 8u;
inline constexpr vk_u32 kUiPanelMargin = 16;
inline constexpr vk_u32 kUiPanelPadding = 18;
inline constexpr vk_u32 kUiHeaderHeight = 76;
inline constexpr vk_u32 kUiFooterHeight = 30;
inline constexpr vk_u32 kStartupWindowWidth = 512;
inline constexpr vk_u32 kStartupWindowHeight = 448;

enum ButtonId : vk_u32 {
    kButtonB = 0,
    kButtonA,
    kButtonY,
    kButtonX,
    kButtonL,
    kButtonR,
    kButtonStart,
    kButtonSelect,
    kButtonUp,
    kButtonDown,
    kButtonLeft,
    kButtonRight,
    kButtonCount
};

struct AudioState {
    int16_t queue[kQueueCapacityFrames * 2] {};
    vk_u32 queue_read = 0;
    vk_u32 queue_write = 0;
    vk_u32 queue_count = 0;
    int16_t play_block[kPlayBlockFrames * 2] {};
    vk_u32 play_block_samples = 0;
    bool play_block_pending = false;
    std::vector<int16_t> scratch;
};

struct TimingState {
    vk_u64 next_frame_tick = 0;
    vk_u64 frame_tick_numerator = 0;
    vk_u64 frame_tick_denominator = 1;
    vk_u64 frame_tick_remainder = 0;
    vk_u64 last_short_wait_tick = 0;
};

struct RomBrowserEntry {
    std::string name;
    vk_u64 size_bytes = 0;
    bool is_directory = false;
};

struct RomBrowserState {
    std::string current_path;
    std::string status;
    std::array<RomBrowserEntry, kRomBrowserMaxEntries> entries {};
    char response[kRomBrowserResponseMax] {};
    char raw_items[kRomBrowserMaxEntries][kRomBrowserItemMax] {};
    vk_u32 entry_count = 0;
    vk_u32 selected_index = 0;
    vk_u32 scroll_index = 0;
};

struct AppState {
    vk_framebuffer_info_t framebuffer {};
    std::vector<vk_u32> present_buffer;
    std::vector<vk_u32> converted_frame;
    std::string loaded_rom_path;
    std::string rom_title;
    bool rom_loaded = false;
    bool quit_requested = false;
    bool reset_requested = false;
    bool loadrom_requested = false;
    AudioState audio;
    TimingState timing;
    RomBrowserState browser;
};

vk_u32 min_u32(vk_u32 lhs, vk_u32 rhs);
vk_u32 max_u32(vk_u32 lhs, vk_u32 rhs);
int compare_casefolded(const char* lhs, const char* rhs);
bool ends_with_casefolded(const char* text, const char* suffix);
const char* path_basename(const char* path);
std::string path_parent(const std::string& path);
std::string path_join(const std::string& parent, const std::string& child);
vk_u32 pack_pixel(unsigned char r, unsigned char g, unsigned char b, vk_pixel_format_t format);
vk_u32 rgb565_to_pixel(uint16_t pixel, vk_pixel_format_t format);

void audio_reset(AppState* app);
void audio_try_submit(AppState* app);
void snes_audio_samples_available(AppState* app);

bool refresh_framebuffer(AppState* app, bool* changed = nullptr);
bool init_framebuffer(AppState* app);
bool load_rom(AppState* app, const char* path);
bool init_emulator(AppState* app);
void set_timing(AppState* app);
void schedule_next_frame(AppState* app);
void idle_until_next_work(AppState* app);
void reset_emulator(AppState* app);
void pump_input(AppState* app);
bool browse_and_load_rom(AppState* app);
void destroy_app(AppState* app);

} // namespace snes9x_frontend
