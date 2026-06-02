#include "vkfm_panel.h"

#include "console_log.h"
#include "icons.h"
#include "window_manager.h"

#include <stdio.h>

namespace vkgui {

namespace {

constexpr vk_u32 k_launch_width = 640;
constexpr vk_u32 k_launch_height = 400;

auto is_separator(char ch) -> bool
{
    return ch == '/' || ch == '\\';
}

auto is_ascii_printable(char ch) -> bool
{
    return ch >= 32 && ch <= 126;
}

auto path_is_root(vk::string_view path) -> bool
{
    return view_equals(path, "/");
}

} // namespace

auto VkfmPanel::query_default_path() const -> std::string
{
    std::array<char, 128> response {};
    std::array<char, 96> value {};

    vk_kobj_rpc_path_json("get", "fs/root_path", response.data(), response.size());
    if (json_extract_string(response.data(), "value", value) && value[0] != '\0') {
        return string_from_buffer(value);
    }

    return "/";
}

auto VkfmPanel::canonicalize_absolute_path(vk::string_view path) const -> std::string
{
    std::array<std::string, 32> parts {};
    int part_count = 0;
    vk_usize index = 0;

    while (index < path.size()) {
        while (index < path.size() && is_separator(path[index])) {
            ++index;
        }
        if (index >= path.size()) {
            break;
        }

        const vk_usize start = index;
        while (index < path.size() && !is_separator(path[index])) {
            ++index;
        }

        const vk::string_view part = subview(path, start, index - start);
        if (part.empty() || view_equals(part, ".")) {
            continue;
        }
        if (view_equals(part, "..")) {
            if (part_count > 0) {
                --part_count;
            }
            continue;
        }
        if (part_count < static_cast<int>(parts.size())) {
            parts[part_count++] = string_from_view(part);
        }
    }

    std::string result("/");
    for (int part_index = 0; part_index < part_count; ++part_index) {
        if (part_index != 0) {
            result.push_back('/');
        }
        result += parts[part_index];
    }
    return result;
}

auto VkfmPanel::resolve_input_path(vk::string_view raw_path) const -> std::string
{
    std::string base = current_path_.empty() ? query_default_path() : current_path_;
    std::string text = trim_ascii(raw_path);
    if (text.empty()) {
        return canonicalize_absolute_path(string_view_of(base));
    }

    if (is_separator(text[0])) {
        return canonicalize_absolute_path(string_view_of(text));
    }

    if (!base.empty() && !view_equals(base, "/")) {
        std::string combined = base;
        combined.push_back('/');
        combined += text;
        text = combined;
    } else {
        std::string combined("/");
        combined += text;
        text = combined;
    }

    return canonicalize_absolute_path(string_view_of(text));
}

auto VkfmPanel::join_path(vk::string_view parent, vk::string_view child) const -> std::string
{
    if (parent.empty() || view_equals(parent, "/")) {
        std::string result("/");
        result.append(child.data(), child.size());
        return result;
    }

    std::string result = string_from_view(parent);
    result.push_back('/');
    result.append(child.data(), child.size());
    return result;
}

auto VkfmPanel::parent_path(vk::string_view path) const -> std::string
{
    std::string normalized = canonicalize_absolute_path(path);
    if (normalized == "/") {
        return normalized;
    }

    vk_usize end = string_view_of(normalized).size();
    while (end > 1 && normalized[end - 1] == '/') {
        --end;
    }
    while (end > 1 && normalized[end - 1] != '/') {
        --end;
    }
    if (end <= 1) {
        return "/";
    }

    normalized.resize(static_cast<size_t>(end - 1));
    return normalized;
}

auto VkfmPanel::parse_item_record(vk::string_view record, Entry& out) const -> bool
{
    DirectoryListEntry parsed {};
    if (!parse_directory_list_item(record, parsed)) {
        return false;
    }
    out.name = std::move(parsed.name);
    out.is_directory = parsed.is_directory;
    out.size = parsed.size;
    return true;
}

auto VkfmPanel::selected_path() const -> std::string
{
    if (selected_index_ < 0 || selected_index_ >= entry_count_) {
        return current_path_;
    }
    return join_path(string_view_of(current_path_), string_view_of(entries_[selected_index_].name));
}

auto VkfmPanel::load_directory_entries(vk::string_view requested,
                                       std::array<Entry, k_items_max>& out_entries,
                                       int& out_count,
                                       std::string& error,
                                       ConsoleLog* log) const -> bool
{
    std::array<char, 12288> response {};
    std::array<std::array<char, k_item_len>, k_items_max> raw_items {};

    vk_kobj_rpc_path_json("fs_list",
                          string_from_view(requested).c_str(),
                          response.data(),
                          response.size());
    if (!vk_kobj_response_ok(response.data())) {
        std::array<char, 128> error_buffer {};
        if (json_extract_string(response.data(), "error", error_buffer)) {
            error = string_from_buffer(error_buffer);
        } else {
            error = "Failed to open directory.";
        }
        if (log != nullptr) {
            log->addf("vkfm: %s (%s)", error.c_str(), string_from_view(requested).c_str());
        }
        out_count = 0;
        return false;
    }

    const int count = vk_json_extract_string_array_field(response.data(),
                                                         "items",
                                                         raw_items[0].data(),
                                                         static_cast<vk_usize>(k_item_len),
                                                         k_items_max);

    out_count = 0;
    for (int index = 0; index < count && out_count < k_items_max; ++index) {
        Entry parsed {};
        if (!parse_item_record(buffer_view(raw_items[index]), parsed)) {
            continue;
        }
        out_entries[out_count++] = parsed;
    }

    sort_entries(out_entries, out_count);
    error.clear();
    return true;
}

auto VkfmPanel::append_tree_node(int parent_index, vk::string_view path) -> int
{
    if (tree_node_count_ >= k_tree_nodes_max) {
        return -1;
    }

    DirectoryNode& node = tree_nodes_[tree_node_count_];
    node = DirectoryNode {};
    node.path = string_from_view(path);
    node.parent_index = parent_index;

    if (path_is_root(path)) {
        node.label = "/";
    } else {
        node.label = string_from_view(path_basename(path));
        if (node.label.empty()) {
            node.label = node.path;
        }
    }

    return tree_node_count_++;
}

auto VkfmPanel::find_tree_child(int parent_index, vk::string_view path) const -> int
{
    if (parent_index < 0 || parent_index >= tree_node_count_) {
        return -1;
    }

    const DirectoryNode& parent = tree_nodes_[parent_index];
    for (int offset = 0; offset < parent.child_count; ++offset) {
        const int child_index = parent.first_child + offset;
        if (child_index >= 0
            && child_index < tree_node_count_
            && view_equals(tree_nodes_[child_index].path, path)) {
            return child_index;
        }
    }

    return -1;
}

void VkfmPanel::sort_entries(std::array<Entry, k_items_max>& entries, int count) const
{
    for (int index = 1; index < count; ++index) {
        Entry entry = entries[index];
        int insert_index = index;
        while (insert_index > 0) {
            const Entry& previous = entries[insert_index - 1];
            const bool directories_first = entry.is_directory && !previous.is_directory;
            const bool names_precede = entry.is_directory == previous.is_directory
                                    && entry.name.compare(previous.name) < 0;
            if (!directories_first && !names_precede) {
                break;
            }
            entries[insert_index] = previous;
            --insert_index;
        }
        entries[insert_index] = entry;
    }
}

void VkfmPanel::refresh_listing(ConsoleLog* log)
{
    std::string previously_selected;
    if (selected_index_ >= 0 && selected_index_ < entry_count_) {
        previously_selected = entries_[selected_index_].name;
    }

    const std::string requested = resolve_input_path(string_view_of(path_input_));
    std::string error;
    int count = 0;
    if (!load_directory_entries(string_view_of(requested), entries_, count, error, log)) {
        status_ = error;
        return;
    }

    entry_count_ = count;
    selected_index_ = -1;
    preview_.clear();
    current_path_ = requested;
    path_input_ = current_path_;
    sync_tree_to_current_path(log);

    if (entry_count_ == 0) {
        status_ = "Directory is empty.";
        return;
    }

    status_ = string_from_i64(entry_count_);
    status_ += entry_count_ == 1 ? " item" : " items";

    for (int index = 0; index < entry_count_; ++index) {
        if (entries_[index].name == previously_selected) {
            select_entry(index, false);
            return;
        }
    }

    select_entry(0, false);
}

void VkfmPanel::select_entry(int index, bool load_preview)
{
    if (index < 0 || index >= entry_count_) {
        selected_index_ = -1;
        preview_.clear();
        return;
    }

    selected_index_ = index;
    preview_.clear();
    if (load_preview) {
        load_preview_for_selection();
    }
}

void VkfmPanel::load_preview_for_selection()
{
    preview_.clear();
    if (selected_index_ < 0 || selected_index_ >= entry_count_) {
        return;
    }

    const Entry& entry = entries_[selected_index_];
    const std::string path = selected_path();
    if (entry.is_directory) {
        preview_ = "Directory\nOpen it to browse its contents.";
        return;
    }

    const vk_file_handle_t handle = VK_CALL(file_open, path.c_str(), "r");
    if (handle == static_cast<vk_file_handle_t>(0)) {
        preview_ = "Unable to open file.";
        return;
    }

    std::array<char, 256> chunk {};
    vk_usize total = 0;

    while (total < k_preview_limit) {
        const vk_usize remaining = k_preview_limit - total;
        const vk_usize chunk_size = remaining < static_cast<vk_usize>(chunk.size())
                                  ? remaining
                                  : static_cast<vk_usize>(chunk.size());
        const vk_usize read_count = VK_CALL(file_read_handle, handle, chunk.data(), chunk_size);
        if (read_count == 0) {
            break;
        }

        for (vk_usize index = 0; index < read_count; ++index) {
            const char ch = chunk[index];
            if (ch == '\r') {
                continue;
            }
            if (is_ascii_printable(ch) || ch == '\n' || ch == '\t') {
                preview_.push_back(ch);
            } else {
                preview_.push_back('.');
            }
        }
        total += read_count;
        if (read_count < chunk_size) {
            break;
        }
    }

    const bool truncated = entry.size > total;
    VK_CALL(file_close, handle);

    if (preview_.empty()) {
        preview_ = "(empty file)";
    }
    if (truncated) {
        preview_ += "\n\n[preview truncated]";
    }
}

void VkfmPanel::open_selected(ConsoleLog& log)
{
    if (selected_index_ < 0 || selected_index_ >= entry_count_) {
        return;
    }

    const Entry& entry = entries_[selected_index_];
    const std::string path = selected_path();
    if (entry.is_directory) {
        path_input_ = path;
        refresh_listing(&log);
        log.addf("vkfm: opened %s", path.c_str());
        return;
    }

    load_preview_for_selection();
    log.addf("vkfm: previewed %s", path.c_str());
}

void VkfmPanel::ensure_tree_initialized()
{
    if (root_node_index_ >= 0) {
        return;
    }
    const std::string root_path = current_path_.empty() ? query_default_path() : current_path_;
    reset_tree(string_view_of(root_path));
}

void VkfmPanel::reset_tree(vk::string_view root_path)
{
    tree_node_count_ = 0;
    root_node_index_ = -1;

    const std::string normalized = canonicalize_absolute_path(root_path);
    root_node_index_ = append_tree_node(-1, string_view_of(normalized));
    if (root_node_index_ >= 0) {
        tree_nodes_[root_node_index_].open_on_next_draw = true;
    }
}

void VkfmPanel::load_tree_children(int node_index, ConsoleLog* log)
{
    if (node_index < 0 || node_index >= tree_node_count_) {
        return;
    }

    DirectoryNode& node = tree_nodes_[node_index];
    if (node.children_loaded) {
        return;
    }

    std::array<Entry, k_items_max> loaded_entries {};
    std::string error;
    int loaded_count = 0;
    if (!load_directory_entries(string_view_of(node.path), loaded_entries, loaded_count, error, log)) {
        node.children_loaded = true;
        node.load_failed = true;
        return;
    }

    node.first_child = tree_node_count_;
    node.child_count = 0;
    node.children_loaded = true;
    node.load_failed = false;

    for (int index = 0; index < loaded_count && tree_node_count_ < k_tree_nodes_max; ++index) {
        const Entry& entry = loaded_entries[index];
        if (!entry.is_directory) {
            continue;
        }

        const std::string child_path = join_path(string_view_of(node.path), string_view_of(entry.name));
        if (append_tree_node(node_index, string_view_of(child_path)) >= 0) {
            ++node.child_count;
        }
    }
}

void VkfmPanel::sync_tree_to_current_path(ConsoleLog* log)
{
    ensure_tree_initialized();
    if (root_node_index_ < 0 || current_path_.empty()) {
        return;
    }

    DirectoryNode& root = tree_nodes_[root_node_index_];
    root.open_on_next_draw = true;
    if (current_path_ == root.path) {
        return;
    }

    int current_node = root_node_index_;
    vk_usize start = string_view_of(root.path).size();
    if (path_is_root(string_view_of(root.path))) {
        start = 1;
    } else if (start < current_path_.size() && current_path_[start] == '/') {
        ++start;
    }

    while (start <= current_path_.size()) {
        vk_usize end = start;
        while (end < current_path_.size() && current_path_[end] != '/') {
            ++end;
        }

        const vk::string_view partial = subview(string_view_of(current_path_), 0, end);
        load_tree_children(current_node, log);
        const int child_index = find_tree_child(current_node, partial);
        if (child_index < 0) {
            break;
        }

        tree_nodes_[child_index].open_on_next_draw = true;
        current_node = child_index;
        if (end >= current_path_.size()) {
            break;
        }
        start = end + 1;
    }
}

void VkfmPanel::draw_sidebar(ConsoleLog& log)
{
    ensure_tree_initialized();

    ImGui::TextDisabled(ICON_FA_FOLDER_TREE " Folders");
    ImGui::Separator();
    ImGui::BeginChild("##vkfm_sidebar_tree", ImVec2(0.0f, 0.0f), false);
    if (root_node_index_ >= 0) {
        draw_tree_node(root_node_index_, log);
    }
    ImGui::EndChild();
}

void VkfmPanel::draw_tree_node(int node_index, ConsoleLog& log)
{
    if (node_index < 0 || node_index >= tree_node_count_) {
        return;
    }

    DirectoryNode& node = tree_nodes_[node_index];
    if (node.open_on_next_draw) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        node.open_on_next_draw = false;
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
    if (node.path == current_path_) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (node.children_loaded && node.child_count == 0) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    const char* icon = path_is_root(string_view_of(node.path)) ? ICON_FA_HARD_DRIVE : ICON_FA_FOLDER;
    const bool open = ImGui::TreeNodeEx(node.path.c_str(), flags, "%s %s", icon, node.label.c_str());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        path_input_ = node.path;
        refresh_listing(&log);
    }

