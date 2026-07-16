/*
 * display_vk.c - vkernel display/input backend for Chocolate Duke3D
 *
 * The BUILD engine renders into an 8-bit paletted surface. We keep that
 * surface in userspace memory and scale it into the current vkernel
 * framebuffer on each page flip.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "display.h"
#include "draw.h"
#include "engine.h"
#include "filesystem.h"
#include "keyboard.h"
#include "global.h"

#include "../include/vk.h"
#include "sdl_shim/SDL.h"

int _argc;
char **_argv;

int32_t xres, yres, bytesperline, imageSize, maxpages;
uint8_t *frameplace;
uint8_t *frameoffset;
uint8_t *screen;
uint8_t vesachecked;
int32_t buffermode, origbuffermode, linearmode;
uint8_t permanentupdate = 0;
uint8_t vgacompatible = 1;

SDL_Surface *surface = NULL;

static SDL_Surface vk_surface;
static vk_framebuffer_info_t vk_fb;
static uint32_t palette_xrgb[256];
uint8_t lastPalette[768];

static uint8_t drawpixel_color;
static char *title_name_long;
static char *title_name_short;
static uint8_t keyboard_queue[256];
static unsigned keyboard_head;
static unsigned keyboard_tail;
static int32_t mouse_relative_x;
static int32_t mouse_relative_y;
static short mouse_buttons;
static int vk_cursor_visible = 0;
static int vk_mouse_grabbed = 1;

static vk_u64 timer_ticks_per_second;
static int32_t timer_last_sample;
static int timer_tics_per_second;
static void (*user_timer_callback)(void);

extern void DSL_VK_Service(void);

static int vk_query_framebuffer(vk_framebuffer_info_t *out)
{
    vk_framebuffer_info_t framebuffer = {0};
    VK_CALL(framebuffer_info, &framebuffer);
    if (!framebuffer.valid || framebuffer.base == 0
        || framebuffer.width == 0 || framebuffer.height == 0) {
        return 0;
    }

    if (out != NULL) {
        *out = framebuffer;
    }

    return 1;
}

static uint32_t vk_pack_pixel(uint8_t r, uint8_t g, uint8_t b)
{
    if (vk_fb.format == VK_PIXEL_FORMAT_RGBX_8BPP) {
        return ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
    }

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void vk_blit_full_frame(void)
{
    if (screen == NULL || !vk_query_framebuffer(&vk_fb)) {
        return;
    }

    const int src_w = xres;
    const int src_h = yres;
    const int dst_w = (int)vk_fb.width;
    const int dst_h = (int)vk_fb.height;
    const int x_scale = (src_w << 16) / dst_w;
    const int y_scale = (src_h << 16) / dst_h;
    uint32_t *dst = (uint32_t *)(uintptr_t)vk_fb.base;

    for (int y = 0; y < dst_h; ++y) {
        const int sy = ((y * y_scale) >> 16) < src_h ? ((y * y_scale) >> 16) : (src_h - 1);
        const uint8_t *src_row = screen + sy * src_w;
        uint32_t *dst_row = dst + y * vk_fb.stride;
        for (int x = 0; x < dst_w; ++x) {
            const int sx = ((x * x_scale) >> 16) < src_w ? ((x * x_scale) >> 16) : (src_w - 1);
            dst_row[x] = palette_xrgb[src_row[sx]];
        }
    }
}

static void vk_update_timer(void)
{
    if (timer_ticks_per_second == 0 || timer_tics_per_second == 0) {
        return;
    }

    const vk_u64 now = VK_CALL(tick_count);
    const int32_t current_sample = (int32_t)((now * (vk_u64)timer_tics_per_second) / timer_ticks_per_second);
    int32_t delta = current_sample - timer_last_sample;

    if (delta <= 0) {
        return;
    }

    totalclock += delta;
    timer_last_sample += delta;
    if (user_timer_callback != NULL) {
        while (delta-- > 0) {
            user_timer_callback();
        }
    }
}

static void vk_push_keycode(uint8_t code)
{
    const unsigned next_tail = (keyboard_tail + 1u) % (unsigned)sizeof(keyboard_queue);
    if (next_tail == keyboard_head) {
        return;
    }

    keyboard_queue[keyboard_tail] = code;
    keyboard_tail = next_tail;
}

static int vk_translate_extended(uint32_t scancode, uint8_t *out_code)
{
    switch (scancode) {
        case 0x9d: *out_code = 0x1d; return 1;
        case 0xb8: *out_code = 0x38; return 1;
        case 0xc7: *out_code = 0x47; return 1;
        case 0xc8: *out_code = 0x48; return 1;
        case 0xc9: *out_code = 0x49; return 1;
        case 0xcb: *out_code = 0x4b; return 1;
        case 0xcd: *out_code = 0x4d; return 1;
        case 0xcf: *out_code = 0x4f; return 1;
        case 0xd0: *out_code = 0x50; return 1;
        case 0xd1: *out_code = 0x51; return 1;
        case 0xd2: *out_code = 0x52; return 1;
        case 0xd3: *out_code = 0x53; return 1;
        default: return 0;
    }
}

static void vk_poll_input(void)
{
    vk_key_event_t key_event;
    vk_mouse_event_t mouse_event;

    while (VK_CALL(poll_key, &key_event)) {
        uint8_t translated = 0;

        if (vk_translate_extended(key_event.scancode, &translated)) {
            vk_push_keycode(0xe0);
            vk_push_keycode((uint8_t)(translated | (key_event.pressed ? 0x00u : 0x80u)));
            continue;
        }

        if ((key_event.scancode & 0x7fu) == 0 || (key_event.scancode & 0x7fu) >= MAXKEYBOARDSCAN) {
            continue;
        }

        translated = (uint8_t)(key_event.scancode & 0x7fu);
        if (!key_event.pressed) {
            translated |= 0x80u;
        }
        vk_push_keycode(translated);
    }

    while (VK_CALL(poll_mouse, &mouse_event)) {
        mouse_relative_x += mouse_event.dx;
        mouse_relative_y += mouse_event.dy;
        mouse_buttons = 0;
        if (mouse_event.buttons & 0x1u) {
            mouse_buttons |= 1;
        }
        if (mouse_event.buttons & 0x2u) {
            mouse_buttons |= 2;
        }
        if (mouse_event.buttons & 0x4u) {
            mouse_buttons |= 4;
        }
    }
}

static void vk_reset_screen_buffer(int32_t width, int32_t height)
{
    if (screen != NULL) {
        free(screen);
        screen = NULL;
    }

    if (horizlookup != NULL) {
        free(horizlookup);
        horizlookup = NULL;
    }
    if (horizlookup2 != NULL) {
        free(horizlookup2);
        horizlookup2 = NULL;
    }

    screen = (uint8_t *)malloc((size_t)width * (size_t)height);
    if (screen == NULL) {
        Error(EXIT_FAILURE, "Unable to allocate %dx%d Duke framebuffer.\n", width, height);
    }
    memset(screen, 0, (size_t)width * (size_t)height);

    frameplace = screen;
    frameoffset = screen;
    vk_surface.w = width;
    vk_surface.h = height;
    vk_surface.pitch = width;
    vk_surface.pixels = screen;
    surface = &vk_surface;

    xdim = xres = width;
    ydim = yres = height;
    ydim16 = height;
    bytesperline = width;
    imageSize = width * height;
    maxpages = 1;
    vesachecked = 1;
    vgacompatible = 1;
    linearmode = 1;
    qsetmode = height;
    activepage = visualpage = 0;

    const size_t lookup_bytes = (size_t)(ydim + 1) * sizeof(ylookup[0]);
    const size_t horiz_bytes = (size_t)(ydim * 4) * sizeof(int32_t);
    horizlookup = (int32_t *)malloc(horiz_bytes);
    horizlookup2 = (int32_t *)malloc(horiz_bytes);
    if (horizlookup == NULL || horizlookup2 == NULL) {
        Error(EXIT_FAILURE, "Unable to allocate Duke lookup tables.\n");
    }

    for (int32_t i = 0, offset = 0; i <= ydim; ++i, offset += bytesperline) {
        ylookup[i] = offset;
    }
    (void)lookup_bytes;

    horizycent = (ydim * 4) >> 1;
    oxdimen = oviewingrange = oxyaspect = -1;
    setBytesPerLine(bytesperline);
    setview(0, 0, xdim - 1, ydim - 1);
    setbrightness(curbrightness, palette);

    if (searchx < 0) {
        searchx = halfxdimen;
        searchy = ydimen >> 1;
    }
}

void restore256_palette(void)
{
}

void set16color_palette(void)
{
}

int vk_sdl_get_grab_input(void)
{
    return vk_mouse_grabbed;
}

void vk_sdl_set_grab_input(int enabled)
{
    vk_mouse_grabbed = enabled != 0;
}

void vk_sdl_set_cursor_visible(int visible)
{
    vk_cursor_visible = visible != 0;
}

void vk_sdl_quit(void)
{
}

void _platform_init(int argc, char **argv, const char *title, const char *iconName)
{
    (void)iconName;

    _argc = argc;
    _argv = argv;
    title_name_long = (char *)title;
    title_name_short = (char *)title;
    vk_query_framebuffer(&vk_fb);
    vk_set_framebuffer_resize_events(1);
    if (screen == NULL) {
        vk_reset_screen_buffer(640, 480);
    }
    /* vkernel does not provide a functional process cwd yet, so point the
     * engine at the staged asset directory explicitly. */
    setGameDir("/data/duke3d");
    (void)chdir("/data/duke3d");
}

