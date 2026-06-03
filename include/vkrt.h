/*
 * vkernel userspace - runtime services
 * Copyright (C) 2026 vkernel authors
 *
 * vkrt.h - Stable userspace runtime entrypoints layered over the raw ABI.
 */

#ifndef VKRT_USERSPACE_H
#define VKRT_USERSPACE_H

#include "vk.h"

#ifdef __cplusplus
extern "C" {
#endif

vk_u32 vkrt_api_version(void);

int vkrt_framebuffer_info(vk_framebuffer_info_t* out_info);
int vkrt_poll_framebuffer_event(vk_framebuffer_event_t* out_event);
int vkrt_poll_key(vk_key_event_t* out_event);
int vkrt_poll_mouse(vk_mouse_event_t* out_event);

vk_u64 vkrt_tick_count(void);
vk_u32 vkrt_ticks_per_sec(void);
void vkrt_sleep_ticks(vk_u64 ticks);
void vkrt_yield(void);

char vkrt_getc(void);
char vkrt_try_getc(void);
int vkrt_get_cmdline(char* out, vk_usize out_cap);

vk_file_handle_t vkrt_file_open(const char* path, const char* mode);
vk_usize vkrt_file_read_handle(vk_file_handle_t handle, void* buffer, vk_usize size);
vk_usize vkrt_file_write_handle(vk_file_handle_t handle, const void* buffer, vk_usize size);
int vkrt_file_close(vk_file_handle_t handle);
int vkrt_file_exists(const char* path);
vk_u64 vkrt_file_size(const char* path);
int vkrt_file_remove(const char* path);

vk_i64 vkrt_run_cmdline(const char* command_line);
void vkrt_wait_task(vk_i64 task_id);
int vkrt_terminate_task(vk_u64 task_id);
vk_usize vkrt_task_snapshot(vk_task_info_t* tasks, vk_usize capacity);

int vkrt_kobj_query(const char* path,
                    char* out_value,
                    vk_usize out_value_cap,
                    vk_kobj_node_info_t* out_info);
vk_usize vkrt_kobj_list(const char* path,
                        vk_kobj_child_t* out_items,
                        vk_usize max_items);
int vkrt_kobj_set_value(const char* path, const char* value);
vk_usize vkrt_kobj_rpc_json(const char* req_json, char* out, vk_usize out_cap);
vk_usize vkrt_kobj_rpc_path_json(const char* op,
                                 const char* path,
                                 char* out,
                                 vk_usize out_cap);
int vkrt_kobj_response_ok(const char* json);

int vkrt_snd_mix_queue_play(vk_u32 channel,
                            const void* samples,
                            vk_usize frame_count,
                            vk_u32 format,
                            vk_u32 sample_rate,
                            vk_u32 vol_left,
                            vk_u32 vol_right);
int vkrt_snd_mix_play(vk_u32 channel,
                      const void* samples,
                      vk_usize frame_count,
                      vk_u32 format,
                      vk_u32 sample_rate,
                      vk_u32 vol_left,
                      vk_u32 vol_right);
int vkrt_snd_mix_is_playing(vk_u32 channel);
void vkrt_snd_mix_stop(vk_u32 channel);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* VKRT_USERSPACE_H */
