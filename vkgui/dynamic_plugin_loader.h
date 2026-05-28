#ifndef VKGUI_DYNAMIC_PLUGIN_LOADER_H
#define VKGUI_DYNAMIC_PLUGIN_LOADER_H

#include "plugin_api.h"
#include "plugin_host.h"

namespace vkgui {

class DynamicPluginModule {
public:
    DynamicPluginModule() = default;
    ~DynamicPluginModule();

    DynamicPluginModule(const DynamicPluginModule&) = delete;
    auto operator=(const DynamicPluginModule&) -> DynamicPluginModule& = delete;

    DynamicPluginModule(DynamicPluginModule&& other) noexcept;
    auto operator=(DynamicPluginModule&& other) noexcept -> DynamicPluginModule&;

    [[nodiscard]] auto load(vk::string_view path, PluginHost& host) -> bool;
    void unload(PluginHost& host);
    void draw_window(PluginHost& host, bool& visible);

    [[nodiscard]] auto valid() const noexcept -> bool { return valid_; }
    [[nodiscard]] auto descriptor() const noexcept -> const vkgui_plugin_descriptor_t& { return descriptor_; }

private:
    using init_array_fn = void (*)();

    void reset() noexcept;
    void run_fini_array() const;

    void* raw_allocation_ = nullptr;
    unsigned char* image_base_ = nullptr;
    vk_usize image_size_ = 0;
    init_array_fn* fini_array_ = nullptr;
    vk_usize fini_count_ = 0;
    vkgui_plugin_descriptor_t descriptor_ {};
    bool valid_ = false;
};

} // namespace vkgui

#endif // VKGUI_DYNAMIC_PLUGIN_LOADER_H
