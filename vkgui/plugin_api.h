#ifndef VKGUI_PLUGIN_API_H
#define VKGUI_PLUGIN_API_H

#include "../include/vk.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VKGUI_PLUGIN_HOST_API_VERSION 1ULL
#define VKGUI_PLUGIN_ABI_VERSION 1ULL

typedef struct vkgui_plugin_host_api {
    vk_u64 abi_version;
    const vk_api_t* vk_api;

    void (*set_next_window_pos_first_use)(float x, float y);
    void (*set_next_window_size_first_use)(float width, float height);
    vk_u32 (*begin_window)(const char* title, vk_u32* open);
    void (*end_window)(void);
    void (*clear_focus_if_host_window_focused)(void);
    void (*separator_text)(const char* text);
    void (*text)(const char* text);
    void (*textf)(const char* fmt, ...);
    void (*same_line)(void);
    vk_u32 (*button)(const char* label, float width, float height);
    void (*spacing)(void);
    void (*log)(const char* text);
    void (*logf)(const char* fmt, ...);
} vkgui_plugin_host_api_t;

typedef void (*vkgui_plugin_shutdown_fn)(void* user_data);
typedef void (*vkgui_plugin_draw_window_fn)(const vkgui_plugin_host_api_t* host,
                                           vk_u32* visible,
                                           void* user_data);

typedef struct vkgui_plugin_descriptor {
    vk_u64 abi_version;
    const char* id;
    const char* menu_label;
    vk_u32 default_visible;
    void* user_data;
    vkgui_plugin_shutdown_fn shutdown;
    vkgui_plugin_draw_window_fn draw_window;
} vkgui_plugin_descriptor_t;

typedef int (*vkgui_plugin_init_fn)(const vkgui_plugin_host_api_t* host,
                                   vkgui_plugin_descriptor_t* out_descriptor);

#ifdef __cplusplus
}
#endif

#endif // VKGUI_PLUGIN_API_H
