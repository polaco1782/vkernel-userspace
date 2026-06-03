/*
 * vkernel userspace - shared runtime glue
 * Copyright (C) 2026 vkernel authors
 *
 * This object is linked into every userspace program so musl's syscall
 * bridge and libc++ destructor registration share one stable runtime anchor.
 */

#include "../include/vk.h"
#include "../include/vkrt.h"

void* __dso_handle __attribute__((weak, visibility("hidden"))) = (void*)0;

vk_i64 __vkernel_dispatch_syscall(vk_u64 n,
                                  vk_u64 a1,
                                  vk_u64 a2,
                                  vk_u64 a3,
                                  vk_u64 a4,
                                  vk_u64 a5,
                                  vk_u64 a6)
{
    return vk_syscall(n, a1, a2, a3, a4, a5, a6);
}

vk_u32 vkrt_api_version(void)
{
    const vk_api_t* api = vk_get_api();
    return api != 0 ? api->api_version : 0;
}

int vkrt_framebuffer_info(vk_framebuffer_info_t* out_info)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_framebuffer_info == 0) {
        return 0;
    }
    api->vk_framebuffer_info(out_info);
    return 1;
}

int vkrt_poll_framebuffer_event(vk_framebuffer_event_t* out_event)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_poll_framebuffer_event == 0) {
        return 0;
    }
    return api->vk_poll_framebuffer_event(out_event);
}

int vkrt_poll_key(vk_key_event_t* out_event)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_poll_key == 0) {
        return 0;
    }
    return api->vk_poll_key(out_event);
}

int vkrt_poll_mouse(vk_mouse_event_t* out_event)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_poll_mouse == 0) {
        return 0;
    }
    return api->vk_poll_mouse(out_event);
}

vk_u64 vkrt_tick_count(void)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_tick_count == 0) {
        return 0;
    }
    return api->vk_tick_count();
}

vk_u32 vkrt_ticks_per_sec(void)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_ticks_per_sec == 0) {
        return 0;
    }
    return api->vk_ticks_per_sec();
}

void vkrt_sleep_ticks(vk_u64 ticks)
{
    const vk_api_t* api = vk_get_api();
    if (api != 0 && api->vk_sleep != 0) {
        api->vk_sleep(ticks);
    }
}

void vkrt_yield(void)
{
    const vk_api_t* api = vk_get_api();
    if (api != 0 && api->vk_yield != 0) {
        api->vk_yield();
    }
}

char vkrt_getc(void)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_getc == 0) {
        return 0;
    }
    return api->vk_getc();
}

char vkrt_try_getc(void)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_try_getc == 0) {
        return 0;
    }
    return api->vk_try_getc();
}

int vkrt_get_cmdline(char* out, vk_usize out_cap)
{
    const vk_api_t* api = vk_get_api();
    if (out != 0 && out_cap > 0) {
        out[0] = '\0';
    }
    if (api == 0 || api->vk_get_cmdline == 0 || out == 0 || out_cap == 0) {
        return 0;
    }
    api->vk_get_cmdline(out, out_cap);
    return 1;
}

vk_file_handle_t vkrt_file_open(const char* path, const char* mode)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_file_open == 0) {
        return (vk_file_handle_t)0;
    }
    return api->vk_file_open(path, mode);
}

vk_usize vkrt_file_read_handle(vk_file_handle_t handle, void* buffer, vk_usize size)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_file_read_handle == 0) {
        return 0;
    }
    return api->vk_file_read_handle(handle, buffer, size);
}

vk_usize vkrt_file_write_handle(vk_file_handle_t handle, const void* buffer, vk_usize size)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_file_write_handle == 0) {
        return 0;
    }
    return api->vk_file_write_handle(handle, buffer, size);
}

int vkrt_file_close(vk_file_handle_t handle)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_file_close == 0) {
        return -1;
    }
    return api->vk_file_close(handle);
}

int vkrt_file_exists(const char* path)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_file_exists == 0) {
        return 0;
    }
    return api->vk_file_exists(path);
}

vk_u64 vkrt_file_size(const char* path)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_file_size == 0) {
        return 0;
    }
    return api->vk_file_size(path);
}

int vkrt_file_remove(const char* path)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_file_remove == 0) {
        return -1;
    }
    return api->vk_file_remove(path);
}

vk_i64 vkrt_run_cmdline(const char* command_line)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_run_cmdline == 0) {
        return -1;
    }
    return api->vk_run_cmdline(command_line);
}

void vkrt_wait_task(vk_i64 task_id)
{
    vk_wait_task(task_id);
}

int vkrt_terminate_task(vk_u64 task_id)
{
    return vk_terminate_task(task_id);
}

vk_usize vkrt_task_snapshot(vk_task_info_t* tasks, vk_usize capacity)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_task_snapshot == 0) {
        return 0;
    }
    return api->vk_task_snapshot(tasks, capacity);
}

int vkrt_kobj_query(const char* path,
                    char* out_value,
                    vk_usize out_value_cap,
                    vk_kobj_node_info_t* out_info)
{
    return vk_kobj_query(path, out_value, out_value_cap, out_info);
}

vk_usize vkrt_kobj_list(const char* path,
                        vk_kobj_child_t* out_items,
                        vk_usize max_items)
{
    return vk_kobj_list(path, out_items, max_items);
}

int vkrt_kobj_set_value(const char* path, const char* value)
{
    return vk_kobj_set_value(path, value);
}

vk_usize vkrt_kobj_rpc_json(const char* req_json, char* out, vk_usize out_cap)
{
    return vk_kobj_rpc_json(req_json, out, out_cap);
}

vk_usize vkrt_kobj_rpc_path_json(const char* op,
                                 const char* path,
                                 char* out,
                                 vk_usize out_cap)
{
    return vk_kobj_rpc_path_json(op, path, out, out_cap);
}

int vkrt_kobj_response_ok(const char* json)
{
    return vk_kobj_response_ok(json);
}

int vkrt_snd_mix_queue_play(vk_u32 channel,
                            const void* samples,
                            vk_usize frame_count,
                            vk_u32 format,
                            vk_u32 sample_rate,
                            vk_u32 vol_left,
                            vk_u32 vol_right)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_snd_mix_queue_play == 0) {
        return 0;
    }
    return api->vk_snd_mix_queue_play(channel,
                                      samples,
                                      frame_count,
                                      format,
                                      sample_rate,
                                      vol_left,
                                      vol_right);
}

int vkrt_snd_mix_play(vk_u32 channel,
                      const void* samples,
                      vk_usize frame_count,
                      vk_u32 format,
                      vk_u32 sample_rate,
                      vk_u32 vol_left,
                      vk_u32 vol_right)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_snd_mix_play == 0) {
        return 0;
    }
    return api->vk_snd_mix_play(channel,
                                samples,
                                frame_count,
                                format,
                                sample_rate,
                                vol_left,
                                vol_right);
}

int vkrt_snd_mix_is_playing(vk_u32 channel)
{
    const vk_api_t* api = vk_get_api();
    if (api == 0 || api->vk_snd_mix_is_playing == 0) {
        return 0;
    }
    return api->vk_snd_mix_is_playing(channel);
}

void vkrt_snd_mix_stop(vk_u32 channel)
{
    const vk_api_t* api = vk_get_api();
    if (api != 0 && api->vk_snd_mix_stop != 0) {
        api->vk_snd_mix_stop(channel);
    }
}
