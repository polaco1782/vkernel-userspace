#include "shell_ui.h"

#include "console_log.h"
#include "kobj_panel.h"
#include "launch_registry.h"
#include "plugin_registry.h"
#include "task_manager_panel.h"
#include "vkfm_panel.h"
#include "window_manager.h"

#include <stdio.h>

namespace vgui {

namespace {
struct RgbColor {
    unsigned int r;
    unsigned int g;
    unsigned int b;
};

[[nodiscard]] constexpr auto hex_digit_value(char digit) -> unsigned int
{
    if (digit >= '0' && digit <= '9') {
        return static_cast<unsigned int>(digit - '0');
    }
    if (digit >= 'a' && digit <= 'f') {
        return static_cast<unsigned int>(digit - 'a' + 10);
    }
    if (digit >= 'A' && digit <= 'F') {
        return static_cast<unsigned int>(digit - 'A' + 10);
    }
    return 0;
}

[[nodiscard]] constexpr auto is_hex_digit(char digit) -> bool
{
    return (digit >= '0' && digit <= '9')
        || (digit >= 'a' && digit <= 'f')
        || (digit >= 'A' && digit <= 'F');
}

[[nodiscard]] auto parse_rgb_hex(vk::string_view hex_string) -> RgbColor
{
    if (hex_string.size() != 7 || hex_string[0] != '#') {
        return {0, 0, 0};
    }
    for (vk_u32 index = 1; index < hex_string.size(); ++index) {
        if (!is_hex_digit(hex_string[index])) {
            return {0, 0, 0};
        }
    }

    /* Accept #RRGGBB and keep the fallback deterministic for malformed input. */
    return {
        (hex_digit_value(hex_string[1]) << 4) | hex_digit_value(hex_string[2]),
        (hex_digit_value(hex_string[3]) << 4) | hex_digit_value(hex_string[4]),
        (hex_digit_value(hex_string[5]) << 4) | hex_digit_value(hex_string[6]),
    };
}

[[nodiscard]] constexpr auto from_rgb(float r, float g, float b, float a) -> ImVec4
{
    /* Our renderer path expects ImGui colors pre-swizzled to BGR. */
    return ImVec4(b, g, r, a);
}

[[nodiscard]] auto FROM_HEX(vk::string_view hex, float alpha) -> ImVec4
{
    const RgbColor color = parse_rgb_hex(hex);
    return from_rgb((float)color.r / 255.0f,
                    (float)color.g / 255.0f,
                    (float)color.b / 255.0f,
                    alpha);

} // namespace

void ImGui_ImplVK_SetClearColor(vk::string_view hex_string)
{
    const auto color = parse_rgb_hex(hex_string);
    ::ImGui_ImplVK_SetClearColor(color.b, color.g, color.r);
}

void apply_scheme_ocean()
{
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_WindowBg] = from_rgb(0.05f, 0.08f, 0.12f, colors[ImGuiCol_WindowBg].w);
    colors[ImGuiCol_ChildBg] = from_rgb(0.04f, 0.07f, 0.10f, colors[ImGuiCol_ChildBg].w);
    colors[ImGuiCol_PopupBg] = from_rgb(0.06f, 0.09f, 0.14f, colors[ImGuiCol_PopupBg].w);
    colors[ImGuiCol_TitleBg] = from_rgb(0.03f, 0.20f, 0.26f, colors[ImGuiCol_TitleBg].w);
    colors[ImGuiCol_TitleBgActive] = from_rgb(0.04f, 0.28f, 0.35f, colors[ImGuiCol_TitleBgActive].w);
    colors[ImGuiCol_Header] = from_rgb(0.08f, 0.33f, 0.46f, colors[ImGuiCol_Header].w);
    colors[ImGuiCol_HeaderHovered] = from_rgb(0.12f, 0.41f, 0.56f, colors[ImGuiCol_HeaderHovered].w);
    colors[ImGuiCol_HeaderActive] = from_rgb(0.14f, 0.46f, 0.62f, colors[ImGuiCol_HeaderActive].w);
    colors[ImGuiCol_Button] = from_rgb(0.07f, 0.36f, 0.50f, colors[ImGuiCol_Button].w);
    colors[ImGuiCol_ButtonHovered] = from_rgb(0.11f, 0.45f, 0.61f, colors[ImGuiCol_ButtonHovered].w);
    colors[ImGuiCol_ButtonActive] = from_rgb(0.14f, 0.50f, 0.68f, colors[ImGuiCol_ButtonActive].w);
    colors[ImGuiCol_FrameBg] = from_rgb(0.08f, 0.15f, 0.22f, colors[ImGuiCol_FrameBg].w);
    colors[ImGuiCol_FrameBgHovered] = from_rgb(0.12f, 0.23f, 0.32f, colors[ImGuiCol_FrameBgHovered].w);
    colors[ImGuiCol_FrameBgActive] = from_rgb(0.13f, 0.27f, 0.38f, colors[ImGuiCol_FrameBgActive].w);
    colors[ImGuiCol_SliderGrab] = from_rgb(0.24f, 0.64f, 0.82f, colors[ImGuiCol_SliderGrab].w);
    colors[ImGuiCol_SliderGrabActive] = from_rgb(0.30f, 0.74f, 0.92f, colors[ImGuiCol_SliderGrabActive].w);
    colors[ImGuiCol_CheckMark] = from_rgb(0.36f, 0.80f, 0.94f, colors[ImGuiCol_CheckMark].w);
    colors[ImGuiCol_Separator] = from_rgb(0.18f, 0.34f, 0.44f, colors[ImGuiCol_Separator].w);
    colors[ImGuiCol_ResizeGrip] = from_rgb(0.22f, 0.56f, 0.72f, colors[ImGuiCol_ResizeGrip].w);
    colors[ImGuiCol_Tab] = from_rgb(0.07f, 0.24f, 0.34f, colors[ImGuiCol_Tab].w);
    colors[ImGuiCol_TabActive] = from_rgb(0.10f, 0.35f, 0.49f, colors[ImGuiCol_TabActive].w);
}

void apply_scheme_forest()
{
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_WindowBg] = from_rgb(0.07f, 0.10f, 0.08f, colors[ImGuiCol_WindowBg].w);
    colors[ImGuiCol_ChildBg] = from_rgb(0.05f, 0.08f, 0.06f, colors[ImGuiCol_ChildBg].w);
    colors[ImGuiCol_PopupBg] = from_rgb(0.08f, 0.12f, 0.09f, colors[ImGuiCol_PopupBg].w);
    colors[ImGuiCol_TitleBg] = from_rgb(0.12f, 0.22f, 0.15f, colors[ImGuiCol_TitleBg].w);
    colors[ImGuiCol_TitleBgActive] = from_rgb(0.15f, 0.30f, 0.19f, colors[ImGuiCol_TitleBgActive].w);
    colors[ImGuiCol_Header] = from_rgb(0.17f, 0.32f, 0.20f, colors[ImGuiCol_Header].w);
    colors[ImGuiCol_HeaderHovered] = from_rgb(0.22f, 0.40f, 0.25f, colors[ImGuiCol_HeaderHovered].w);
    colors[ImGuiCol_HeaderActive] = from_rgb(0.25f, 0.45f, 0.27f, colors[ImGuiCol_HeaderActive].w);
    colors[ImGuiCol_Button] = from_rgb(0.16f, 0.36f, 0.22f, colors[ImGuiCol_Button].w);
    colors[ImGuiCol_ButtonHovered] = from_rgb(0.22f, 0.45f, 0.27f, colors[ImGuiCol_ButtonHovered].w);
    colors[ImGuiCol_ButtonActive] = from_rgb(0.26f, 0.52f, 0.31f, colors[ImGuiCol_ButtonActive].w);
    colors[ImGuiCol_FrameBg] = from_rgb(0.11f, 0.17f, 0.12f, colors[ImGuiCol_FrameBg].w);
    colors[ImGuiCol_FrameBgHovered] = from_rgb(0.16f, 0.25f, 0.17f, colors[ImGuiCol_FrameBgHovered].w);
    colors[ImGuiCol_FrameBgActive] = from_rgb(0.19f, 0.29f, 0.20f, colors[ImGuiCol_FrameBgActive].w);
    colors[ImGuiCol_SliderGrab] = from_rgb(0.45f, 0.71f, 0.35f, colors[ImGuiCol_SliderGrab].w);
    colors[ImGuiCol_SliderGrabActive] = from_rgb(0.55f, 0.80f, 0.44f, colors[ImGuiCol_SliderGrabActive].w);
    colors[ImGuiCol_CheckMark] = from_rgb(0.63f, 0.86f, 0.45f, colors[ImGuiCol_CheckMark].w);
    colors[ImGuiCol_Separator] = from_rgb(0.24f, 0.35f, 0.25f, colors[ImGuiCol_Separator].w);
    colors[ImGuiCol_ResizeGrip] = from_rgb(0.33f, 0.55f, 0.36f, colors[ImGuiCol_ResizeGrip].w);
    colors[ImGuiCol_Tab] = from_rgb(0.12f, 0.21f, 0.14f, colors[ImGuiCol_Tab].w);
    colors[ImGuiCol_TabActive] = from_rgb(0.21f, 0.36f, 0.24f, colors[ImGuiCol_TabActive].w);
}

