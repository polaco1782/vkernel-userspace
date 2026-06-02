#include "shell_ui.h"

#include "console_log.h"
#include "ImGuiNotify.hpp"
#include "kobj_panel.h"
#include "launch_registry.h"
#include "plugin_registry.h"
#include "task_manager_panel.h"
#include "text_editor_panel.h"
#include "vkfm_panel.h"
#include "window_manager.h"

#include <stdio.h>

namespace vkgui {

namespace {
auto theme_name_getter(void* data, int index, const char** out_text) -> bool
{
    if (out_text == nullptr) {
        return false;
    }

    const auto* catalog = static_cast<const ThemeCatalog*>(data);
    if (catalog == nullptr || index < 0 || index >= catalog->count) {
        return false;
    }

    *out_text = catalog->schemes[index].name.c_str();
    return true;
}

auto clamp_theme_color_byte(float value) -> int
{
    if (value <= 0.0f) {
        return 0;
    }
    if (value >= 1.0f) {
        return 255;
    }
    return static_cast<int>(value * 255.0f + 0.5f);
}

void push_notification(ImGuiToastType type, int dismiss_time_ms, const std::string& message)
{
    ImGuiToast toast(type, dismiss_time_ms);
    toast.setContent("%s", message.c_str());
    ImGui::InsertNotification(toast);
}
} // namespace

void ShellUi::initialize(const vk_framebuffer_info_t& framebuffer, ConsoleLog* log)
{
    default_app_width_ = framebuffer.width / 2;
    default_app_height_ = framebuffer.height / 2;
    if (default_app_width_ < 320) {
        default_app_width_ = 320;
    }
    if (default_app_height_ < 200) {
        default_app_height_ = 200;
    }

    theme_catalog_ = builtin_theme_catalog();

    if (settings_store_.open("/data/vkgui/vkgui_settings.db")) {
        if (settings_store_.seeded_default_themes() && log != nullptr) {
            log->add("vkGUI settings: seeded default color schemes into /data/vkgui/vkgui_settings.db.");
        }

        ThemeCatalog loaded_themes;
        if (settings_store_.load_theme_catalog(loaded_themes)) {
            if (loaded_themes.count > 0) {
                theme_catalog_ = loaded_themes;
            } else if (log != nullptr) {
                log->add("vkGUI settings: theme catalog was empty; using built-in defaults.");
                push_notification(ImGuiToastType::Warning, 5000, "Theme catalog was empty. Using built-in defaults.");
            }
        } else if (log != nullptr) {
            log->addf("vkGUI settings: failed to load color schemes from /data/vkgui/vkgui_settings.db (%s); using built-in defaults.",
                      settings_store_.last_error().c_str());
            push_notification(ImGuiToastType::Warning, 5000, "Failed to load theme catalog. Using built-in defaults.");
        }

        PersistedSettings settings = current_settings_snapshot();
        if (settings_store_.load(settings)) {
            apply_saved_settings(settings);
            last_saved_settings_ = settings;
            settings_store_ready_ = true;
            if (log != nullptr) {
                log->add("vkGUI settings: loaded saved settings from /data/vkgui/vkgui_settings.db.");
            }
        } else if (log != nullptr) {
            log->addf("vkGUI settings: failed to load /data/vkgui/vkgui_settings.db (%s).",
                      settings_store_.last_error().c_str());
            push_notification(ImGuiToastType::Warning, 5000, "Failed to load saved settings. Using current defaults.");
        }
    } else if (log != nullptr) {
        log->addf("vkGUI settings: failed to open /data/vkgui/vkgui_settings.db (%s).",
                  settings_store_.last_error().c_str());
        push_notification(ImGuiToastType::Error, 0, "Settings database could not be opened.");
    }

    ImGui_ImplVK_SetTransparencyEnabled(transparency_);
    apply_style();
    ImGui::GetIO().FontGlobalScale = font_scale_;
    load_theme_editor_from_selected_scheme();
}

void ShellUi::request_quit(ConsoleLog* log, vk::string_view message)
{
    if (log != nullptr && !message.empty()) {
        log->add(message);
    }
    drop_to_shell_requested_ = false;
    running_ = false;
}

void ShellUi::request_drop_to_shell(ConsoleLog* log, vk::string_view message)
{
    if (log != nullptr && !message.empty()) {
        log->add(message);
    }
    drop_to_shell_requested_ = true;
    running_ = false;
}

void ShellUi::reset_counter(ConsoleLog* log, vk::string_view message)
{
    counter_ = 0;
    if (log != nullptr && !message.empty()) {
        log->add(message);
    }
}

auto ShellUi::current_settings_snapshot() const -> PersistedSettings
{
    PersistedSettings settings;
    settings.style_index = style_index_;
    settings.font_scale = font_scale_;
    settings.transparency = transparency_;
    settings.show_info = show_info_;
    settings.show_console = show_console_;
    settings.show_task_manager = show_task_manager_;
    settings.show_kobj = show_kobj_;
    settings.show_vkfm = show_vkfm_;
    settings.show_text_editor = show_text_editor_;
    return settings;
}

void ShellUi::apply_saved_settings(const PersistedSettings& settings)
{
    style_index_ = settings.style_index;
    const int max_style_index = theme_catalog_.count > 0 ? theme_catalog_.count - 1 : 0;
    if (style_index_ < 0 || style_index_ > max_style_index) {
        style_index_ = 0;
    }

    font_scale_ = settings.font_scale;
    if (font_scale_ < 0.5f) {
        font_scale_ = 0.5f;
    } else if (font_scale_ > 2.0f) {
        font_scale_ = 2.0f;
    }

    transparency_ = settings.transparency;
    show_info_ = settings.show_info;
    show_console_ = settings.show_console;
    show_task_manager_ = settings.show_task_manager;
    show_kobj_ = settings.show_kobj;
    show_vkfm_ = settings.show_vkfm;
    show_text_editor_ = settings.show_text_editor;
}

void ShellUi::apply_style()
{
    if (theme_catalog_.count <= 0) {
        ImGui::StyleColorsDark();
        ::ImGui_ImplVK_SetClearColor(22, 22, 30);
    } else {
        if (style_index_ < 0 || style_index_ >= theme_catalog_.count) {
            style_index_ = 0;
        }
        apply_theme_scheme(theme_catalog_.schemes[style_index_]);
    }

    if (!transparency_) {
        ImGuiStyle& style = ImGui::GetStyle();
        for (int index = 0; index < ImGuiCol_COUNT; ++index) {
            style.Colors[index].w = 1.0f;
        }
    }
}

void ShellUi::load_theme_editor_from_selected_scheme()
{
    if (theme_catalog_.count <= 0) {
        return;
    }
    if (style_index_ < 0 || style_index_ >= theme_catalog_.count) {
        style_index_ = 0;
    }

    const ThemeScheme& scheme = theme_catalog_.schemes[style_index_];
    theme_editor_clear_color_[0] = static_cast<float>(scheme.clear_r) / 255.0f;
    theme_editor_clear_color_[1] = static_cast<float>(scheme.clear_g) / 255.0f;
    theme_editor_clear_color_[2] = static_cast<float>(scheme.clear_b) / 255.0f;
    theme_editor_reference_style_ = ImGui::GetStyle();
}

void ShellUi::discard_theme_editor_changes()
{
    if (theme_catalog_.count <= 0) {
        return;
    }

    apply_style();
    load_theme_editor_from_selected_scheme();
}

void ShellUi::save_current_theme(ConsoleLog& log)
{
    if (theme_catalog_.count <= 0 || style_index_ < 0 || style_index_ >= theme_catalog_.count) {
        return;
    }

    ThemeCatalog updated_catalog = theme_catalog_;
    const ThemeScheme& current_scheme = updated_catalog.schemes[style_index_];
    const int clear_r = clamp_theme_color_byte(theme_editor_clear_color_[0]);
    const int clear_g = clamp_theme_color_byte(theme_editor_clear_color_[1]);
    const int clear_b = clamp_theme_color_byte(theme_editor_clear_color_[2]);
    updated_catalog.schemes[style_index_] = theme_scheme_from_style(string_view_of(current_scheme.name),
                                                                    current_scheme.base_style,
                                                                    current_scheme.use_win9x_chrome,
                                                                    clear_r,
                                                                    clear_g,
                                                                    clear_b,
                                                                    ImGui::GetStyle());

    if (!settings_store_.save_theme_catalog(updated_catalog)) {
        log.addf("vkGUI settings: failed to save themes to /data/vkgui/vkgui_settings.db (%s).",
                 settings_store_.last_error().c_str());
        push_notification(ImGuiToastType::Error, 0, "Failed to save theme.");
        return;
    }

    theme_catalog_ = updated_catalog;
    discard_theme_editor_changes();
    log.addf("Theme saved to database: %s.", theme_catalog_.schemes[style_index_].name.c_str());
    push_notification(ImGuiToastType::Success, 3000, "Theme saved.");
}

void ShellUi::draw_menu_bar(PluginHost& plugin_host, PanelRegistry& panel_registry)
{
    LaunchRegistry& launch_registry = plugin_host.launch_registry;
    WindowManager& window_manager = plugin_host.window_manager;
    ConsoleLog& log = plugin_host.log;

    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New", "Ctrl+N")) {
            reset_counter(&log, "File > New: counter reset.");
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Drop to Shell")) {
            request_drop_to_shell(&log, "File > Drop to Shell: replacing vkGUI with /bin/shell.vbin.");
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Ctrl+Q")) {
            request_quit(&log, "Quit requested via menu.");
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Increment Counter", "+")) {
            ++counter_;
            log.addf("Counter incremented to %d.", counter_);
        }
        if (ImGui::MenuItem("Decrement Counter", "-")) {
            --counter_;
            log.addf("Counter decremented to %d.", counter_);
        }
        if (ImGui::MenuItem("Reset Counter")) {
            reset_counter(&log, "Counter reset.");
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Settings...")) {
            show_settings_ = true;
            log.add("Opened Settings.");
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Launch")) {
        if (ImGui::MenuItem("Refresh App List")) {
            launch_registry.refresh(log);
            push_notification(ImGuiToastType::Success, 3000, "App list refreshed.");
        }

        ImGui::Separator();
        if (launch_registry.empty()) {
            ImGui::BeginDisabled();
            ImGui::MenuItem("No staged apps found", nullptr, false, false);
            ImGui::EndDisabled();
        } else {
            for (int index = 0; index < launch_registry.size(); ++index) {
                const LaunchMenuEntry& entry = launch_registry.entry(index);
                if (ImGui::MenuItem(entry.label.c_str())) {
                    (void)window_manager.launch_windowed_app(string_view_of(entry.path),
                                                             default_app_width_,
                                                             default_app_height_);
                }
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Info Panel", nullptr, &show_info_);
        ImGui::MenuItem("Console", nullptr, &show_console_);
        ImGui::MenuItem("Task Manager", nullptr, &show_task_manager_);
        ImGui::MenuItem("KObj Navigator", nullptr, &show_kobj_);
        ImGui::MenuItem("vkfm", nullptr, &show_vkfm_);
        ImGui::MenuItem("Text Editor", nullptr, &show_text_editor_);
        if (panel_registry.size() > 0) {
            ImGui::Separator();
            panel_registry.draw_menu_items();
        }
        ImGui::Separator();
        ImGui::MenuItem("ImGui Demo", nullptr, &show_demo_);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About vkGUI...")) {
            open_about_ = true;
        }
        ImGui::EndMenu();
    }

    ImGuiIO& io = ImGui::GetIO();
    std::array<char, 32> fps_buffer {};
    const unsigned fps_times_ten = static_cast<unsigned>(io.Framerate * 10.0f + 0.5f);
    snprintf(fps_buffer.data(), fps_buffer.size(), "%u.%u FPS", fps_times_ten / 10, fps_times_ten % 10);
    const float width = ImGui::CalcTextSize(fps_buffer.data()).x + ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - width);
    ImGui::TextDisabled("%s", fps_buffer.data());