    if (!open) {
        return;
    }

    load_tree_children(node_index, &log);
    if (node.load_failed) {
        ImGui::TextDisabled("Unable to read this folder.");
    } else {
        for (int offset = 0; offset < node.child_count; ++offset) {
            draw_tree_node(node.first_child + offset, log);
        }
    }

    ImGui::TreePop();
}

void VkfmPanel::draw_entry_tile(int index, float tile_width, float tile_height, ConsoleLog& log)
{
    const Entry& entry = entries_[index];
    const bool is_selected = selected_index_ == index;

    ImGui::PushID(index);
    const ImVec2 tile_size(tile_width, tile_height);
    const ImVec2 tile_pos = ImGui::GetCursorScreenPos();
    const bool activated = ImGui::InvisibleButton("##tile", tile_size);
    const bool hovered = ImGui::IsItemHovered();
    const bool double_clicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

    const ImGuiStyle& style = ImGui::GetStyle();
    const ImU32 fill = ImGui::GetColorU32(is_selected ? ImGuiCol_Header
                                      : hovered ? ImGuiCol_ButtonHovered
                                                : ImGuiCol_Button);
    const ImU32 border = ImGui::GetColorU32(is_selected ? ImGuiCol_HeaderActive : ImGuiCol_Border);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(tile_pos,
                             ImVec2(tile_pos.x + tile_size.x, tile_pos.y + tile_size.y),
                             fill,
                             style.FrameRounding);
    draw_list->AddRect(tile_pos,
                       ImVec2(tile_pos.x + tile_size.x, tile_pos.y + tile_size.y),
                       border,
                       style.FrameRounding);

    const char* icon = file_icon_for_entry(string_view_of(entry.name), entry.is_directory);
    const ImVec2 icon_size = ImGui::CalcTextSize(icon);
    ImGui::SetCursorScreenPos(ImVec2(tile_pos.x + (tile_size.x - icon_size.x) * 0.5f, tile_pos.y + 10.0f));
    ImGui::TextUnformatted(icon);

    ImGui::SetCursorScreenPos(ImVec2(tile_pos.x + 8.0f, tile_pos.y + 34.0f));
    ImGui::PushTextWrapPos(tile_pos.x + tile_size.x - 8.0f);
    ImGui::TextWrapped("%s", truncate_with_ellipsis(string_view_of(entry.name), 28).c_str());
    ImGui::PopTextWrapPos();

    const std::string detail = entry.is_directory ? "Folder" : format_byte_size(entry.size);
    ImGui::SetCursorScreenPos(ImVec2(tile_pos.x + 8.0f, tile_pos.y + tile_size.y - 22.0f));
    ImGui::TextDisabled("%s", detail.c_str());

    if (activated) {
        select_entry(index, false);
    }
    if (double_clicked) {
        select_entry(index, false);
        open_selected(log);
    }

    ImGui::PopID();
}