void apply_scheme_sunset()
{
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_WindowBg] = from_rgb(0.14f, 0.09f, 0.10f, colors[ImGuiCol_WindowBg].w);
    colors[ImGuiCol_ChildBg] = from_rgb(0.12f, 0.07f, 0.08f, colors[ImGuiCol_ChildBg].w);
    colors[ImGuiCol_PopupBg] = from_rgb(0.16f, 0.10f, 0.11f, colors[ImGuiCol_PopupBg].w);
    colors[ImGuiCol_TitleBg] = from_rgb(0.31f, 0.15f, 0.12f, colors[ImGuiCol_TitleBg].w);
    colors[ImGuiCol_TitleBgActive] = from_rgb(0.40f, 0.20f, 0.16f, colors[ImGuiCol_TitleBgActive].w);
    colors[ImGuiCol_Header] = from_rgb(0.45f, 0.24f, 0.17f, colors[ImGuiCol_Header].w);
    colors[ImGuiCol_HeaderHovered] = from_rgb(0.53f, 0.30f, 0.20f, colors[ImGuiCol_HeaderHovered].w);
    colors[ImGuiCol_HeaderActive] = from_rgb(0.59f, 0.35f, 0.22f, colors[ImGuiCol_HeaderActive].w);
    colors[ImGuiCol_Button] = from_rgb(0.47f, 0.24f, 0.15f, colors[ImGuiCol_Button].w);
    colors[ImGuiCol_ButtonHovered] = from_rgb(0.56f, 0.30f, 0.18f, colors[ImGuiCol_ButtonHovered].w);
    colors[ImGuiCol_ButtonActive] = from_rgb(0.64f, 0.35f, 0.20f, colors[ImGuiCol_ButtonActive].w);
    colors[ImGuiCol_FrameBg] = from_rgb(0.20f, 0.12f, 0.11f, colors[ImGuiCol_FrameBg].w);
    colors[ImGuiCol_FrameBgHovered] = from_rgb(0.29f, 0.17f, 0.14f, colors[ImGuiCol_FrameBgHovered].w);
    colors[ImGuiCol_FrameBgActive] = from_rgb(0.35f, 0.20f, 0.16f, colors[ImGuiCol_FrameBgActive].w);
    colors[ImGuiCol_SliderGrab] = from_rgb(0.88f, 0.52f, 0.23f, colors[ImGuiCol_SliderGrab].w);
    colors[ImGuiCol_SliderGrabActive] = from_rgb(0.97f, 0.61f, 0.28f, colors[ImGuiCol_SliderGrabActive].w);
    colors[ImGuiCol_CheckMark] = from_rgb(0.99f, 0.73f, 0.35f, colors[ImGuiCol_CheckMark].w);
    colors[ImGuiCol_Separator] = from_rgb(0.41f, 0.24f, 0.17f, colors[ImGuiCol_Separator].w);
    colors[ImGuiCol_ResizeGrip] = from_rgb(0.67f, 0.37f, 0.21f, colors[ImGuiCol_ResizeGrip].w);
    colors[ImGuiCol_Tab] = from_rgb(0.24f, 0.13f, 0.11f, colors[ImGuiCol_Tab].w);
    colors[ImGuiCol_TabActive] = from_rgb(0.40f, 0.22f, 0.15f, colors[ImGuiCol_TabActive].w);
}

