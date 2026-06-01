#include "frontend.h"

#include "font.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

extern "C" {
unsigned char* memsurface_data = nullptr;
unsigned char used[0x10006] = {};
unsigned char used2[0x101] = {};
int last_pc = -1;
}

namespace vspcplay_frontend {

namespace {

auto ascii_to_lower(char ch) -> char
{
    return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch;
}

void draw_char(AppState* app, int x, int y, char ch, vk_u32 color, int scale)
{
    if (app == nullptr || scale <= 0)
        return;

    unsigned char* glyph = font_getChar(ch);
    for (int row = 0; row < 7; ++row) {
        for (int column = 0; column < 8; ++column) {
            if ((glyph[row] & (0x80u >> column)) == 0)
                continue;
            fill_rect(app, x + column * scale, y + row * scale, scale, scale, color);
        }
    }
}

void clamp_mouse_to_framebuffer(AppState* app)
{
    if (app == nullptr)
        return;

    if (app->ui.mouse_x < 0)
        app->ui.mouse_x = 0;
    if (app->ui.mouse_y < 0)
        app->ui.mouse_y = 0;

    const int max_x = app->framebuffer.width == 0 ? 0 : static_cast<int>(app->framebuffer.width - 1u);
    const int max_y = app->framebuffer.height == 0 ? 0 : static_cast<int>(app->framebuffer.height - 1u);
    if (app->ui.mouse_x > max_x)
        app->ui.mouse_x = max_x;
    if (app->ui.mouse_y > max_y)
        app->ui.mouse_y = max_y;
}

} // namespace

auto pack_pixel(unsigned char r, unsigned char g, unsigned char b, vk_pixel_format_t format) -> vk_u32
{
    (void)format;
    return (static_cast<vk_u32>(r) << 16)
         | (static_cast<vk_u32>(g) << 8)
         | static_cast<vk_u32>(b);
}

auto path_basename(const char* path) -> const char*
{
    if (path == nullptr || path[0] == '\0')
        return "";

    const char* slash = strrchr(path, '/');
    if (slash != nullptr && slash[1] != '\0')
        return slash + 1;
    return path;
}

auto path_parent(const std::string& path) -> std::string
{
    const size_t slash = path.rfind('/');
    if (slash == std::string::npos || slash == 0)
        return "/";
    return path.substr(0, slash);
}

auto path_join(const std::string& parent, const std::string& child) -> std::string
{
    if (parent.empty() || parent == "/")
        return "/" + child;
    return parent + "/" + child;
}

auto strip_extension(const std::string& path) -> std::string
{
    const std::string base = path_basename(path.c_str());
    const size_t dot = base.rfind('.');
    if (dot == std::string::npos)
        return base;
    return base.substr(0, dot);
}

auto ends_with_casefolded(const char* text, const char* suffix) -> bool
{
    if (text == nullptr || suffix == nullptr)
        return false;

    const size_t text_length = strlen(text);
    const size_t suffix_length = strlen(suffix);
    if (suffix_length > text_length)
        return false;

    const char* lhs = text + text_length - suffix_length;
    while (*lhs != '\0' && *suffix != '\0') {
        if (ascii_to_lower(*lhs) != ascii_to_lower(*suffix))
            return false;
        ++lhs;
        ++suffix;
    }
    return *suffix == '\0';
}

auto parse_u64_decimal(const char* text) -> vk_u64
{
    vk_u64 value = 0;
    if (text == nullptr)
        return 0;

    while (*text >= '0' && *text <= '9') {
        value = value * 10u + static_cast<vk_u64>(*text - '0');
        ++text;
    }
    return value;
}

void set_status(AppState* app, const char* format, ...)
{
    if (app == nullptr || format == nullptr)
        return;

    char buffer[kStatusMax] = {};
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    app->status_message = buffer;
}

void print_help()
{
    std::cout
        << "vspcplay\n"
        << "Usage: vspcplay [options] files...\n"
        << "Options:\n"
        << "  -h, --help              Show this help\n"
        << "  --nosound               Disable audio output\n"
        << "  --novideo               Disable framebuffer UI\n"
        << "  --waveout file.wav      Write a WAV alongside playback\n"
        << "  --interpolation         Enable SPC interpolation\n"
        << "  --echo                  Enable echo\n"
        << "  --status_line           Print a live status line\n"
        << "  --default_time seconds  Default song time when no tag is present\n"
        << "  --ignore_tag_time       Ignore the ID666 play time\n"
        << "  --extra_time seconds    Add extra playback time\n"
        << "  --mute channel          Mute channel 1-8 or all at startup\n"
        << "  --unmute channel        Unmute channel 1-8 or all at startup\n"
        << "  --auto_write_mask       Save the usage mask when a track ends\n"
        << "  --apply_mask_block      Apply the 256-byte block mask to the SPC\n"
        << "  --filler value          Filler byte for --apply_mask_block\n"
        << "  --id666                 Print ID666 tags and exit\n";
}

namespace {

auto parse_channel_mask(AppState* app, const char* value, bool muted) -> bool
{
    if (app == nullptr || value == nullptr)
        return false;

    if (strcmp(value, "all") == 0) {
        for (unsigned char& channel : app->config.muted_at_startup)
            channel = muted ? 1u : 0u;
        return true;
    }

    const int index = atoi(value);
    if (index < 1 || index > 8)
        return false;
    app->config.muted_at_startup[static_cast<size_t>(index - 1)] = muted ? 1u : 0u;
    return true;
}

auto take_option_value(int argc, char** argv, int* index) -> const char*
{
    if (argv == nullptr || index == nullptr)
        return nullptr;
    if (*index + 1 >= argc)
        return nullptr;
    ++(*index);
    return argv[*index];
}

} // namespace

auto parse_args(AppState* app, int argc, char** argv) -> bool
{
    if (app == nullptr)
        return false;

    app->playlist.clear();
    for (int index = 1; index < argc; ++index) {
        const char* arg = argv[index];
        if (arg == nullptr)
            continue;

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_help();
            return false;
        }
        if (strcmp(arg, "--nosound") == 0) {
            app->config.nosound = true;
            continue;
        }
        if (strcmp(arg, "--novideo") == 0) {
            app->config.novideo = true;
            continue;
        }
        if (strcmp(arg, "--interpolation") == 0) {
            app->config.interpolation = true;
            continue;
        }
        if (strcmp(arg, "--echo") == 0) {
            app->config.echo = true;
            continue;
        }
        if (strcmp(arg, "--status_line") == 0) {
            app->config.show_status_line = true;
            continue;
        }
        if (strcmp(arg, "--ignore_tag_time") == 0) {
            app->config.ignore_tag_time = true;
            continue;
        }
        if (strcmp(arg, "--auto_write_mask") == 0) {
            app->config.auto_write_mask = true;
            continue;
        }
        if (strcmp(arg, "--apply_mask_block") == 0) {
            app->config.apply_mask_block = true;
            continue;
        }
        if (strcmp(arg, "--id666") == 0) {
            app->config.show_id666 = true;
            continue;
        }
        if (strcmp(arg, "--nice") == 0 || strcmp(arg, "--yield") == 0) {
            app->config.low_rate_ui = true;
            continue;
        }
        if (strcmp(arg, "--update_in_callback") == 0 || strcmp(arg, "--apply_mask_byte") == 0) {
            continue;
        }
        if (strcmp(arg, "--waveout") == 0) {
            const char* value = take_option_value(argc, argv, &index);
            if (value == nullptr) {
                std::cout << "vspcplay: --waveout expects a file path\n";
                return false;
            }
            app->config.wave_output_path = value;
            continue;
        }
        if (strcmp(arg, "--default_time") == 0) {
            const char* value = take_option_value(argc, argv, &index);
            if (value == nullptr) {
                std::cout << "vspcplay: --default_time expects a value\n";
                return false;
            }
            app->config.default_song_time_seconds = atoi(value);
            continue;
        }
        if (strcmp(arg, "--extra_time") == 0) {
            const char* value = take_option_value(argc, argv, &index);
            if (value == nullptr) {
                std::cout << "vspcplay: --extra_time expects a value\n";
                return false;
            }
            app->config.extra_time_seconds = atoi(value);
            continue;
        }
        if (strcmp(arg, "--filler") == 0) {
            const char* value = take_option_value(argc, argv, &index);
            if (value == nullptr) {
                std::cout << "vspcplay: --filler expects a value\n";
                return false;
            }
            app->config.filler = static_cast<unsigned char>(strtoul(value, nullptr, 0));
            continue;
        }
        if (strcmp(arg, "--mute") == 0) {
            const char* value = take_option_value(argc, argv, &index);
            if (!parse_channel_mask(app, value, true)) {
                std::cout << "vspcplay: invalid --mute channel\n";
                return false;
            }
            continue;
        }
        if (strcmp(arg, "--unmute") == 0) {
            const char* value = take_option_value(argc, argv, &index);
            if (!parse_channel_mask(app, value, false)) {
                std::cout << "vspcplay: invalid --unmute channel\n";
                return false;
            }
            continue;
        }

        app->playlist.emplace_back(arg);
    }

