#ifndef VGUI_WINDOW_MANAGER_H
#define VGUI_WINDOW_MANAGER_H

#include "vgui_common.h"

namespace vgui {

class ConsoleLog;

struct FramebufferDeleter {
    void operator()(vk_u32* ptr) const noexcept;
};

class FramebufferSurface {
public:
    FramebufferSurface() = default;
    FramebufferSurface(FramebufferSurface&&) noexcept = default;
    auto operator=(FramebufferSurface&&) noexcept -> FramebufferSurface& = default;

    FramebufferSurface(const FramebufferSurface&) = delete;
    auto operator=(const FramebufferSurface&) -> FramebufferSurface& = delete;

    [[nodiscard]] auto allocate(vk_u32 width, vk_u32 height) -> bool;
    void reset() noexcept;
    void zero() noexcept;

    [[nodiscard]] auto data() -> vk_u32* { return pixels_.get(); }
    [[nodiscard]] auto data() const -> const vk_u32* { return pixels_.get(); }
    [[nodiscard]] auto bytes() const -> vk_usize { return pixel_count_ * sizeof(vk_u32); }
    [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(pixels_); }

private:
    vk::unique_ptr<vk_u32, FramebufferDeleter> pixels_ { nullptr, FramebufferDeleter {} };
    vk_usize pixel_count_ = 0;
};

class WindowManager {
public:
    explicit WindowManager(ConsoleLog& log) : log_(log) {}

    [[nodiscard]] auto launch_windowed_app(vk::string_view path, vk_u32 width, vk_u32 height) -> vk_i64;
    [[nodiscard]] auto route_key_event(const vk_key_event_t& event) -> bool;
    [[nodiscard]] auto route_mouse_event(const vk_mouse_event_t& event) -> bool;
    void shutdown();

    void draw_windows();
    void clear_focus_if_host_window_focused();

private:
    struct AppWindow {
        bool used = false;
        bool open = false;
        bool close_requested = false;
        bool focus_next = false;
        bool minimized = false;
        bool maximized = false;
        bool restore_pending = false;
        bool restore_bounds_valid = false;
        bool startup_size_handled = false;
        vk_i64 task_id = -1;
        vk_u32 width = 0;
        vk_u32 height = 0;
        ImVec2 restore_pos {};
        ImVec2 restore_size {};
        FramebufferSurface pixels;
        FramebufferSurface snapshot;
        FramebufferSurface verify;
        std::string path;
        std::string title;
    };

    static constexpr int k_max_apps = 6;

    [[nodiscard]] static auto app_task_running(vk_i64 task_id) -> bool;
    [[nodiscard]] static auto app_accepts_framebuffer_resize(vk_i64 task_id) -> bool;
    [[nodiscard]] static auto app_startup_window_size(vk_i64 task_id, vk_u32& width, vk_u32& height) -> bool;
    [[nodiscard]] auto focused_app_index() -> int;
    [[nodiscard]] auto find_free_app_slot() const -> int;
    void request_app_termination(const AppWindow& app);
    void release_app_slot(AppWindow& app);
    void capture_app_snapshot(AppWindow& app);
    [[nodiscard]] auto resize_app_framebuffer(AppWindow& app, vk_u32 width, vk_u32 height) -> bool;
    void remember_restore_bounds(AppWindow& app, const ImVec2& position, const ImVec2& size);
    void toggle_maximize(AppWindow& app, const ImVec2& position, const ImVec2& size);

    ConsoleLog& log_;
    std::array<AppWindow, k_max_apps> apps_ {};
    int focused_app_ = -1;
};

} // namespace vgui

#endif // VGUI_WINDOW_MANAGER_H
