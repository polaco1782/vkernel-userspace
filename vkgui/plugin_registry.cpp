#include "dynamic_plugin_loader.h"
#include "plugin_registry.h"

#include "console_log.h"
#include "imgui/imgui.h"

#include <new>
#include <string.h>

namespace vkgui {

namespace {

constexpr auto k_plugin_manifest_path = "vkgui_plugins.txt";

class DynamicPanelPlugin final : public PanelPlugin {
public:
    explicit DynamicPanelPlugin(DynamicPluginModule&& module) noexcept
        : PanelPlugin(module.descriptor().id,
                      module.descriptor().menu_label,
                      module.descriptor().default_visible != 0u),
          module_(std::move(module))
    {
    }

    void shutdown(PluginHost& host) override
    {
        module_.unload(host);
    }

    void draw_window(PluginHost& host, bool& visible) override
    {
        module_.draw_window(host, visible);
    }

private:
    DynamicPluginModule module_ {};
};

} // namespace

PanelPlugin::PanelPlugin(const char* id, const char* menu_label, bool default_visible) noexcept
    : id_(id != nullptr ? id : ""),
      menu_label_(menu_label != nullptr ? menu_label : ""),
      visible_(default_visible)
{
}

void PanelPlugin::initialize(PluginHost& /*host*/)
{
}

void PanelPlugin::shutdown(PluginHost& /*host*/)
{
}

PanelRegistry::~PanelRegistry()
{
    for (int index = 0; index < plugin_count_; ++index) {
        delete plugins_[index];
        plugins_[index] = nullptr;
    }
}

auto PanelRegistry::contains_id(const char* id) const -> bool
{
    if (id == nullptr || id[0] == '\0') {
        return true;
    }

    for (int index = 0; index < plugin_count_; ++index) {
        if (strcmp(plugins_[index]->id(), id) == 0) {
            return true;
        }
    }

    return false;
}

void PanelRegistry::sort_plugins()
{
    for (int index = 1; index < plugin_count_; ++index) {
        PanelPlugin* plugin = plugins_[index];
        int insert_index = index;
        while (insert_index > 0
            && strcmp(plugin->menu_label(), plugins_[insert_index - 1]->menu_label()) < 0) {
            plugins_[insert_index] = plugins_[insert_index - 1];
            --insert_index;
        }
        plugins_[insert_index] = plugin;
    }
}

void PanelRegistry::add_external_plugin(vk::string_view path, PluginHost& host)
{
    if (plugin_count_ >= k_max_plugins) {
        host.log.add("vkGUI plugin loader: plugin limit reached.");
        return;
    }

    DynamicPluginModule module;
    if (!module.load(path, host)) {
        return;
    }
    if (contains_id(module.descriptor().id)) {
        host.log.addf("vkGUI plugin loader: duplicate plugin id '%s' ignored.",
                      module.descriptor().id);
        module.unload(host);
        return;
    }

    auto* plugin = new (std::nothrow) DynamicPanelPlugin(std::move(module));
    if (plugin == nullptr) {
        host.log.addf("vkGUI plugin loader: failed to allocate plugin wrapper for %s.",
                      string_from_view(path).c_str());
        return;
    }

    plugins_[plugin_count_++] = plugin;
}

void PanelRegistry::load_external_plugins(PluginHost& host)
{
    const vk_file_handle_t handle = VK_CALL(file_open, k_plugin_manifest_path, "r");
    if (handle == static_cast<vk_file_handle_t>(0)) {
        host.log.add("vkGUI plugin loader: plugin manifest not found.");
        return;
    }

    std::array<char, 256> chunk {};
    std::string line;
    for (;;) {
        const vk_usize count = VK_CALL(file_read_handle, handle, chunk.data(), chunk.size());
        if (count == 0) {
            break;
        }

        for (vk_usize index = 0; index < count; ++index) {
            const char ch = chunk[index];
            if (ch == '\r') {
                continue;
            }
            if (ch == '\n') {
                std::string trimmed = trim_ascii(string_view_of(line));
                if (!trimmed.empty() && trimmed[0] != '#') {
                    add_external_plugin(string_view_of(trimmed), host);
                }
                line.clear();
                continue;
            }
            line.push_back(ch);
        }
    }

    if (!line.empty()) {
        std::string trimmed = trim_ascii(string_view_of(line));
        if (!trimmed.empty() && trimmed[0] != '#') {
            add_external_plugin(string_view_of(trimmed), host);
        }
    }

    VK_CALL(file_close, handle);
}

void PanelRegistry::discover(PluginHost& host)
{
    if (discovered_) {
        return;
    }

    load_external_plugins(host);
    sort_plugins();
    for (int index = 0; index < plugin_count_; ++index) {
        plugins_[index]->initialize(host);
    }

    discovered_ = true;
}

void PanelRegistry::shutdown(PluginHost& host)
{
    if (!discovered_) {
        return;
    }

    for (int index = plugin_count_ - 1; index >= 0; --index) {
        plugins_[index]->shutdown(host);
    }
    discovered_ = false;
}

void PanelRegistry::draw_menu_items()
{
    for (int index = 0; index < plugin_count_; ++index) {
        PanelPlugin& plugin = *plugins_[index];
        ImGui::MenuItem(plugin.menu_label(), nullptr, plugin.visible_ptr());
    }
}

void PanelRegistry::draw_windows(PluginHost& host)
{
    for (int index = 0; index < plugin_count_; ++index) {
        PanelPlugin& plugin = *plugins_[index];
        if (!plugin.visible()) {
            continue;
        }

        bool visible = plugin.visible();
        plugin.draw_window(host, visible);
        plugin.set_visible(visible);
    }
}

} // namespace vkgui
