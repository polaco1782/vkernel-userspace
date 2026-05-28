#include "window_manager.h"

#include "console_log.h"
#include "imgui/imgui_internal.h"

#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <utility>

namespace vgui {

namespace {

constexpr vk_usize k_framebuffer_alignment = 4096;
constexpr vk_u32 k_min_app_width = 320;
constexpr vk_u32 k_min_app_height = 240;
constexpr float k_window_chrome_width = 40.0f;
constexpr float k_window_chrome_height = 80.0f;

auto query_app_task(vk_i64 task_id, vk_task_info_t* out = nullptr) -> bool
{
    if (task_id < 0) {
        if (out != nullptr) {
            *out = {};
        }
        return false;
    }

    std::array<vk_task_info_t, 64> tasks {};
    const vk_usize total = VK_CALL(task_snapshot, tasks.data(), tasks.size());
    const vk_usize count = total < tasks.size() ? total : tasks.size();
    for (vk_usize index = 0; index < count; ++index) {
        if (static_cast<vk_i64>(tasks[index].id) != task_id) {
            continue;
        }

        if (out != nullptr) {
            *out = tasks[index];
        }
        return true;
    }

    if (out != nullptr) {
        *out = {};
    }
    return false;
}

} // namespace

void FramebufferDeleter::operator()(vk_u32* ptr) const noexcept
{
    if (ptr != nullptr) {
        free(ptr);
    }
}

auto FramebufferSurface::allocate(vk_u32 width, vk_u32 height) -> bool
{
    const vk_usize pixel_count = static_cast<vk_usize>(width) * static_cast<vk_usize>(height);
    if (pixel_count == 0) {
        return false;
    }

    const vk_usize bytes = pixel_count * sizeof(vk_u32);
    const vk_usize aligned_bytes =
        (bytes + k_framebuffer_alignment - 1) & ~(k_framebuffer_alignment - 1);
    auto* pixels = static_cast<vk_u32*>(aligned_alloc(k_framebuffer_alignment, aligned_bytes));
    if (pixels == nullptr) {
        return false;
    }

    reset();
    pixels_.reset(pixels);
    pixel_count_ = pixel_count;
    zero();
    return true;
}

void FramebufferSurface::reset() noexcept
{
    pixels_.reset();
    pixel_count_ = 0;
}

void FramebufferSurface::zero() noexcept
{
    if (pixels_ && pixel_count_ != 0) {
        memset(pixels_.get(), 0, bytes());
    }
}

auto WindowManager::app_task_running(vk_i64 task_id) -> bool
{
    vk_task_info_t task = {};
    return query_app_task(task_id, &task) && task.state != 3u;
}

auto WindowManager::app_accepts_framebuffer_resize(vk_i64 task_id) -> bool
{
    if (task_id < 0 || !vk_get_api()->vk_task_accepts_framebuffer_resize) {
        return false;
    }

    return vk_get_api()->vk_task_accepts_framebuffer_resize(static_cast<vk_u64>(task_id)) != 0;
}

auto WindowManager::app_startup_window_size(vk_i64 task_id, vk_u32& width, vk_u32& height) -> bool
{
    width = 0;
    height = 0;

    if (task_id < 0 || !vk_get_api()->vk_get_task_startup_window_size) {
        return false;
    }

    return vk_get_api()->vk_get_task_startup_window_size(static_cast<vk_u64>(task_id),
                                                         &width,
                                                         &height) != 0
        && width != 0
        && height != 0;
}

auto WindowManager::focused_app_index() -> int
{
    if (focused_app_ < 0 || focused_app_ >= k_max_apps) {
        return -1;
    }

    AppWindow& app = apps_[focused_app_];
    if (!app.used || !app.open || !app_task_running(app.task_id)) {
        focused_app_ = -1;
        return -1;
    }

    return focused_app_;
}

void WindowManager::clear_focus_if_host_window_focused()
{
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        focused_app_ = -1;
    }
}

auto WindowManager::route_key_event(const vk_key_event_t& event) -> bool
{
    const int focused = focused_app_index();
    if (focused < 0 || !vk_get_api()->vk_send_key) {
        return false;
    }

    (void)vk_get_api()->vk_send_key(static_cast<vk_u64>(apps_[focused].task_id), &event);
    return true;
}

auto WindowManager::route_mouse_event(const vk_mouse_event_t& event) -> bool
{
    const int focused = focused_app_index();
    if (focused < 0 || !vk_get_api()->vk_send_mouse) {
        return false;
    }

    (void)vk_get_api()->vk_send_mouse(static_cast<vk_u64>(apps_[focused].task_id), &event);
    return true;
}

