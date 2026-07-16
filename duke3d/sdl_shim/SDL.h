#ifndef VK_DUKE_SDL_H
#define VK_DUKE_SDL_H

#include <stdint.h>
#include <stdlib.h>

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int32_t Sint32;

typedef struct SDL_Surface {
    int w;
    int h;
    int pitch;
    void *pixels;
} SDL_Surface;

typedef struct SDL_Rect {
    int x;
    int y;
    int w;
    int h;
} SDL_Rect;

typedef struct SDL_Color {
    Uint8 r;
    Uint8 g;
    Uint8 b;
    Uint8 unused;
} SDL_Color;

typedef struct SDL_mutex {
    int placeholder;
} SDL_mutex;

typedef enum SDL_GrabMode {
    SDL_GRAB_QUERY = -1,
    SDL_GRAB_OFF = 0,
    SDL_GRAB_ON = 1,
} SDL_GrabMode;

enum {
    SDL_INIT_VIDEO = 0x00000020u,
    SDL_INIT_AUDIO = 0x00000010u,
    SDL_INIT_NOPARACHUTE = 0x00100000u,
};

extern SDL_Surface *surface;

int vk_sdl_get_grab_input(void);
void vk_sdl_set_grab_input(int enabled);
void vk_sdl_set_cursor_visible(int visible);
void vk_sdl_quit(void);

static inline SDL_GrabMode SDL_WM_GrabInput(SDL_GrabMode mode)
{
    if (mode == SDL_GRAB_QUERY) {
        return vk_sdl_get_grab_input() ? SDL_GRAB_ON : SDL_GRAB_OFF;
    }

    vk_sdl_set_grab_input(mode == SDL_GRAB_ON);
    return mode;
}

static inline int SDL_ShowCursor(int toggle)
{
    vk_sdl_set_cursor_visible(toggle != 0);
    return 0;
}

static inline int SDL_InitSubSystem(Uint32 flags)
{
    (void)flags;
    return 0;
}

static inline void SDL_QuitSubSystem(Uint32 flags)
{
    (void)flags;
}

static inline void SDL_Quit(void)
{
    vk_sdl_quit();
}

static inline SDL_mutex *SDL_CreateMutex(void)
{
    return (SDL_mutex *)calloc(1, sizeof(SDL_mutex));
}

static inline void SDL_DestroyMutex(SDL_mutex *mutex)
{
    free(mutex);
}

static inline int SDL_mutexP(SDL_mutex *mutex)
{
    (void)mutex;
    return 0;
}

static inline int SDL_mutexV(SDL_mutex *mutex)
{
    (void)mutex;
    return 0;
}

#endif