void _idle(void)
{
    vk_poll_input();
    vk_update_timer();
    DSL_VK_Service();
}

void _handle_events(void)
{
    vk_framebuffer_event_t framebuffer_event;

    vk_poll_input();
    while (VK_CALL(poll_framebuffer_event, &framebuffer_event)) {
        if (framebuffer_event.type == VK_FRAMEBUFFER_EVENT_RESIZED) {
            vk_fb = framebuffer_event.framebuffer;
        }
    }
    vk_update_timer();
    DSL_VK_Service();
}

void *_getVideoBase(void)
{
    return screen;
}

void initkeys(void)
{
}

void uninitkeys(void)
{
}

void _nextpage(void)
{
    _handle_events();
    vk_blit_full_frame();
}

void _uninitengine(void)
{
    if (screen != NULL) {
        free(screen);
        screen = NULL;
        frameplace = NULL;
        frameoffset = NULL;
        surface = NULL;
    }
    if (horizlookup != NULL) {
        free(horizlookup);
        horizlookup = NULL;
    }
    if (horizlookup2 != NULL) {
        free(horizlookup2);
        horizlookup2 = NULL;
    }
}

void _joystick_init(void)
{
}

void _joystick_deinit(void)
{
}

int _joystick_update(void)
{
    return 0;
}