    ImGui::EndMainMenuBar();
}

void ShellUi::draw_info_window(const vk_framebuffer_info_t& framebuffer,
                               WindowManager& window_manager,
                               ConsoleLog& log)
{
    if (!show_info_) {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(10.0f, 30.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(330.0f, 290.0f), ImGuiCond_FirstUseEver);

    if (!imgui_begin_window_readable_caption("Info Panel", &show_info_)) {
        ImGui::End();
        return;
    }
    window_manager.clear_focus_if_host_window_focused();

    ImGui::SeparatorText("Framebuffer");
    ImGui::Text("Resolution  : %u x %u", framebuffer.width, framebuffer.height);
    ImGui::Text("Stride      : %u px", framebuffer.stride);
    ImGui::Text("Format      : %s",
                framebuffer.format == VK_PIXEL_FORMAT_BGRX_8BPP ? "BGRX-8bpp"
              : framebuffer.format == VK_PIXEL_FORMAT_RGBX_8BPP ? "RGBX-8bpp"
              : framebuffer.format == VK_PIXEL_FORMAT_BITMASK ? "Bitmask"
                                                               : "BLT-only");
    ImGui::Text("Base        : 0x%llx", static_cast<unsigned long long>(framebuffer.base));

    ImGui::SeparatorText("Timing");
    ImGuiIO& io = ImGui::GetIO();
    const unsigned frame_ms_times_100 = static_cast<unsigned>(io.DeltaTime * 100000.0f + 0.5f);
    const unsigned fps_times_ten = static_cast<unsigned>(io.Framerate * 10.0f + 0.5f);
    ImGui::Text("Frame time  : %u.%02u ms", frame_ms_times_100 / 100, frame_ms_times_100 % 100);
    ImGui::Text("FPS         : %u.%u", fps_times_ten / 10, fps_times_ten % 10);

    const vk_api_t* api = vk_get_api();
    const vk_u64 tick = api->vk_tick_count();
    const vk_u32 ticks_per_second = api->vk_ticks_per_sec();
    const vk_u64 seconds = ticks_per_second != 0 ? tick / ticks_per_second : 0;
    ImGui::Text("Uptime      : %llus", static_cast<unsigned long long>(seconds));

    ImGui::SeparatorText("Counter");
    ImGui::SetNextItemWidth(80.0f);
    ImGui::InputInt("##counter_val", &counter_, 0, 0);

    ImGui::SameLine();
    if (ImGui::Button(" + ")) {
        if (counter_wrap_ && counter_ >= counter_max_) {
            counter_ = 0;
        } else {
            ++counter_;
        }
        log.addf("Counter -> %d", counter_);
    }
    ImGui::SameLine();
    if (ImGui::Button(" - ")) {
        --counter_;
        log.addf("Counter -> %d", counter_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        reset_counter(&log, "Counter reset.");
    }

    const int display_max = counter_max_ > 0 ? counter_max_ : 1;
    float progress = static_cast<float>(counter_) / static_cast<float>(display_max);
    if (progress < 0.0f) {
        progress = 0.0f;
    }
    if (progress > 1.0f) {
        progress = 1.0f;
    }
    ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));

    ImGui::Checkbox("Wrap at max", &counter_wrap_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::InputInt("Max", &counter_max_, 1, 10);

    ImGui::End();
}

void ShellUi::draw_settings_window(WindowManager& window_manager, ConsoleLog& log)
{
    if (!show_settings_) {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(200.0f, 150.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(310.0f, 215.0f), ImGuiCond_FirstUseEver);

    if (!imgui_begin_window_readable_caption("Settings", &show_settings_, ImGuiWindowFlags_NoResize)) {
        ImGui::End();
        return;
    }
    window_manager.clear_focus_if_host_window_focused();

    ImGui::SeparatorText("Appearance");

    if (theme_catalog_.count > 0
        && ImGui::Combo("Color scheme", &style_index_, theme_name_getter, &theme_catalog_, theme_catalog_.count)) {
        apply_style();
        load_theme_editor_from_selected_scheme();
        log.addf("Style changed to %s.", theme_catalog_.schemes[style_index_].name.c_str());
    }

    if (theme_catalog_.count > 0) {
        if (ImGui::Button("Open Theme Editor", ImVec2(-1.0f, 0.0f))) {
            discard_theme_editor_changes();
            show_theme_editor_ = true;
            log.addf("Opened Theme Editor for %s.", theme_catalog_.schemes[style_index_].name.c_str());
        }
    }

    if (ImGui::SliderFloat("Font scale", &font_scale_, 0.5f, 2.0f, "%.1f")) {
        ImGui::GetIO().FontGlobalScale = font_scale_;
    }

    ImGui::SeparatorText("Renderer");
    if (ImGui::Checkbox("Transparency (slow)", &transparency_)) {
        ImGui_ImplVK_SetTransparencyEnabled(transparency_);
        apply_style();
        log.addf("Transparency %s.", transparency_ ? "ON" : "OFF");
    }

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Close", ImVec2(120.0f, 0.0f))) {
        show_settings_ = false;
    }

    ImGui::End();
}

void ShellUi::draw_theme_editor_window(WindowManager& window_manager, ConsoleLog& log)
{
    if (!show_theme_editor_) {
        return;
    }

    bool keep_open = show_theme_editor_;
    ImGui::SetNextWindowPos(ImVec2(220.0f, 70.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(760.0f, 640.0f), ImGuiCond_FirstUseEver);

    if (!imgui_begin_window_readable_caption("Theme Editor", &keep_open)) {
        ImGui::End();
        show_theme_editor_ = keep_open;
        if (!show_theme_editor_) {
            discard_theme_editor_changes();
        }
        return;
    }
    show_theme_editor_ = keep_open;
    window_manager.clear_focus_if_host_window_focused();

    if (theme_catalog_.count <= 0 || style_index_ < 0 || style_index_ >= theme_catalog_.count) {
        ImGui::TextDisabled("No themes available.");
        ImGui::End();
        return;
    }

    const std::string scheme_name = theme_catalog_.schemes[style_index_].name;
    ImGui::Text("Editing scheme: %s", scheme_name.c_str());
    ImGui::TextDisabled("Preview is live. Save writes the selected scheme back to the database.");

    if (ImGui::ColorEdit3("Desktop clear color", theme_editor_clear_color_.data())) {
        ::ImGui_ImplVK_SetClearColor(static_cast<unsigned int>(clamp_theme_color_byte(theme_editor_clear_color_[0])),
                                     static_cast<unsigned int>(clamp_theme_color_byte(theme_editor_clear_color_[1])),
                                     static_cast<unsigned int>(clamp_theme_color_byte(theme_editor_clear_color_[2])));
    }

    if (ImGui::Button("Save Theme", ImVec2(130.0f, 0.0f))) {
        save_current_theme(log);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Theme", ImVec2(130.0f, 0.0f))) {
        discard_theme_editor_changes();
        log.addf("Theme reset to saved values: %s.", scheme_name.c_str());
    }

    ImGui::Separator();
    ImGui::ShowStyleEditor(&theme_editor_reference_style_);
    ImGui::End();

    if (!keep_open) {
        show_theme_editor_ = false;
        discard_theme_editor_changes();
    }
}

void ShellUi::draw_about_modal()
{
    if (open_about_) {
        ImGui::OpenPopup("About vkGUI");
        open_about_ = false;
    }

    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(320.0f, 160.0f), ImGuiCond_Always);

    if (ImGui::BeginPopupModal("About vkGUI", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::TextWrapped("vkGUI - Dear ImGui GUI for vkernel");
        ImGui::Spacing();
        ImGui::TextWrapped("Software renderer: barycentric triangle fill,");
        ImGui::TextWrapped("alpha-8 font atlas, framebuffer blending.");
        ImGui::Spacing();
        ImGui::TextWrapped("Keyboard-only navigation, newlib C runtime.");
        ImGui::Spacing();
        ImGui::Separator();

        const float button_width = 100.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - button_width) * 0.5f);
        if (ImGui::Button("OK", ImVec2(button_width, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ShellUi::sync_settings(ConsoleLog& log)
{
    if (!settings_store_ready_) {
        return;
    }

    const PersistedSettings current = current_settings_snapshot();
    if (current.compare(last_saved_settings_)) {
        return;
    }

    if (!settings_store_.save(current)) {
        log.addf("vkGUI settings: failed to save /data/vkgui/vkgui_settings.db (%s).",
                 settings_store_.last_error().c_str());
        settings_store_ready_ = false;
        push_notification(ImGuiToastType::Warning, 5000, "Settings autosave failed and was disabled.");
        return;
    }

    last_saved_settings_ = current;
}

void ShellUi::draw(PluginHost& plugin_host,
                   PanelRegistry& panel_registry,
                   TaskManagerPanel& task_manager,
                   KobjNavigator& kobj_navigator,
                   TextEditorPanel& text_editor,
                   VkfmPanel& vkfm_panel)
{
    draw_menu_bar(plugin_host, panel_registry);
    draw_info_window(plugin_host.framebuffer, plugin_host.window_manager, plugin_host.log);
    plugin_host.log.draw_window(show_console_, plugin_host.window_manager);
    task_manager.draw_window(show_task_manager_, plugin_host.window_manager);
    kobj_navigator.draw_window(show_kobj_, plugin_host.window_manager);
    vkfm_panel.draw_window(show_vkfm_, plugin_host.window_manager, plugin_host.log);
    text_editor.draw_window(show_text_editor_, plugin_host.window_manager, plugin_host.log);
    panel_registry.draw_windows(plugin_host);
    plugin_host.window_manager.draw_windows();
    draw_settings_window(plugin_host.window_manager, plugin_host.log);
    draw_theme_editor_window(plugin_host.window_manager, plugin_host.log);
    draw_about_modal();
    sync_settings(plugin_host.log);

    if (show_demo_) {
        ImGui::ShowDemoWindow(&show_demo_);
    }
}

} // namespace vkgui
