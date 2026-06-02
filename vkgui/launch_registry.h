#ifndef VKGUI_LAUNCH_REGISTRY_H
#define VKGUI_LAUNCH_REGISTRY_H

#include "vkgui_common.h"

namespace vkgui {

class ConsoleLog;

struct LaunchMenuEntry {
    std::string path;
    std::string label;
};

class LaunchRegistry {
public:
    void refresh(ConsoleLog& log);

    [[nodiscard]] auto empty() const -> bool { return count_ == 0; }
    [[nodiscard]] auto size() const -> int { return count_; }
    [[nodiscard]] auto entry(int index) const -> const LaunchMenuEntry& { return entries_[index]; }

private:
    static constexpr int k_capacity = 64;
    static constexpr int k_item_len = 192;
    static constexpr int k_items_max = 128;
    static constexpr vk_usize k_response_overhead = 3 * 1024;
    static constexpr vk_usize k_response_max = (static_cast<vk_usize>(k_item_len)
                                              * static_cast<vk_usize>(k_items_max))
                                             + k_response_overhead;

    [[nodiscard]] auto load_from_bin_directory(ConsoleLog& log) -> bool;
    void add_app(vk::string_view path);
    [[nodiscard]] auto exists(vk::string_view path) const -> bool;
    void sort();

    std::array<LaunchMenuEntry, k_capacity> entries_ {};
    std::array<char, k_response_max> response_ {};
    std::array<std::array<char, k_item_len>, k_items_max> raw_items_ {};
    int count_ = 0;
};

} // namespace vkgui

#endif // VKGUI_LAUNCH_REGISTRY_H