auto WindowManager::find_free_app_slot() const -> int
{
    for (int index = 0; index < k_max_apps; ++index) {
        if (!apps_[index].used) {
            return index;
        }
    }

    return -1;
}

void WindowManager::request_app_termination(const AppWindow& app)
{
    if (app.task_id < 0 || !app_task_running(app.task_id)) {
        return;
    }

    if (vk_get_api()->vk_terminate_task(static_cast<vk_u64>(app.task_id))) {
        log_.addf("App task %lld termination requested.", static_cast<long long>(app.task_id));
    } else {
        log_.addf("Failed to terminate app task %lld.", static_cast<long long>(app.task_id));
    }
}

void WindowManager::release_app_slot(AppWindow& app)
{
    app.pixels.reset();
    app.snapshot.reset();
    app.verify.reset();
    app.used = false;
    app.open = false;
    app.close_requested = false;
    app.focus_next = false;
    app.minimized = false;
    app.maximized = false;
    app.restore_pending = false;
    app.restore_bounds_valid = false;
    app.startup_size_handled = false;
    app.task_id = -1;
    app.width = 0;
    app.height = 0;
    app.restore_pos = ImVec2(0.0f, 0.0f);
    app.restore_size = ImVec2(0.0f, 0.0f);
    app.path.clear();
    app.title.clear();
}

void WindowManager::capture_app_snapshot(AppWindow& app)
{
    if (!app.pixels || !app.snapshot || !app.verify || app.pixels.bytes() == 0) {
        return;
    }

    for (int attempt = 0; attempt < 3; ++attempt) {
        memcpy(app.verify.data(), app.pixels.data(), app.pixels.bytes());
        if (memcmp(app.verify.data(), app.pixels.data(), app.pixels.bytes()) == 0) {
            memcpy(app.snapshot.data(), app.verify.data(), app.verify.bytes());
            return;
        }
    }
}

auto WindowManager::resize_app_framebuffer(AppWindow& app, vk_u32 width, vk_u32 height) -> bool
{
    if (width == 0 || height == 0) {
        return false;
    }

    if (app.width == width && app.height == height) {
        return true;
    }

    vk_task_info_t task = {};
    if (!query_app_task(app.task_id, &task) || task.state == 3u) {
        return false;
    }

    /* Retry once the app yields so we do not swap its shared mapping mid-blit. */
    if (task.state == 1u) {
        return false;
    }

    FramebufferSurface pixels;
    FramebufferSurface snapshot;
    FramebufferSurface verify;
    if (!pixels.allocate(width, height)
        || !snapshot.allocate(width, height)
        || !verify.allocate(width, height)) {
        log_.addf("Failed to resize app task %lld surface to %ux%u.",
                  static_cast<long long>(app.task_id),
                  width,
                  height);
        return false;
    }

    vk_framebuffer_info_t framebuffer = {};
    framebuffer.base = static_cast<vk_u64>(reinterpret_cast<vk_usize>(pixels.data()));
    framebuffer.width = width;
    framebuffer.height = height;
    framebuffer.stride = width;
    framebuffer.format = VK_PIXEL_FORMAT_BGRX_8BPP;
    framebuffer.valid = 1u;

    if(!vk_get_api()->vk_set_task_framebuffer(static_cast<vk_u64>(app.task_id), &framebuffer)) {
        log_.addf("Failed to remap framebuffer for app task %lld.",
                  static_cast<long long>(app.task_id));
        return false;
    }

    if (vk_get_api()->vk_set_compositor_default_fb) {
        (void)vk_get_api()->vk_set_compositor_default_fb(&framebuffer);
    }

    app.pixels = std::move(pixels);
    app.snapshot = std::move(snapshot);
    app.verify = std::move(verify);
    app.width = width;
    app.height = height;
    return true;
}

void WindowManager::remember_restore_bounds(AppWindow& app, const ImVec2& position, const ImVec2& size)
{
    if (size.x <= 0.0f || size.y <= 0.0f) {
        return;
    }

    app.restore_pos = position;
    app.restore_size = size;
    app.restore_bounds_valid = true;
}

void WindowManager::toggle_maximize(AppWindow& app, const ImVec2& position, const ImVec2& size)
{
    if (!app.maximized) {
        remember_restore_bounds(app, position, size);
        app.maximized = true;
    } else {
        app.maximized = false;
        app.restore_pending = app.restore_bounds_valid;
    }

    app.focus_next = true;
}