int _joystick_axis(int axis)
{
    (void)axis;
    return 0;
}

int _joystick_hat(int hat)
{
    (void)hat;
    return 0;
}

int _joystick_button(int button)
{
    (void)button;
    return 0;
}

void getvalidvesamodes(void)
{
    static const int32_t candidates[][2] = {
        {320, 200},
        {320, 240},
        {400, 300},
        {512, 384},
        {640, 400},
        {640, 480},
        {800, 600},
        {1024, 768},
    };

    validmodecnt = 0;
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (!vk_query_framebuffer(&vk_fb)) {
            break;
        }
        if ((vk_u32)candidates[i][0] > vk_fb.width || (vk_u32)candidates[i][1] > vk_fb.height) {
            continue;
        }
        validmode[validmodecnt] = (short)validmodecnt;
        validmodexdim[validmodecnt] = candidates[i][0];
        validmodeydim[validmodecnt] = candidates[i][1];
        ++validmodecnt;
    }

    if (validmodecnt == 0) {
        validmode[0] = 0;
        validmodexdim[0] = 320;
        validmodeydim[0] = 200;
        validmodecnt = 1;
    }
}

int VBE_setPalette(uint8_t *palettebuffer)
{
    memcpy(lastPalette, palettebuffer, sizeof(lastPalette));
    for (int i = 0; i < 256; ++i) {
        const uint8_t b = (uint8_t)((palettebuffer[i * 4 + 0] * 255u) / 63u);
        const uint8_t g = (uint8_t)((palettebuffer[i * 4 + 1] * 255u) / 63u);
        const uint8_t r = (uint8_t)((palettebuffer[i * 4 + 2] * 255u) / 63u);
        palette_xrgb[i] = vk_pack_pixel(r, g, b);
    }
    return 1;
}

int VBE_getPalette(int32_t start, int32_t num, uint8_t *palettebuffer)
{
    memcpy(palettebuffer + start * 4, lastPalette + start * 4, (size_t)num * 4u);
    return 1;
}

