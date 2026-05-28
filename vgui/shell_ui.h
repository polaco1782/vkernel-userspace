#ifndef VGUI_SHELL_UI_H
#define VGUI_SHELL_UI_H

#include "settings_store.h"
#include "vgui_common.h"

namespace vgui {

class ConsoleLog;
class KobjNavigator;
class PanelRegistry;
struct PluginHost;
class TaskManagerPanel;
class VkfmPanel;
class WindowManager;

class ShellUi {
public:
    void initialize(const vk_framebuffer_info_t& framebuffer, ConsoleLog* log = nullptr);

    [[nodiscard]] auto running() const -> bool { return running_; }
    [[nodiscard]] auto drop_to_shell_requested() const -> bool { return drop_to_shell_requested_; }
    void request_quit(ConsoleLog* log = nullptr, vk::string_view message = vk::string_view());
    void request_drop_to_shell(ConsoleLog* log = nullptr, vk::string_view message = vk::string_view());
    void reset_counter(ConsoleLog* log = nullptr, vk::string_view message = vk::string_view());

    void draw(PluginHost& plugin_host,
              PanelRegistry& panel_registry,
              TaskManagerPanel& task_manager,
              KobjNavigator& kobj_navigator,
              VkfmPanel& vkfm_panel);

private:
    [[nodiscard]] auto current_settings_snapshot() const -> PersistedSettings;
    void apply_style();
    void apply_saved_settings(const PersistedSettings& settings);
    void draw_menu_bar(PluginHost& plugin_host, PanelRegistry& panel_registry);
    void draw_info_window(const vk_framebuffer_info_t& framebuffer, WindowManager& window_manager, ConsoleLog& log);
    void draw_settings_window(WindowManager& window_manager, ConsoleLog& log);
    void draw_about_modal();
    void sync_settings(ConsoleLog& log);

    bool running_ = true;
    bool drop_to_shell_requested_ = false;
    bool show_info_ = true;
    bool show_console_ = true;
    bool show_settings_ = false;
    bool show_demo_ = false;
    bool show_task_manager_ = false;
    bool show_kobj_ = false;
    bool show_vkfm_ = false;
    bool open_about_ = false;
    vk_u32 default_app_width_ = 320;
    vk_u32 default_app_height_ = 200;

    int counter_ = 0;
    bool counter_wrap_ = false;
    int counter_max_ = 100;

    int style_index_ = 0;
    float font_scale_ = 1.0f;
    bool transparency_ = false;

    SettingsStore settings_store_;
    PersistedSettings last_saved_settings_ {};
    bool settings_store_ready_ = false;
};

} // namespace vgui

#endif // VGUI_SHELL_UI_H
