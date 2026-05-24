#ifndef VGUI_KOBJ_PANEL_H
#define VGUI_KOBJ_PANEL_H

#include "vgui_common.h"

namespace vgui {

class WindowManager;

class KobjNavigator {
public:
    auto refresh_selected() -> bool;
    void draw_window(bool& visible, WindowManager& window_manager);

private:
    struct Item {
        std::string name;
        vk_u32 type = VK_KOBJ_TYPE_STRUCT;
    };

    static constexpr size_t k_value_len = 256;
    static constexpr int k_items_max = 64;
    static constexpr int k_enum_label_max = 8;

    [[nodiscard]] auto join_path(vk::string_view parent, vk::string_view child) const -> std::string;
    [[nodiscard]] auto list_items(vk::string_view path, std::array<Item, k_items_max>& items) const -> int;
    static void sort_items(std::array<Item, k_items_max>& items, int count);
    void parse_edit_state();
    auto select_path(vk::string_view path) -> bool;
    void draw_tree_node(vk::string_view parent_path, const Item& item, int depth);
    auto status_text() const -> const char*;

    std::string selected_path_ = "sys";
    std::string value_;
    std::string path_input_ = "sys";
    std::string status_;
    vk_u64 last_refresh_tick_ = 0;
    bool auto_refresh_ = true;
    bool writable_ = false;
    bool selected_valid_ = true;
    vk_kobj_node_info_t info_ {};
    std::array<std::string, k_enum_label_max> enum_labels_ {};
    int enum_count_ = 0;
    int enum_selected_ = 0;
    std::string edit_value_;
    bool has_range_ = false;
    long long range_min_ = 0;
    long long range_max_ = 0;
};

} // namespace vgui

#endif // VGUI_KOBJ_PANEL_H