void setvmode(int mode)
{
    if (mode == 0x3 && screen != NULL) {
        memset(screen, 0, (size_t)xres * (size_t)yres);
        _nextpage();
    }
}

uint8_t readpixel(uint8_t *location)
{
    return *location;
}

void drawpixel(uint8_t *location, uint8_t pixel)
{
    *location = pixel;
}

void setcolor16(uint8_t color)
{
    drawpixel_color = color;
}

void drawpixel16(int32_t offset)
{
    if (screen == NULL || offset < 0 || offset >= imageSize) {
        return;
    }
    screen[offset] = drawpixel_color;
}

void fillscreen16(int32_t offset, int32_t color, int32_t blocksize)
{
    if (screen == NULL || blocksize <= 0) {
        return;
    }

    if (offset < 0) {
        blocksize += offset;
        offset = 0;
    }
    if (offset >= imageSize || blocksize <= 0) {
        return;
    }
    if (offset + blocksize > imageSize) {
        blocksize = imageSize - offset;
    }

    memset(screen + offset, color, (size_t)blocksize);
    _nextpage();
}

void clear2dscreen(void)
{
    if (screen != NULL) {
        memset(screen, 0, (size_t)xres * (size_t)yres);
    }
}

void _updateScreenRect(int32_t x, int32_t y, int32_t w, int32_t h)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    vk_blit_full_frame();
}

int setupmouse(void)
{
    mouse_relative_x = 0;
    mouse_relative_y = 0;
    mouse_buttons = 0;
    vk_mouse_grabbed = 1;
    vk_cursor_visible = 0;
    return 1;
}

void readmousexy(short *x, short *y)
{
    if (x != NULL) {
        *x = (short)mouse_relative_x;
    }
    if (y != NULL) {
        *y = (short)mouse_relative_y;
    }
    mouse_relative_x = 0;
    mouse_relative_y = 0;
}

void readmousebstatus(short *bstatus)
{
    if (bstatus != NULL) {
        *bstatus = mouse_buttons;
    }
}

uint8_t _readlastkeyhit(void)
{
    if (keyboard_head == keyboard_tail) {
        return 0;
    }

    const uint8_t code = keyboard_queue[keyboard_head];
    keyboard_head = (keyboard_head + 1u) % (unsigned)sizeof(keyboard_queue);
    return code;
}

void (*installusertimercallback(void (*callback)(void)))(void)
{
    void (*old)(void) = user_timer_callback;
    user_timer_callback = callback;
    return old;
}

int inittimer(int tickspersecond)
{
    if (timer_ticks_per_second != 0) {
        return 0;
    }

    timer_ticks_per_second = VK_CALL(ticks_per_sec);
    if (timer_ticks_per_second == 0) {
        return -1;
    }

    timer_tics_per_second = tickspersecond;
    timer_last_sample = (int32_t)((VK_CALL(tick_count) * (vk_u64)tickspersecond) / timer_ticks_per_second);
    user_timer_callback = NULL;
    return 0;
}

void uninittimer(void)
{
    timer_ticks_per_second = 0;
    timer_tics_per_second = 0;
    timer_last_sample = 0;
    user_timer_callback = NULL;
}

uint32_t getticks(void)
{
    const vk_u32 ticks_per_sec = VK_CALL(ticks_per_sec);
    if (ticks_per_sec == 0) {
        return 0;
    }
    return (uint32_t)((VK_CALL(tick_count) * 1000u) / ticks_per_sec);
}

void sampletimer(void)
{
    vk_update_timer();
}

void drawline16(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t color)
{
    if (screen == NULL) {
        return;
    }

    int dx = abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        if (x0 >= 0 && x0 < xres && y0 >= 0 && y0 < yres) {
            screen[y0 * bytesperline + x0] = color;
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int e2 = err << 1;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

int screencapture(char *filename, uint8_t inverseit)
{
    (void)filename;
    (void)inverseit;
    return 0;
}

int32_t _setgamemode(uint8_t davidoption, int32_t daxdim, int32_t daydim)
{
    (void)davidoption;
    getvalidvesamodes();
    for (int i = 0; i < validmodecnt; ++i) {
        if (validmodexdim[i] == daxdim && validmodeydim[i] == daydim) {
            vk_reset_screen_buffer(daxdim, daydim);
            return 0;
        }
    }
    return -1;
}