auto WindowManager::launch_windowed_app(vk::string_view path, vk_u32 width, vk_u32 height) -> vk_i64
{
    const int slot = find_free_app_slot();
    if (slot < 0) {
        log_.add("No free app window slots.");
        return -1;
    }

    AppWindow& app = apps_[slot];
    release_app_slot(app);
    app.width = width;
    app.height = height;
    if (!app.pixels.allocate(width, height)
        || !app.snapshot.allocate(width, height)
        || !app.verify.allocate(width, height)) {
        release_app_slot(app);
        log_.add("Failed to allocate app surface.");
        return -1;
    }

    vk_framebuffer_info_t framebuffer = {};
    framebuffer.base = static_cast<vk_u64>(reinterpret_cast<vk_usize>(app.pixels.data()));
    framebuffer.width = width;
    framebuffer.height = height;
    framebuffer.stride = width;
    framebuffer.format = VK_PIXEL_FORMAT_BGRX_8BPP;
    framebuffer.valid = 1u;

    const std::string path_string = string_from_view(path);
    const vk_i64 task = vk_get_api()->vk_run_with_fb(path_string.c_str(), &framebuffer);
    if (task < 0) {
        release_app_slot(app);
        log_.addf("Failed to start app: %s", path_string.c_str());
        return -1;
    }

    app.used = true;
    app.open = true;
    app.focus_next = true;
    app.task_id = task;
    app.path = path_string;

    std::array<char, 160> title {};
    snprintf(title.data(), title.size(), "App: %s##%lld", path_string.c_str(), static_cast<long long>(task));
    app.title = title.data();
    focused_app_ = slot;

    if (vk_get_api()->vk_set_compositor_default_fb) {
        (void)vk_get_api()->vk_set_compositor_default_fb(&framebuffer);
    }

    log_.addf("App started: %s (task %lld).", path_string.c_str(), static_cast<long long>(task));
    return task;
}

void WindowManager::shutdown()
{
    bool waiting_for_apps = false;
    for (int index = 0; index < k_max_apps; ++index) {
        AppWindow& app = apps_[index];
        if (!app.used) {
            continue;
        }

        if (app_task_running(app.task_id)) {
            if (!app.close_requested) {
                request_app_termination(app);
                app.close_requested = true;
            }
            waiting_for_apps = true;
        }
    }

    while (waiting_for_apps) {
        waiting_for_apps = false;
        for (int index = 0; index < k_max_apps; ++index) {
            AppWindow& app = apps_[index];
            if (app.used && app_task_running(app.task_id)) {
                waiting_for_apps = true;
                break;
            }
        }

        if (waiting_for_apps) {
            VK_CALL(sleep, 1);
        }
    }

    for (int index = 0; index < k_max_apps; ++index) {
        AppWindow& app = apps_[index];
        if (app.used) {
            release_app_slot(app);
        }
    }

    focused_app_ = -1;
    if (vk_get_api()->vk_set_compositor_default_fb) {
        (void)vk_get_api()->vk_set_compositor_default_fb(nullptr);
    }
}

enum class TitleBarButtonIcon {
    Minimize,
    Maximize,
    Restore,
};