    return true;
}

void print_id666_info(const AppState& app)
{
    std::cout << app.playlist.size() << " files\n";
    for (size_t index = 0; index < app.playlist.size(); ++index) {
        if (index != 0)
            std::cout << "------\n";

        const std::string& path = app.playlist[index];
        std::cout << "File: " << path << '\n';
        FILE* file = fopen(path.c_str(), "rb");
        if (file == nullptr) {
            std::cout << "Could not open file " << path << '\n';
            continue;
        }

        id666_tag tag {};
        read_id666(file, &tag);
        print_id666(&tag);
        fclose(file);
    }
}

auto refresh_framebuffer(AppState* app) -> bool
{
    if (app == nullptr)
        return false;

    vk_framebuffer_info_t framebuffer {};
    VK_CALL(framebuffer_info, &framebuffer);
    if (framebuffer.valid == 0
        || framebuffer.base == 0
        || framebuffer.width == 0
        || framebuffer.height == 0) {
        return false;
    }

    const bool changed = app->framebuffer.base != framebuffer.base
                      || app->framebuffer.width != framebuffer.width
                      || app->framebuffer.height != framebuffer.height
                      || app->framebuffer.stride != framebuffer.stride
                      || app->framebuffer.format != framebuffer.format;
    app->framebuffer = framebuffer;
    if (changed) {
        const vk_usize pixels =
            static_cast<vk_usize>(framebuffer.stride) * static_cast<vk_usize>(framebuffer.height);
        app->present_buffer.resize(pixels);
        clamp_mouse_to_framebuffer(app);
    }
    return true;
}