void apply_scheme_win9x()
{
    ImGui::StyleColorsClassic();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text] = FROM_HEX("#000000", colors[ImGuiCol_Text].w);
    colors[ImGuiCol_TextDisabled] = FROM_HEX("#7F7F7F", colors[ImGuiCol_TextDisabled].w);

    /* Windows and controls stay classic gray (#d4d0c8). */
    colors[ImGuiCol_WindowBg] = FROM_HEX("#d4d0c8", colors[ImGuiCol_WindowBg].w);
    colors[ImGuiCol_ChildBg] = FROM_HEX("#FFFFFF", colors[ImGuiCol_ChildBg].w);
    colors[ImGuiCol_PopupBg] = FROM_HEX("#d4d0c8", colors[ImGuiCol_PopupBg].w);

    /* 3D cue: white highlight and dark gray shadow. */
    colors[ImGuiCol_Border] = FROM_HEX("#4F4F4F", colors[ImGuiCol_Border].w);
    colors[ImGuiCol_BorderShadow] = FROM_HEX("#FFFFFF", colors[ImGuiCol_BorderShadow].w);

    colors[ImGuiCol_FrameBg] = FROM_HEX("#FFFFFF", colors[ImGuiCol_FrameBg].w);
    colors[ImGuiCol_FrameBgHovered] = FROM_HEX("#DCDCDC", colors[ImGuiCol_FrameBgHovered].w);
    colors[ImGuiCol_FrameBgActive] = FROM_HEX("#A8A8A8", colors[ImGuiCol_FrameBgActive].w);

    colors[ImGuiCol_TitleBg] = FROM_HEX("#4040A0", colors[ImGuiCol_TitleBg].w);
    colors[ImGuiCol_TitleBgActive] = FROM_HEX("#0A246A", colors[ImGuiCol_TitleBgActive].w);
    colors[ImGuiCol_MenuBarBg] = FROM_HEX("#d4d0c8", colors[ImGuiCol_MenuBarBg].w);

    colors[ImGuiCol_ScrollbarBg] = FROM_HEX("#d4d0c8", colors[ImGuiCol_ScrollbarBg].w);
    colors[ImGuiCol_ScrollbarGrab] = FROM_HEX("#AEAEAE", colors[ImGuiCol_ScrollbarGrab].w);
    colors[ImGuiCol_ScrollbarGrabHovered] = FROM_HEX("#949494", colors[ImGuiCol_ScrollbarGrabHovered].w);
    colors[ImGuiCol_ScrollbarGrabActive] = FROM_HEX("#737373", colors[ImGuiCol_ScrollbarGrabActive].w);

    colors[ImGuiCol_CheckMark] = FROM_HEX("#000000", colors[ImGuiCol_CheckMark].w);
    colors[ImGuiCol_SliderGrab] = FROM_HEX("#A0A0A0", colors[ImGuiCol_SliderGrab].w);
    colors[ImGuiCol_SliderGrabActive] = FROM_HEX("#737373", colors[ImGuiCol_SliderGrabActive].w);

    colors[ImGuiCol_Button] = FROM_HEX("#d4d0c8", colors[ImGuiCol_Button].w);
    colors[ImGuiCol_ButtonHovered] = FROM_HEX("#DCDCDC", colors[ImGuiCol_ButtonHovered].w);
    colors[ImGuiCol_ButtonActive] = FROM_HEX("#A8A8A8", colors[ImGuiCol_ButtonActive].w);

    // table rows are same as window background, with a dark separator line, so they look like they are sunken into the window.
    colors[ImGuiCol_TableRowBg] = FROM_HEX("#FFFFFF", colors[ImGuiCol_TableRowBg].w);

    colors[ImGuiCol_Header] = FROM_HEX("#C6D2EA", colors[ImGuiCol_Header].w);
    colors[ImGuiCol_HeaderHovered] = FROM_HEX("#ACCAE6", colors[ImGuiCol_HeaderHovered].w);
    colors[ImGuiCol_HeaderActive] = FROM_HEX("#8EADDF", colors[ImGuiCol_HeaderActive].w);
    colors[ImGuiCol_Separator] = FROM_HEX("#606060", colors[ImGuiCol_Separator].w);
    colors[ImGuiCol_ResizeGrip] = FROM_HEX("#A0A0A0", colors[ImGuiCol_ResizeGrip].w);
    colors[ImGuiCol_ResizeGripHovered] = FROM_HEX("#808080", colors[ImGuiCol_ResizeGripHovered].w);
    colors[ImGuiCol_ResizeGripActive] = FROM_HEX("#5A5A5A", colors[ImGuiCol_ResizeGripActive].w);
    colors[ImGuiCol_Tab] = FROM_HEX("#d4d0c8", colors[ImGuiCol_Tab].w);
    colors[ImGuiCol_TabHovered] = FROM_HEX("#DCDCDC", colors[ImGuiCol_TabHovered].w);
    colors[ImGuiCol_TabActive] = FROM_HEX("#A8A8A8", colors[ImGuiCol_TabActive].w);

    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBorderSize = 1.0f;
    style.FramePadding = ImVec2(5.0f, 3.0f);

    style.ScrollbarSize = 20.0f;
    style.GrabMinSize = 15.0f;
    style.WindowBorderSize = 1.0f;

    style.AntiAliasedFill = true;
    style.AntiAliasedLines = true;
}

}

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

    if (settings_store_.open("/vgui_settings.db")) {
        PersistedSettings settings = current_settings_snapshot();
        if (settings_store_.load(settings)) {
            apply_saved_settings(settings);
            last_saved_settings_ = settings;
            settings_store_ready_ = true;
            log->add("vGUI settings: loaded saved settings from /vgui_settings.db.");
        } else if (log != nullptr) {
            log->addf("vGUI settings: failed to load /vgui_settings.db (%s).",
                      settings_store_.last_error().c_str());
        }
    } else if (log != nullptr) {
        log->addf("vGUI settings: failed to open /vgui_settings.db (%s).",
                  settings_store_.last_error().c_str());
    }

    ImGui_ImplVK_SetTransparencyEnabled(transparency_);
    apply_style();
    ImGui::GetIO().FontGlobalScale = font_scale_;
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
    return settings;
}