static auto draw_title_bar_symbol_button(ImGuiWindow* window,
                                         const ImRect& title_bar_rect,
                                         const char* id_suffix,
                                         const ImVec2& pos,
                                         TitleBarButtonIcon icon) -> bool
{
    ImGuiContext& g = *GImGui;
    const ImRect bb(pos, pos + ImVec2(g.FontSize, g.FontSize));

    const ImGuiItemFlags item_flags_backup = g.CurrentItemFlags;
    const ImGuiNavLayer nav_layer_backup = window->DC.NavLayerCurrent;
    g.CurrentItemFlags |= ImGuiItemFlags_NoNavDefaultFocus;
    window->DC.NavLayerCurrent = ImGuiNavLayer_Menu;

    ImGui::PushClipRect(title_bar_rect.Min, title_bar_rect.Max, false);
    const bool is_clipped = !ImGui::ItemAdd(bb, window->GetID(id_suffix));
    bool hovered = false;
    bool held = false;
    const bool pressed = ImGui::ButtonBehavior(bb,
                                               window->GetID(id_suffix),
                                               &hovered,
                                               &held,
                                               ImGuiButtonFlags_NoNavFocus);
    if (!is_clipped) {
        if (hovered || held) {
            const ImU32 bg_col = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered);
            window->DrawList->AddRectFilled(bb.Min, bb.Max, bg_col);
        }

        ImGui::RenderNavCursor(bb, window->GetID(id_suffix), ImGuiNavRenderCursorFlags_Compact);

        const ImU32 line_col = imgui_title_should_use_light_text()
            ? IM_COL32(255, 255, 255, 255)
            : ImGui::GetColorU32(ImGuiCol_Text);
        if (icon == TitleBarButtonIcon::Minimize) {
            const float y = bb.Max.y - 3.0f;
            window->DrawList->AddLine(ImVec2(bb.Min.x + 2.0f, y),
                                      ImVec2(bb.Max.x - 2.0f, y),
                                      line_col,
                                      1.0f);
        } else if (icon == TitleBarButtonIcon::Maximize) {
            window->DrawList->AddRect(ImVec2(bb.Min.x + 2.0f, bb.Min.y + 2.0f),
                                      ImVec2(bb.Max.x - 2.0f, bb.Max.y - 2.0f),
                                      line_col);
        } else {
            const ImVec2 back_min(bb.Min.x + 2.0f, bb.Min.y + 4.0f);
            const ImVec2 back_max(bb.Max.x - 3.0f, bb.Max.y - 1.0f);
            const ImVec2 front_min(bb.Min.x + 4.0f, bb.Min.y + 2.0f);
            const ImVec2 front_max(bb.Max.x - 1.0f, bb.Max.y - 3.0f);
            window->DrawList->AddRect(back_min, back_max, line_col);
            window->DrawList->AddRect(front_min, front_max, line_col);
        }
    }
    ImGui::PopClipRect();

    window->DC.NavLayerCurrent = nav_layer_backup;
    g.CurrentItemFlags = item_flags_backup;
    return pressed;
}

static void draw_title_bar_window_controls(bool maximized,
                                           bool& minimize_requested,
                                           bool& maximize_toggled)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window == nullptr) {
        return;
    }

    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = ImGui::GetStyle();
    const ImRect title_bar_rect = window->TitleBarRect();
    const float button_size = g.FontSize;
    const float button_y = title_bar_rect.Min.y + style.FramePadding.y;
    const float button_gap = style.ItemInnerSpacing.x;
    const float close_slot_left =
        title_bar_rect.Max.x - style.FramePadding.x - button_size - button_gap;
    const float maximize_x = close_slot_left - button_size;
    const float minimize_x = maximize_x - button_gap - button_size;

    if (draw_title_bar_symbol_button(window,
                                     title_bar_rect,
                                     "##title_minimize",
                                     ImVec2(minimize_x, button_y),
                                     TitleBarButtonIcon::Minimize)) {
        minimize_requested = true;
    }

    if (draw_title_bar_symbol_button(window,
                                     title_bar_rect,
                                     maximized ? "##title_restore" : "##title_maximize",
                                     ImVec2(maximize_x, button_y),
                                     maximized ? TitleBarButtonIcon::Restore : TitleBarButtonIcon::Maximize)) {
        maximize_toggled = true;
    }
}

