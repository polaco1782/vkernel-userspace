#ifndef VKGUI_VKFM_PANEL_H
#define VKGUI_VKFM_PANEL_H

#include "vkgui_common.h"

namespace vkgui {

class ConsoleLog;
class WindowManager;

class VkfmPanel {
public:
    void draw_window(bool& visible, WindowManager& window_manager, ConsoleLog& log);

private:
    static constexpr int k_items_max = 128;
    static constexpr int k_item_len = 192;
    static constexpr int k_tree_nodes_max = 256;
    static constexpr vk_usize k_preview_limit = 8192;

    struct Entry {
        std::string name;
        bool is_directory = false;
        vk_u64 size = 0;
    };

    struct DirectoryNode {
        std::string path;
        std::string label;
        int parent_index = -1;
        int first_child = -1;
        int child_count = 0;
        bool children_loaded = false;
        bool load_failed = false;
        bool open_on_next_draw = false;
    };

    [[nodiscard]] auto query_default_path() const -> std::string;
    [[nodiscard]] auto canonicalize_absolute_path(vk::string_view path) const -> std::string;
    [[nodiscard]] auto resolve_input_path(vk::string_view raw_path) const -> std::string;
    [[nodiscard]] auto join_path(vk::string_view parent, vk::string_view child) const -> std::string;
    [[nodiscard]] auto parent_path(vk::string_view path) const -> std::string;
    [[nodiscard]] auto parse_item_record(vk::string_view record, Entry& out) const -> bool;
    [[nodiscard]] auto selected_path() const -> std::string;
    [[nodiscard]] auto load_directory_entries(vk::string_view requested,
                                              std::array<Entry, k_items_max>& out_entries,
                                              int& out_count,
                                              std::string& error,
                                              ConsoleLog* log = nullptr) const -> bool;
    [[nodiscard]] auto append_tree_node(int parent_index, vk::string_view path) -> int;
    [[nodiscard]] auto find_tree_child(int parent_index, vk::string_view path) const -> int;
    void sort_entries(std::array<Entry, k_items_max>& entries, int count) const;
    void refresh_listing(ConsoleLog* log = nullptr);
    void select_entry(int index, bool load_preview);
    void load_preview_for_selection();
    void open_selected(ConsoleLog& log);
    void ensure_tree_initialized();
    void reset_tree(vk::string_view root_path);
    void load_tree_children(int node_index, ConsoleLog* log = nullptr);
    void sync_tree_to_current_path(ConsoleLog* log = nullptr);
    void draw_sidebar(ConsoleLog& log);
    void draw_tree_node(int node_index, ConsoleLog& log);
    void draw_entry_grid(ConsoleLog& log);
    void draw_entry_tile(int index, float tile_width, float tile_height, ConsoleLog& log);
    void draw_selection_panel(WindowManager& window_manager, ConsoleLog& log);

    std::array<Entry, k_items_max> entries_ {};
    std::array<DirectoryNode, k_tree_nodes_max> tree_nodes_ {};
    int entry_count_ = 0;
    int selected_index_ = -1;
    int tree_node_count_ = 0;
    int root_node_index_ = -1;
    bool initialized_ = false;
    std::string current_path_;
    std::string path_input_;
    std::string status_;
    std::string preview_;
};

} // namespace vkgui

#endif // VKGUI_VKFM_PANEL_H
