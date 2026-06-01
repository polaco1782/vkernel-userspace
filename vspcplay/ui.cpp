#include "frontend.h"
#include "spc_backend.h"

#include <stdio.h>
#include <string.h>

namespace vspcplay_frontend {

namespace {

constexpr int kMemoryViewX = 16;
constexpr int kMemoryViewY = 40;
constexpr int kPortToolX = 540;
constexpr int kPortToolY = 380;
constexpr int kInfoX = 540;
constexpr int kInfoY = 420;

void fade_memory_surface()
{
    if (memsurface_data == nullptr)
        return;

    const size_t total_bytes = static_cast<size_t>(kMemorySurfaceWidth) * kMemorySurfaceHeight * 4u;
    for (size_t index = 0; index < total_bytes; ++index) {
        if (memsurface_data[index] > 0x40)
            --memsurface_data[index];
    }
}

void blit_memory_surface(AppState* app, int dst_x, int dst_y)
{
    if (app == nullptr || memsurface_data == nullptr)
        return;

    for (int y = 0; y < static_cast<int>(kMemorySurfaceHeight); ++y) {
        if (dst_y + y < 0 || dst_y + y >= static_cast<int>(app->framebuffer.height))
            continue;

        vk_u32* dst =
            &app->present_buffer[(static_cast<vk_usize>(dst_y + y) * app->framebuffer.stride) + static_cast<vk_usize>(dst_x)];
        const unsigned char* src = memsurface_data + (static_cast<size_t>(y) * kMemorySurfaceWidth * 4u);
        for (int x = 0; x < static_cast<int>(kMemorySurfaceWidth); ++x) {
            const size_t pixel_index = static_cast<size_t>(x) * 4u;
            dst[x] = pack_pixel(src[pixel_index + 2u], src[pixel_index + 1u], src[pixel_index + 0u], app->framebuffer.format);
        }
    }
}

auto hexdump_color(AppState* app, int address) -> vk_u32
{
    if (app == nullptr || memsurface_data == nullptr)
        return pack_pixel(255, 255, 255, app->framebuffer.format);

    const int wrapped = address & 0xffff;
    const int idx = (((wrapped & 0xff00) << 4) + ((wrapped & 0xff) << 3));
    const unsigned char red = static_cast<unsigned char>(0x7f + (memsurface_data[idx + 0] >> 1));
    const unsigned char green = static_cast<unsigned char>(0x7f + (memsurface_data[idx + 1] >> 1));
    const unsigned char blue = static_cast<unsigned char>(0x7f + (memsurface_data[idx + 2] >> 1));
    return pack_pixel(blue, green, red, app->framebuffer.format);
}

void draw_mouse_cursor(AppState* app)
{
    const vk_u32 color = pack_pixel(255, 224, 96, app->framebuffer.format);
    fill_rect(app, app->ui.mouse_x - 4, app->ui.mouse_y, 9, 1, color);
    fill_rect(app, app->ui.mouse_x, app->ui.mouse_y - 4, 1, 9, color);
}

void draw_browser(AppState* app)
{
    const vk_u32 panel = pack_pixel(12, 16, 24, app->framebuffer.format);
    const vk_u32 frame = pack_pixel(178, 202, 255, app->framebuffer.format);
    const vk_u32 text = pack_pixel(255, 255, 255, app->framebuffer.format);
    const vk_u32 dim = pack_pixel(168, 184, 220, app->framebuffer.format);
    const vk_u32 selected = pack_pixel(56, 92, 160, app->framebuffer.format);
    const vk_u32 selected_text = pack_pixel(255, 246, 214, app->framebuffer.format);

    fill_rect(app, 28, 28, 744, 544, panel);
    fill_rect(app, 28, 28, 744, 2, frame);
    fill_rect(app, 28, 570, 744, 2, frame);
    fill_rect(app, 28, 28, 2, 544, frame);
    fill_rect(app, 770, 28, 2, 544, frame);

    draw_text(app, 48, 48, "SPC BROWSER", text, 1);
    draw_text_clipped(app, 48, 66, app->browser.current_path.c_str(), dim, 1, 86);
    draw_text_clipped(app, 48, 84, app->browser.status.c_str(), dim, 1, 86);

    int row_y = 116;
    constexpr vk_u32 kVisibleRows = 26;
    for (vk_u32 row = 0; row < kVisibleRows; ++row) {
        const vk_u32 entry_index = app->browser.scroll_index + row;
        if (entry_index >= app->browser.entry_count)
            break;

        const BrowserEntry& entry = app->browser.entries[entry_index];
        const bool selected_row = entry_index == app->browser.selected_index;
        if (selected_row)
            fill_rect(app, 44, row_y - 2, 712, 16, selected);

        char label[128] = {};
        snprintf(label,
                 sizeof(label),
                 "%c %s",
                 entry.is_directory ? '>' : ' ',
                 entry.name.c_str());
        draw_text_clipped(app, 48, row_y, label, selected_row ? selected_text : text, 1, 48);

        char size_text[32] = {};
        if (entry.is_directory) {
            snprintf(size_text, sizeof(size_text), "<DIR>");
        } else {
            snprintf(size_text, sizeof(size_text), "%llu", static_cast<unsigned long long>(entry.size_bytes));
        }
        draw_text_clipped(app, 650, row_y, size_text, selected_row ? selected_text : dim, 1, 10);
        row_y += 16;
    }

    draw_text(app, 48, 540, "UP/DOWN SELECT  ENTER OPEN  BACKSPACE PARENT  ESC CLOSE", dim, 1);
}

void draw_main_ui(AppState* app)
{
    static int songx_pos = 0;
    static int songx_dir = 1;
    static int songx_tick = 0;

    const vk_u32 white = pack_pixel(255, 255, 255, app->framebuffer.format);
    const vk_u32 black = pack_pixel(0, 0, 0, app->framebuffer.format);
    const vk_u32 yellow = pack_pixel(255, 255, 0, app->framebuffer.format);
    const vk_u32 cyan = pack_pixel(0, 255, 255, app->framebuffer.format);
    const vk_u32 magenta = pack_pixel(255, 0, 255, app->framebuffer.format);
    const vk_u32 red = pack_pixel(255, 0, 0, app->framebuffer.format);
    const vk_u32 gray = pack_pixel(127, 127, 127, app->framebuffer.format);

    fade_memory_surface();

    if (++songx_tick >= 4) {
        songx_tick = 0;
        songx_pos += songx_dir;
        if (songx_pos >= 768) {
            songx_pos = 768;
            songx_dir = -1;
        } else if (songx_pos <= 0) {
            songx_pos = 0;
            songx_dir = 1;
        }
    }

    draw_text(app, kMemoryViewX + songx_pos, kMemoryViewY - 20, "SONG:", white, 1);
    draw_text_clipped(app, kMemoryViewX + songx_pos + 8 * 5, kMemoryViewY - 20, app->current_filename.c_str(), yellow, 1, 28);

    fill_rect(app, kMemoryViewX - 1, kMemoryViewY - 1, 514, 514, white);
    fill_rect(app, kMemoryViewX, kMemoryViewY, 512, 512, black);
    blit_memory_surface(app, kMemoryViewX, kMemoryViewY);
    draw_text(app, kMemoryViewX, kMemoryViewY - 10, "SPC MEMORY", white, 1);

    char buffer[256] = {};
    if (app->ui.current_mouse_address >= 0) {
        snprintf(buffer, sizeof(buffer), "ADDR $%04X", app->ui.current_mouse_address);
        draw_text(app, kMemoryViewX + 8 * 24, kMemoryViewY - 10, buffer, white, 1);
    }

    snprintf(buffer, sizeof(buffer), "PC $%04X", last_pc & 0xffff);
    draw_text(app, kMemoryViewX + 8 * 14, kMemoryViewY - 10, buffer, white, 1);

    int used_blocks = 0;
    for (int index = 0; index < 256; ++index) {
        if (!used2[index])
            continue;
        ++used_blocks;
        fill_rect(app, kMemoryViewX - 3, kMemoryViewY + index * 2, 2, 2, white);
    }

    snprintf(buffer, sizeof(buffer), "BLOCKS USED %3d/256", used_blocks);
    draw_text(app, kMemoryViewX, kMemoryViewY + 516, buffer, white, 1);

    draw_text(app, kMemoryViewX + 520, 32, "VSPCPLAY", white, 1);
    draw_text_clipped(app, kMemoryViewX + 520, 48, app->now_playing.c_str(), yellow, 1, 28);
    draw_text_clipped(app, kMemoryViewX + 520, 64, app->status_message.c_str(), gray, 1, 28);

    draw_text(app, kMemoryViewX + 520, 86, "VOICE PITCH", white, 1);
    fill_rect(app, kMemoryViewX + 520, 96, 240, 8 * 8, black);
    for (int voice = 0; voice < 8; ++voice) {
        const unsigned short pitch = static_cast<unsigned short>(
            vspcplay_read_dsp_reg(static_cast<unsigned char>(2 + (voice * 0x10)))
          | (vspcplay_read_dsp_reg(static_cast<unsigned char>(3 + (voice * 0x10))) << 8));
        snprintf(buffer, sizeof(buffer), "%d", voice);
        draw_text(app, kMemoryViewX + 520, 96 + voice * 8, buffer, white, 1);
        const int dot_x = kMemoryViewX + 540 + static_cast<int>((pitch * 200u) / 0x4000u);
        fill_rect(app, dot_x, 98 + voice * 8, 5, 5, white);
    }

    int graph_y = 168;
    draw_text(app, kMemoryViewX + 520, graph_y, "VOICE VOLUME", white, 1);
    draw_text(app, kMemoryViewX + 520 + (16 * 8), graph_y, "LEFT", yellow, 1);
    draw_text(app, kMemoryViewX + 520 + (22 * 8), graph_y, "RIGHT", cyan, 1);
    draw_text(app, kMemoryViewX + 520 + (29 * 8), graph_y, "GAIN", magenta, 1);
    graph_y += 10;
    fill_rect(app, kMemoryViewX + 520, graph_y, 240, 82, black);
    for (int voice = 0; voice < 8; ++voice) {
        const unsigned char left = vspcplay_read_dsp_reg(static_cast<unsigned char>(0 + (voice * 0x10)));
        const unsigned char right = vspcplay_read_dsp_reg(static_cast<unsigned char>(1 + (voice * 0x10)));
        const unsigned char gain = static_cast<unsigned char>(vspcplay_read_envx(voice));
        const int row_y = graph_y + voice * 10;

        snprintf(buffer, sizeof(buffer), "%d", voice);
        draw_text(app, kMemoryViewX + 520, row_y, buffer, white, 1);
        if (vspcplay_is_voice_muted(voice))
            draw_text(app, kMemoryViewX + 720, row_y, "MUTED", gray, 1);

        fill_rect(app, kMemoryViewX + 538, row_y, (left * 200) / 255, 2, yellow);
        fill_rect(app, kMemoryViewX + 538, row_y + 3, (right * 200) / 255, 2, cyan);
        fill_rect(app, kMemoryViewX + 538, row_y + 6, (gain * 200) / 255, 2, magenta);
    }

    int dump_y = 264;
    draw_text(app, kMemoryViewX + 520, dump_y, "HEXDUMP", white, 1);
    if (app->ui.hexdump_locked)
        draw_text(app, kMemoryViewX + 520 + 14 * 8, dump_y, "LOCKED", red, 1);
    dump_y += 10;
    for (int line = 0; line < 16; ++line) {
        const int base_address = (app->ui.hexdump_address + line * 8) & 0xffff;
        snprintf(buffer, sizeof(buffer), "%04X:", base_address);
        draw_text(app, kMemoryViewX + 520, dump_y, buffer, white, 1);
        int x = kMemoryViewX + 520 + 6 * 8;
        for (int column = 0; column < 8; ++column) {
            const int address = (base_address + column) & 0xffff;
            snprintf(buffer, sizeof(buffer), "%02X", vspcplay_read_apu_byte(static_cast<uint16_t>(address)));
            draw_text(app, x, dump_y, buffer, hexdump_color(app, address), 1);
            x += 20;
        }
        dump_y += 9;
    }

    draw_text(app, kPortToolX, kPortToolY, "PORT TOOL", white, 1);
    draw_text(app, kPortToolX, kPortToolY + 8, "APU:", white, 1);
    draw_text(app, kPortToolX, kPortToolY + 16, "SNES:", white, 1);
    snprintf(buffer,
             sizeof(buffer),
             " +%02X- +%02X- +%02X- +%02X-",
             vspcplay_read_input_port(0),
             vspcplay_read_input_port(1),
             vspcplay_read_input_port(2),
             vspcplay_read_input_port(3));
    draw_text(app, kPortToolX + 40, kPortToolY + 8, buffer, white, 1);
    snprintf(buffer,
             sizeof(buffer),
             "  %02X   %02X   %02X   %02X",
             vspcplay_read_output_port(0),
             vspcplay_read_output_port(1),
             vspcplay_read_output_port(2),
             vspcplay_read_output_port(3));
    draw_text(app, kPortToolX + 40, kPortToolY + 16, buffer, white, 1);

    draw_text(app, kInfoX, kInfoY, "INFO", white, 1);
    snprintf(buffer, sizeof(buffer), "FILE: %s", app->current_filename.c_str());
    draw_text_clipped(app, kInfoX, kInfoY + 8, buffer, white, 1, 31);
    snprintf(buffer, sizeof(buffer), "TITLE: %s", app->current_tag.title);
    draw_text_clipped(app, kInfoX, kInfoY + 16, buffer, white, 1, 31);
    snprintf(buffer, sizeof(buffer), "GAME: %s", app->current_tag.game_title);
    draw_text_clipped(app, kInfoX, kInfoY + 24, buffer, white, 1, 31);
    snprintf(buffer, sizeof(buffer), "DUMPER: %s", app->current_tag.name_of_dumper);
    draw_text_clipped(app, kInfoX, kInfoY + 32, buffer, white, 1, 31);
    snprintf(buffer, sizeof(buffer), "COMMENT: %s", app->current_tag.comments);
    draw_text_clipped(app, kInfoX, kInfoY + 40, buffer, white, 1, 31);
    snprintf(buffer,
             sizeof(buffer),
             "TIME: %llu / %d",
             static_cast<unsigned long long>(app->config.nosound
                 ? (app->audio.generated_frames / kOutputSampleRate)
                 : ((VK_CALL(tick_count) - app->ui.track_start_tick - app->ui.paused_ticks) / VK_CALL(ticks_per_sec))),
             app->song_time_seconds);
    draw_text(app, kInfoX, kInfoY + 48, buffer, white, 1);
    snprintf(buffer, sizeof(buffer), "ECHO: %s", app->config.echo ? "ON" : "OFF");
    draw_text(app, kInfoX, kInfoY + 56, buffer, white, 1);
    snprintf(buffer, sizeof(buffer), "INTERP: %s", app->config.interpolation ? "ON" : "OFF");
    draw_text(app, kInfoX, kInfoY + 64, buffer, white, 1);
    snprintf(buffer, sizeof(buffer), "AUTOMASK: %s", app->config.auto_write_mask ? "YES" : "NO");
    draw_text(app, kInfoX, kInfoY + 72, buffer, white, 1);
    snprintf(buffer, sizeof(buffer), "ENDLESS: %s", app->ui.endless ? "ON" : "OFF");
    draw_text(app, kInfoX, kInfoY + 80, buffer, white, 1);
    snprintf(buffer, sizeof(buffer), "PAUSED: %s", app->ui.paused ? "YES" : "NO");
    draw_text(app, kInfoX, kInfoY + 88, buffer, white, 1);

    draw_text(app,
              0,
              static_cast<int>(app->framebuffer.height) - 10,
              " QUIT - PAUSE - RESTART - PREV - NEXT - BROWSER - MASK - ENDLESS",
              yellow,
              1);
    draw_mouse_cursor(app);
}

} // namespace

void render(AppState* app)
{
    if (app == nullptr || app->config.novideo)
        return;
    if (!refresh_framebuffer(app))
        return;

    begin_frame(app);
    if (app->browser.open) {
        draw_browser(app);
    } else if (app->loaded) {
        draw_main_ui(app);
    } else {
        const vk_u32 text = pack_pixel(255, 255, 255, app->framebuffer.format);
        draw_text(app, 32, 40, "VSPCPLAY", text, 1);
        draw_text(app, 32, 58, "TAB OPENS THE SPC BROWSER", text, 1);
        if (!app->status_message.empty())
            draw_text_clipped(app, 32, 76, app->status_message.c_str(), text, 1, 48);
    }
    present_frame(app);
}

} // namespace vspcplay_frontend