void WindowManager::draw_windows()
{
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    const float workspace_top = ImGui::GetFrameHeight();

    for (int index = 0; index < k_max_apps; ++index) {
        AppWindow& app = apps_[index];
        if (!app.used) {
            continue;
        }

        const bool running = app_task_running(app.task_id);
        if (!running) {
            app.open = false;
            app.close_requested = true;
        }

        if (!app.open) {
            if (running) {
                if (!app.close_requested) {
                    request_app_termination(app);
                    app.close_requested = true;
                }
                continue;
            }

            /* The task can disappear from snapshots before the scheduler
             * finishes taking it off-CPU. Wait for that handoff before we
             * recycle the shared framebuffer back into vgui's heap. */
            vk_wait_task(app.task_id);
            if (focused_app_ == index) {
                focused_app_ = -1;
            }
            release_app_slot(app);
            continue;
        }

        const bool app_accepts_resize = app_accepts_framebuffer_resize(app.task_id);
        vk_u32 startup_width = app.width;
        vk_u32 startup_height = app.height;
        ImGuiCond startup_size_condition = ImGuiCond_FirstUseEver;

        if (!app.startup_size_handled) {
            vk_u32 requested_width = 0;
            vk_u32 requested_height = 0;
            if (app_startup_window_size(app.task_id, requested_width, requested_height)) {
                if (app_accepts_resize) {
                    if (requested_width < k_min_app_width) {
                        requested_width = k_min_app_width;
                    }
                    if (requested_height < k_min_app_height) {
                        requested_height = k_min_app_height;
                    }

                    if (resize_app_framebuffer(app, requested_width, requested_height)) {
                        startup_width = app.width;
                        startup_height = app.height;
                    } else {
                        startup_width = requested_width;
                        startup_height = requested_height;
                    }
                } else {
                    startup_width = requested_width;
                    startup_height = requested_height;
                }

                startup_size_condition = ImGuiCond_Always;
                app.startup_size_handled = true;
            }
        }

        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(startup_width) + k_window_chrome_width,
                                        static_cast<float>(startup_height) + k_window_chrome_height),
                                 startup_size_condition);
        ImGui::SetNextWindowSizeConstraints(
            ImVec2((app_accepts_resize ? static_cast<float>(k_min_app_width) : 1.0f) + k_window_chrome_width,
                   (app_accepts_resize ? static_cast<float>(k_min_app_height) : 1.0f) + k_window_chrome_height),
            ImVec2(FLT_MAX, FLT_MAX));
        if (app.maximized) {
            const float workspace_height =
                display_size.y > workspace_top ? display_size.y - workspace_top : 1.0f;
            ImGui::SetNextWindowPos(ImVec2(0.0f, workspace_top), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(display_size.x, workspace_height), ImGuiCond_Always);
            ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
        } else if (app.restore_pending && app.restore_bounds_valid) {
            ImGui::SetNextWindowPos(app.restore_pos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(app.restore_size, ImGuiCond_Always);
            app.restore_pending = false;
        }
        if (app.focus_next) {
            ImGui::SetNextWindowFocus();
            app.focus_next = false;
        }

        const bool draw_contents = imgui_begin_window_readable_caption(app.title.c_str(), &app.open);
        bool minimize_requested = false;
        bool maximize_toggled = false;
        if (draw_contents) {
            draw_title_bar_window_controls(app.maximized, minimize_requested, maximize_toggled);
        }

        if (draw_contents) {
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
                focused_app_ = index;
            }

            if (!app.maximized && !ImGui::IsWindowCollapsed()) {
                remember_restore_bounds(app, ImGui::GetWindowPos(), ImGui::GetWindowSize());
            }

            if (maximize_toggled) {
                app.minimized = false;
                toggle_maximize(app, ImGui::GetWindowPos(), ImGui::GetWindowSize());
            }

            if (minimize_requested) {
                app.minimized = true;
                if (!app.maximized) {
                    remember_restore_bounds(app, ImGui::GetWindowPos(), ImGui::GetWindowSize());
                }
                ImGui::SetWindowCollapsed(true, ImGuiCond_Always);
                if (focused_app_ == index) {
                    focused_app_ = -1;
                }
            }

            if (minimize_requested) {
                ImGui::End();
                continue;
            }

            const ImVec2 available = ImGui::GetContentRegionAvail();
            float draw_width = available.x > 1.0f ? available.x : 1.0f;
            float draw_height = available.y > 1.0f ? available.y : 1.0f;

            if (app_accepts_resize) {
                vk_u32 target_width = static_cast<vk_u32>(draw_width);
                vk_u32 target_height = static_cast<vk_u32>(draw_height);

                if (target_width < k_min_app_width) {
                    target_width = k_min_app_width;
                }
                if (target_height < k_min_app_height) {
                    target_height = k_min_app_height;
                }

                if (!resize_app_framebuffer(app, target_width, target_height)) {
                    draw_width = static_cast<float>(app.width);
                    draw_height = static_cast<float>(app.height);
                } else {
                    draw_width = static_cast<float>(app.width);
                    draw_height = static_cast<float>(app.height);
                }
            }

            ImGui::InvisibleButton("##app_canvas",
                                   ImVec2(draw_width, draw_height));

            const ImVec2 p_min = ImGui::GetItemRectMin();
            const ImVec2 p_max = ImGui::GetItemRectMax();

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRectFilled(p_min, p_max, IM_COL32(8, 8, 8, 255));
            capture_app_snapshot(app);

            ImGui_ImplVK_FramebufferImage image = {};
            image.pixels = app.snapshot.data();
            image.width = app.width;
            image.height = app.height;
            image.stride = app.width;
            image.format = VK_PIXEL_FORMAT_BGRX_8BPP;
            image.p_min = p_min;
            image.p_max = p_max;
            ImGui_ImplVK_AddFramebufferImage(draw_list, &image);
            draw_list->AddRect(p_min, p_max, IM_COL32(180, 180, 180, 255));
        }

        ImGui::End();

        if (!app.open) {
            if (!app.close_requested) {
                request_app_termination(app);
                app.close_requested = true;
            }
        }
    }
}

} // namespace vgui
