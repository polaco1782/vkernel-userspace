#ifndef VGUI_PLUGIN_REGISTRY_H
#define VGUI_PLUGIN_REGISTRY_H

#include "plugin_host.h"

namespace vgui {

class PanelPlugin {
public:
    PanelPlugin(const char* id, const char* menu_label, bool default_visible = false) noexcept;
    virtual ~PanelPlugin() = default;

    [[nodiscard]] auto id() const noexcept -> const char* { return id_; }
    [[nodiscard]] auto menu_label() const noexcept -> const char* { return menu_label_; }
    [[nodiscard]] auto visible() const noexcept -> bool { return visible_; }
    auto visible_ptr() noexcept -> bool* { return &visible_; }
    void set_visible(bool visible) noexcept { visible_ = visible; }

    virtual void initialize(PluginHost& host);
    virtual void shutdown(PluginHost& host);
    virtual void draw_window(PluginHost& host, bool& visible) = 0;

private:
    const char* id_ = "";
    const char* menu_label_ = "";
    bool visible_ = false;
};

class PanelRegistry {
public:
    static constexpr int k_max_plugins = 32;

    ~PanelRegistry();

    void discover(PluginHost& host);
    void shutdown(PluginHost& host);
    void draw_menu_items();
    void draw_windows(PluginHost& host);

    [[nodiscard]] auto size() const noexcept -> int { return plugin_count_; }

private:
    void load_external_plugins(PluginHost& host);
    void add_external_plugin(vk::string_view path, PluginHost& host);
    void sort_plugins();
    [[nodiscard]] auto contains_id(const char* id) const -> bool;

    std::array<PanelPlugin*, k_max_plugins> plugins_ {};
    int plugin_count_ = 0;
    bool discovered_ = false;
};

} // namespace vgui

#endif // VGUI_PLUGIN_REGISTRY_H