void VkfmPanel::draw_entry_grid(ConsoleLog& log)
{
    if (entry_count_ == 0) {
        ImGui::TextDisabled("No items in this folder.");
        return;
    }

    const float tile_width = 140.0f;
    const float tile_height = 96.0f;
    float available_width = ImGui::GetContentRegionAvail().x;
    if (available_width < tile_width) {
        available_width = tile_width;
    }

    int columns = static_cast<int>(available_width / tile_width);
    if (columns < 1) {
        columns = 1;
    }

    if (ImGui::BeginTable("##vkfm_tiles",
                          columns,
                          ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX)) {
        for (int index = 0; index < entry_count_; ++index) {
            ImGui::TableNextColumn();
            draw_entry_tile(index, tile_width - 10.0f, tile_height, log);
        }
        ImGui::EndTable();
    }
}

void VkfmPanel::draw_selection_panel(WindowManager& window_manager, ConsoleLog& log)
{
    if (selected_index_ < 0 || selected_index_ >= entry_count_) {
        ImGui::TextDisabled("Select a folder or file to inspect it.");
        return;
    }

    const Entry& entry = entries_[selected_index_];
    const std::string path = selected_path();
    ImGui::Text("%s %s",
                file_icon_for_entry(string_view_of(entry.name), entry.is_directory),
                entry.name.c_str());
    ImGui::TextDisabled("%s", path.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("| %s", entry.is_directory ? "Folder" : format_byte_size(entry.size).c_str());

    if (ImGui::Button(entry.is_directory ? ICON_FA_FOLDER_OPEN " Open" : ICON_FA_MAGNIFYING_GLASS " Preview")) {
        open_selected(log);
    }
    if (!entry.is_directory && is_vbin_program_path(string_view_of(path))) {
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_ROCKET " Launch")) {
            (void)window_manager.launch_windowed_app(string_view_of(path), k_launch_width, k_launch_height);
            log.addf("vkfm: launched %s", path.c_str());
        }
    }

    ImGui::BeginChild("##vkfm_preview", ImVec2(0.0f, 0.0f), true);
    if (preview_.empty()) {
        ImGui::TextDisabled(entry.is_directory
                                ? "Open this folder to browse into it."
                                : "Use Preview to inspect file contents.");
    } else {
        ImGui::TextWrapped("%s", preview_.c_str());
    }
    ImGui::EndChild();
}

