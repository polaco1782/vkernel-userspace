#include "dynamic_plugin_loader.h"
#include "plugin_registry.h"

#include "console_log.h"
#include "imgui/imgui.h"

#include <new>
#include <string.h>

namespace vkgui {

namespace {

constexpr auto k_plugin_directory_path = "/data/vkgui/plugins";
constexpr auto k_plugin_suffix = ".vplg";
constexpr int k_plugin_item_len = 96;
constexpr int k_plugin_items_max = 96;
constexpr vk_usize k_plugin_response_overhead = 3 * 1024;
constexpr vk_usize k_plugin_response_max = (static_cast<vk_usize>(k_plugin_item_len)
                                          * static_cast<vk_usize>(k_plugin_items_max))
                                         + k_plugin_response_overhead;

struct directory_item {
    std::string name;
    bool is_directory = false;
};

auto parse_directory_item(vk::string_view record, directory_item& out) -> bool
{
    if (record.size() < 4 || record[1] != '\t') {
        return false;
    }

    vk_usize second_tab = k_not_found;
    for (vk_usize index = 2; index < record.size(); ++index) {
        if (record[index] == '\t') {
            second_tab = index;
            break;
        }
    }
    if (second_tab == k_not_found || second_tab <= 2) {
        return false;
    }
    if (record[0] != 'D' && record[0] != 'F') {
        return false;
    }

    out.is_directory = record[0] == 'D';
    out.name = string_from_view(subview(record, 2, second_tab - 2));
    return !out.name.empty();
}

auto join_plugin_path(vk::string_view name) -> std::string
{
    std::string path = k_plugin_directory_path;
    path.push_back('/');
    path.append(name.data(), name.size());
    return path;
}

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
    std::array<char, k_plugin_response_max> response {};
    std::array<std::array<char, k_plugin_item_len>, k_plugin_items_max> raw_items {};

    vk_kobj_rpc_path_json("fs_list", k_plugin_directory_path, response.data(), response.size());
    if (!vk_kobj_response_ok(response.data())) {
        host.log.add("vkGUI plugin loader: plugin directory not found.");
        return;
    }

    const int item_count = vk_json_extract_string_array_field(response.data(),
                                                              "items",
                                                              raw_items[0].data(),
                                                              static_cast<vk_usize>(k_plugin_item_len),
                                                              k_plugin_items_max);

    for (int index = 0; index < item_count; ++index) {
        directory_item item {};
        if (!parse_directory_item(buffer_view(raw_items[index]), item)) {
            continue;
        }
        if (item.is_directory || !ends_with(string_view_of(item.name), k_plugin_suffix)) {
            continue;
        }

        const std::string path = join_plugin_path(string_view_of(item.name));
        add_external_plugin(string_view_of(path), host);
    }
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
