/*
 * vkgui/main.cpp
 * Dear ImGui demonstration for vkernel.
 */

#include "console_log.h"
#include "font_catalog.h"
#include "icons.h"
#include "implot.h"
#include "kobj_panel.h"
#include "deps/imguinotify/fa-solid-900.h"
#include "ImGuiNotify.hpp"
#include "launch_registry.h"
#include "plugin_registry.h"
#include "shell_ui.h"
#include "task_manager_panel.h"
#include "text_editor_panel.h"
#include "vkfm_panel.h"
#include "window_manager.h"

#include <iostream>
#include <unistd.h>

namespace {

auto framebuffer_available(const vk_framebuffer_info_t& framebuffer) -> bool
{
    return framebuffer.valid != 0 && framebuffer.base != 0 && framebuffer.width != 0 && framebuffer.height != 0;
}

} // namespace

int main(int /*argc*/, char** /*argv*/)
{
    bool drop_to_shell = false;

    vk_framebuffer_info_t framebuffer = {};
    VK_CALL(framebuffer_info, &framebuffer);
    if (!framebuffer_available(framebuffer)) {
        std::cout << "vkgui: no framebuffer available\n";
        return 1;
    }

    ImGui::CreateContext();
    ImPlot::CreateContext();
    {
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
    }

    /* Merge the vendored IconFontCppHeaders Font Awesome map before backend
     * init so menus, vkfm tiles, and notifications all share the same glyphs. */
    {
        ImGuiIO& io = ImGui::GetIO();
        (void)vkgui::configure_ui_fonts(io, vkgui::UiFontFamily::default_builtin);
    }

    if (!ImGui_ImplVK_Init(&framebuffer)) {
        std::cout << "vkgui: backend init failed\n";
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
        return 1;
    }

    {
        vkgui::ConsoleLog log;
        vkgui::LaunchRegistry launch_registry;
        vkgui::TaskManagerPanel task_manager;
        vkgui::KobjNavigator kobj_navigator;
        vkgui::TextEditorPanel text_editor;
        vkgui::VkfmPanel vkfm_panel;
        vkgui::PanelRegistry panel_registry;
        vkgui::WindowManager window_manager(log);
        vkgui::ShellUi ui;
        vkgui::PluginHost plugin_host {
            framebuffer,
            launch_registry,
            window_manager,
            log,
        };

        ui.initialize(framebuffer, &log);

        log.add("vkGUI started.");
        log.addf("Framebuffer: %ux%u @ %s",
                 framebuffer.width,
                 framebuffer.height,
                 framebuffer.format == VK_PIXEL_FORMAT_BGRX_8BPP ? "BGRX" : "RGBX");
        log.add("Move the mouse to control the cursor.");
        log.add("Alt to open the menu bar. Tab/Arrows to navigate.");
        log.add("Enter/Space to activate. Ctrl+Q to quit.");
        log.add("Use Launch from the menu bar to start staged apps.");
        launch_registry.refresh(log);
        kobj_navigator.refresh_selected();
        panel_registry.discover(plugin_host);

        if (vk_get_api()->vk_set_compositor_active) {
            (void)vk_get_api()->vk_set_compositor_active(1u);
        }

        while (ui.running()) {
            vk_key_event_t key_event;
            while (vk_get_api()->vk_poll_key(&key_event)) {
                if (!window_manager.route_key_event(key_event)) {
                    ImGui_ImplVK_ProcessKey(&key_event);
                }

                if (key_event.pressed != 0
                    && (key_event.modifiers & 2u) != 0
                    && (key_event.ascii == 'q' || key_event.ascii == 'Q')) {
                    ui.request_quit();
                }
            }

            vk_mouse_event_t mouse_event;
            while (vk_get_api()->vk_poll_mouse(&mouse_event)) {
                ImGui_ImplVK_ProcessMouse(&mouse_event);
                (void)window_manager.route_mouse_event(mouse_event);
            }

            ImGui_ImplVK_NewFrame();
            ImGui::NewFrame();

            if (ImGui::GetIO().KeyCtrl) {
                if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) {
                    ui.request_quit(&log, "Ctrl+Q - quitting.");
                }
                if (ImGui::IsKeyPressed(ImGuiKey_N, false)) {
                    ui.reset_counter(&log, "Ctrl+N - counter reset.");
                }
            }

            ui.draw(plugin_host,
                    panel_registry,
                    task_manager,
                    kobj_navigator,
                    text_editor,
                    vkfm_panel);

            /* Render notifications with the active popup styling so they
             * follow the selected theme instead of forcing a dark toast skin. */
            const ImGuiStyle& style = ImGui::GetStyle();
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, style.PopupRounding);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, style.PopupBorderSize);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, style.Colors[ImGuiCol_PopupBg]);
            ImGui::RenderNotifications();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(1);

            ImGui::Render();
            ImGui_ImplVK_RenderDrawData(ImGui::GetDrawData(), &framebuffer);
            vk_get_api()->vk_sleep(1);
        }

        drop_to_shell = ui.drop_to_shell_requested();
        panel_registry.shutdown(plugin_host);
        window_manager.shutdown();
    }

    ImGui_ImplVK_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    if (vk_get_api()->vk_set_compositor_active) {
        (void)vk_get_api()->vk_set_compositor_active(0u);
    }
    if (vk_get_api()->vk_set_compositor_default_fb) {
        (void)vk_get_api()->vk_set_compositor_default_fb(nullptr);
    }

    if (drop_to_shell) {
        char* const argv[] = {
            const_cast<char*>("/bin/shell.vbin"),
            nullptr,
        };
        (void)execve("/bin/shell.vbin", argv, nullptr);
        std::cout << "vkgui: failed to exec /bin/shell.vbin\n";
        return 1;
    }

    std::cout << "vkgui: clean exit.\n";

    return 0;
}