auto init_framebuffer(AppState* app) -> bool
{
    if (!refresh_framebuffer(app))
        return false;

    vk_set_framebuffer_resize_events(1);
    vk_set_startup_window_size(kUiWidth, kUiHeight);
    app->ui.mouse_x = static_cast<int>(app->framebuffer.width / 2u);
    app->ui.mouse_y = static_cast<int>(app->framebuffer.height / 2u);
    app->ui.next_redraw_tick = VK_CALL(tick_count);
    return true;
}

void begin_frame(AppState* app)
{
    if (app == nullptr || app->present_buffer.empty())
        return;

    const vk_u32 width = app->framebuffer.width;
    const vk_u32 height = app->framebuffer.height;
    for (vk_u32 y = 0; y < height; ++y) {
        const unsigned char r = static_cast<unsigned char>(8u + (y * 10u) / (height == 0 ? 1u : height));
        const unsigned char g = static_cast<unsigned char>(8u + (y * 8u) / (height == 0 ? 1u : height));
        const unsigned char b = static_cast<unsigned char>(18u + (y * 26u) / (height == 0 ? 1u : height));
        const vk_u32 color = pack_pixel(r, g, b, app->framebuffer.format);
        vk_u32* row = &app->present_buffer[static_cast<vk_usize>(y) * app->framebuffer.stride];
        for (vk_u32 x = 0; x < width; ++x)
            row[x] = color;
    }
}

void present_frame(const AppState* app)
{
    if (app == nullptr || app->present_buffer.empty())
        return;

    memcpy(reinterpret_cast<void*>(static_cast<uintptr_t>(app->framebuffer.base)),
           app->present_buffer.data(),
           app->present_buffer.size() * sizeof(vk_u32));
}

void fill_rect(AppState* app, int x, int y, int width, int height, vk_u32 color)
{
    if (app == nullptr || app->present_buffer.empty() || width <= 0 || height <= 0)
        return;

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + width;
    int y1 = y + height;
    const int max_width = static_cast<int>(app->framebuffer.width);
    const int max_height = static_cast<int>(app->framebuffer.height);

    if (x1 > max_width)
        x1 = max_width;
    if (y1 > max_height)
        y1 = max_height;
    if (x0 >= x1 || y0 >= y1)
        return;

    for (int row = y0; row < y1; ++row) {
        vk_u32* dst =
            &app->present_buffer[(static_cast<vk_usize>(row) * app->framebuffer.stride) + static_cast<vk_usize>(x0)];
        for (int column = x0; column < x1; ++column)
            dst[column - x0] = color;
    }
}

auto draw_text(AppState* app, int x, int y, const char* text, vk_u32 color, int scale) -> int
{
    int cursor_x = x;
    if (text == nullptr)
        return cursor_x;

    for (const char* cursor = text; *cursor != '\0'; ++cursor) {
        draw_char(app, cursor_x, y, *cursor, color, scale);
        cursor_x += 8 * scale;
    }
    return cursor_x;
}

void draw_text_clipped(AppState* app, int x, int y, const char* text, vk_u32 color, int scale, int max_chars)
{
    if (text == nullptr || max_chars <= 0)
        return;

    int cursor_x = x;
    int count = 0;
    for (const char* cursor = text; *cursor != '\0' && count < max_chars; ++cursor, ++count) {
        draw_char(app, cursor_x, y, *cursor, color, scale);
        cursor_x += 8 * scale;
    }
}

} // namespace vspcplay_frontend