void VkfmPanel::draw_window(bool& visible, WindowManager& window_manager, ConsoleLog& log)
{
    if (!visible) {
        return;
    }

    if (!initialized_) {
        current_path_ = query_default_path();
        path_input_ = current_path_;
        reset_tree(string_view_of(current_path_));
        refresh_listing();
        initialized_ = true;
    }

    ImGui::SetNextWindowPos(ImVec2(320.0f, 30.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(900.0f, 700.0f), ImGuiCond_FirstUseEver);

    if (!imgui_begin_window_readable_caption(ICON_FA_FOLDER_OPEN " vkfm Explorer", &visible)) {
        ImGui::End();
        return;
    }
    window_manager.clear_focus_if_host_window_focused();

    if (ImGui::Button(ICON_FA_HOUSE " Home")) {
        path_input_ = query_default_path();
        refresh_listing(&log);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ARROW_UP " Up")) {
        path_input_ = parent_path(string_view_of(current_path_));
        refresh_listing(&log);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ARROW_ROTATE_RIGHT " Refresh")) {
        refresh_listing(&log);
    }

    ImGui::SetNextItemWidth(-70.0f);
    const bool submit_path = imgui_input_text("##vkfm_path",
                                              path_input_,
                                              ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if (ImGui::Button("Go") || submit_path) {
        refresh_listing(&log);
    }

    ImGui::TextDisabled("%s %s", ICON_FA_FOLDER_OPEN, current_path_.c_str());
    if (!status_.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("| %s", status_.c_str());
    }

    const ImGuiTableFlags layout_flags = ImGuiTableFlags_Resizable
                                       | ImGuiTableFlags_BordersInnerV
                                       | ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("##vkfm_layout", 2, layout_flags, ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 240.0f);
        ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch, 0.0f);

        ImGui::TableNextColumn();
        draw_sidebar(log);

        ImGui::TableNextColumn();
        ImGui::TextDisabled(ICON_FA_TABLE_CELLS_LARGE " Items");
        ImGui::Separator();
        ImGui::BeginChild("##vkfm_content", ImVec2(0.0f, 0.0f), false);
        ImGui::BeginChild("##vkfm_grid", ImVec2(0.0f, -210.0f), false);
        draw_entry_grid(log);
        ImGui::EndChild();
        ImGui::SeparatorText("Selection");
        draw_selection_panel(window_manager, log);
        ImGui::EndChild();

        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace vkgui