void ShellUi::apply_saved_settings(const PersistedSettings& settings)
{
    style_index_ = settings.style_index;
    if (style_index_ < 0 || style_index_ > 6) {
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
}

void ShellUi::apply_style()
{
    /* Default desktop clear used by non-Win9x themes. */
    ::ImGui_ImplVK_SetClearColor(22, 22, 30);

    switch (style_index_) {
    case 1:
        ImGui::StyleColorsLight();
        break;
    case 2:
        ImGui::StyleColorsClassic();
        break;
    case 3:
        apply_scheme_ocean();
        break;
    case 4:
        apply_scheme_forest();
        break;
    case 5:
        apply_scheme_sunset();
        break;
    case 6:
        apply_scheme_win9x();
        /* Framebuffer clear color stays on the backend's RGB byte path. */
        ImGui_ImplVK_SetClearColor("#3a6ea5");
        break;
    default:
        ImGui::StyleColorsDark();
        break;
    }

    if (!transparency_) {
        ImGuiStyle& style = ImGui::GetStyle();
        for (int index = 0; index < ImGuiCol_COUNT; ++index) {
            style.Colors[index].w = 1.0f;
        }
    }
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
            request_drop_to_shell(&log, "File > Drop to Shell: replacing vGUI with shell.vbin.");
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
        if (panel_registry.size() > 0) {
            ImGui::Separator();
            panel_registry.draw_menu_items();
        }
        ImGui::Separator();
        ImGui::MenuItem("ImGui Demo", nullptr, &show_demo_);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About vGUI...")) {
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
    ImGui::SetNextWindowSize(ImVec2(270.0f, 170.0f), ImGuiCond_FirstUseEver);

    if (!imgui_begin_window_readable_caption("Settings", &show_settings_, ImGuiWindowFlags_NoResize)) {
        ImGui::End();
        return;
    }
    window_manager.clear_focus_if_host_window_focused();

    ImGui::SeparatorText("Appearance");

    const char* style_names[] = { "Dark", "Light", "Classic", "Ocean", "Forest", "Sunset", "Win9x" };
    const int style_name_count = static_cast<int>(sizeof(style_names) / sizeof(style_names[0]));
    if (ImGui::Combo("Color scheme", &style_index_, style_names, style_name_count)) {
        apply_style();
        log.addf("Style changed to %s.", style_names[style_index_]);
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

void ShellUi::draw_about_modal()
{
    if (open_about_) {
        ImGui::OpenPopup("About vGUI");
        open_about_ = false;
    }

    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(320.0f, 160.0f), ImGuiCond_Always);

    if (ImGui::BeginPopupModal("About vGUI", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::TextWrapped("vGUI - Dear ImGui GUI for vkernel");
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
    if (current.equals(last_saved_settings_)) {
        return;
    }

    if (!settings_store_.save(current)) {
        log.addf("vGUI settings: failed to save /vgui_settings.db (%s).",
                 settings_store_.last_error().c_str());
        settings_store_ready_ = false;
        return;
    }

    last_saved_settings_ = current;
}

void ShellUi::draw(PluginHost& plugin_host,
                   PanelRegistry& panel_registry,
                   TaskManagerPanel& task_manager,
                   KobjNavigator& kobj_navigator,
                   VkfmPanel& vkfm_panel)
{
    draw_menu_bar(plugin_host, panel_registry);
    draw_info_window(plugin_host.framebuffer, plugin_host.window_manager, plugin_host.log);
    plugin_host.log.draw_window(show_console_, plugin_host.window_manager);
    task_manager.draw_window(show_task_manager_, plugin_host.window_manager);
    kobj_navigator.draw_window(show_kobj_, plugin_host.window_manager);
    vkfm_panel.draw_window(show_vkfm_, plugin_host.window_manager, plugin_host.log);
    panel_registry.draw_windows(plugin_host);
    plugin_host.window_manager.draw_windows();
    draw_settings_window(plugin_host.window_manager, plugin_host.log);
    draw_about_modal();
    sync_settings(plugin_host.log);

    if (show_demo_) {
        ImGui::ShowDemoWindow(&show_demo_);
    }
}

} // namespace vgui
