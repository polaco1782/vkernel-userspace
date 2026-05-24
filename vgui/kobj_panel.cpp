#include "kobj_panel.h"

#include "window_manager.h"

#include <stdio.h>

namespace vgui {

static auto node_type_name(vk_u32 type) -> const char*
{
    switch (type) {
    case VK_KOBJ_TYPE_U64:
        return "u64";
    case VK_KOBJ_TYPE_I64:
        return "i64";
    case VK_KOBJ_TYPE_BOOL:
        return "bool";
    case VK_KOBJ_TYPE_STR:
        return "str";
    case VK_KOBJ_TYPE_ENUM:
        return "enum";
    case VK_KOBJ_TYPE_STRUCT:
        return "struct";
    case VK_KOBJ_TYPE_STREAM:
        return "stream";
    case VK_KOBJ_TYPE_ERR:
        return "err";
    default:
        return "unknown";
    }
}

auto KobjNavigator::join_path(vk::string_view parent, vk::string_view child) const -> std::string
{
    if (parent.empty()) {
        return string_from_view(child);
    }

    std::string result = string_from_view(parent);
    result.push_back('/');
    result.append(child.data(), child.size());
    return result;
}

auto KobjNavigator::list_items(vk::string_view path, std::array<Item, k_items_max>& items) const -> int
{
    std::array<vk_kobj_child_t, k_items_max> raw_items {};
    const std::string path_string = string_from_view(path);
    const vk_usize total = vk_kobj_list(path_string.c_str(), raw_items.data(), raw_items.size());
    const int count = static_cast<int>(total < raw_items.size() ? total : raw_items.size());

    for (int index = 0; index < count; ++index) {
        items[index].name = raw_items[index].name;
        items[index].type = raw_items[index].type;
    }
    sort_items(items, count);
    return count;
}

void KobjNavigator::sort_items(std::array<Item, k_items_max>& items, int count)
{
    for (int index = 1; index < count; ++index) {
        Item current = items[index];
        int insert = index;
        while (insert > 0) {
            const bool current_struct = current.type == VK_KOBJ_TYPE_STRUCT;
            const bool previous_struct = items[insert - 1].type == VK_KOBJ_TYPE_STRUCT;
            const bool should_move = current_struct != previous_struct
                ? current_struct
                : current.name.compare(items[insert - 1].name) < 0;
            if (!should_move) {
                break;
            }
            items[insert] = items[insert - 1];
            --insert;
        }
        items[insert] = current;
    }
}

void KobjNavigator::parse_edit_state()
{
    writable_ = info_.writable != 0;
    enum_count_ = 0;
    enum_selected_ = 0;
    has_range_ = false;
    range_min_ = 0;
    range_max_ = 0;
    edit_value_.clear();

    if (!writable_) {
        return;
    }

    if (info_.type == VK_KOBJ_TYPE_ENUM) {
        enum_count_ = static_cast<int>(info_.enum_count < k_enum_label_max ? info_.enum_count : k_enum_label_max);
        for (int index = 0; index < enum_count_; ++index) {
            enum_labels_[index] = info_.enum_labels[index];
            if (enum_labels_[index].compare(value_) == 0) {
                enum_selected_ = index;
            }
        }
        return;
    }

    if ((info_.type == VK_KOBJ_TYPE_U64 || info_.type == VK_KOBJ_TYPE_I64) && info_.range_max > info_.range_min) {
        range_min_ = static_cast<long long>(info_.range_min);
        range_max_ = static_cast<long long>(info_.range_max);
        has_range_ = true;
    }

    edit_value_ = value_;
}

auto KobjNavigator::refresh_selected() -> bool
{
    std::array<char, k_value_len> value_buffer {};

    info_ = {};
    selected_valid_ = false;
    if (vk_kobj_query(selected_path_.c_str(), value_buffer.data(), value_buffer.size(), &info_)) {
        selected_valid_ = true;
        if (info_.readable) {
            value_ = string_from_buffer(value_buffer);
        } else if (info_.type == VK_KOBJ_TYPE_STRUCT) {
            value_ = "(struct)";
        } else {
            value_ = "(not readable)";
        }
        status_.clear();
    } else {
        value_ = "(unavailable)";
        status_ = "Path not found.";
    }

    parse_edit_state();
    last_refresh_tick_ = VK_CALL(tick_count);
    return selected_valid_;
}

auto KobjNavigator::select_path(vk::string_view path) -> bool
{
    const std::string path_string = string_from_view(path);
    if (path_string.empty()) {
        return false;
    }

    selected_path_ = path_string;
    path_input_ = selected_path_;
    return refresh_selected();
}

void KobjNavigator::draw_tree_node(vk::string_view parent_path, const Item& item, int depth)
{
    if (item.name.empty() || depth > 8) {
        return;
    }

    const std::string path = join_path(parent_path, string_view_of(item.name));
    const bool is_leaf = item.type != VK_KOBJ_TYPE_STRUCT;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selected_path_.compare(path) == 0) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (is_leaf) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    const bool open = ImGui::TreeNodeEx(path.c_str(), flags, "%s", item.name.c_str());
    if (ImGui::IsItemClicked()) {
        select_path(string_view_of(path));
    }

    if (!open || is_leaf) {
        return;
    }

    std::array<Item, k_items_max> children {};
    const int child_count = list_items(string_view_of(path), children);
    if (child_count == 0) {
        ImGui::TextDisabled("(empty)");
    } else {
        for (int index = 0; index < child_count; ++index) {
            draw_tree_node(string_view_of(path), children[index], depth + 1);
        }
    }
    ImGui::TreePop();
}

auto KobjNavigator::status_text() const -> const char*
{
    return status_.empty() ? nullptr : status_.c_str();
}

void KobjNavigator::draw_window(bool& visible, WindowManager& window_manager)
{
    if (!visible) {
        return;
    }

    if (auto_refresh_) {
        const vk_u64 now = VK_CALL(tick_count);
        const vk_u32 ticks_per_second = VK_CALL(ticks_per_sec);
        const vk_u64 period = ticks_per_second != 0 ? static_cast<vk_u64>(ticks_per_second / 2u) : 0;
        if (period == 0 || now - last_refresh_tick_ >= period) {
            refresh_selected();
        }
    }

    ImGui::SetNextWindowPos(ImVec2(860.0f, 470.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(470.0f, 260.0f), ImGuiCond_FirstUseEver);

    if (!imgui_begin_window_readable_caption("KObj Navigator", &visible)) {
        ImGui::End();
        return;
    }
    window_manager.clear_focus_if_host_window_focused();

    if (ImGui::Button("Refresh")) {
        refresh_selected();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto", &auto_refresh_);

    ImGui::SetNextItemWidth(-64.0f);
    imgui_input_text("##kobj_path", path_input_);
    ImGui::SameLine();
    if (ImGui::Button("Go")) {
        if (!select_path(string_view_of(path_input_))) {
            status_ = "Unable to resolve that path.";
        }
    }

    if (const char* status = status_text()) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "%s", status);
    }

    ImGui::BeginChild("##kobj_tree", ImVec2(180.0f, 0.0f), true);
    std::array<Item, k_items_max> roots {};
    const int root_count = list_items("", roots);
    if (root_count == 0) {
        ImGui::TextDisabled("No kobj nodes.");
    } else {
        for (int index = 0; index < root_count; ++index) {
            draw_tree_node("", roots[index], 0);
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##kobj_detail", ImVec2(0.0f, 0.0f), true);
    ImGui::Text("Path: %s", selected_path_.c_str());
    ImGui::Text("Type: %s", node_type_name(info_.type));
    ImGui::Text("Status: %s", selected_valid_ ? "ok" : "missing");
    ImGui::Text("Readable: %s", info_.readable ? "yes" : "no");
    ImGui::Text("Writable: %s", info_.writable ? "yes" : "no");
    ImGui::Text("Volatile: %s", info_.volatile_node ? "yes" : "no");
    if (info_.unit[0] != '\0') {
        ImGui::Text("Unit: %s", info_.unit);
    }
    if ((info_.type == VK_KOBJ_TYPE_U64 || info_.type == VK_KOBJ_TYPE_I64) && info_.range_max > info_.range_min) {
        ImGui::Text("Range: %llu..%llu",
                    static_cast<unsigned long long>(info_.range_min),
                    static_cast<unsigned long long>(info_.range_max));
    }
    if (info_.type == VK_KOBJ_TYPE_ENUM && info_.enum_count > 0) {
        ImGui::SeparatorText("Labels");
        for (vk_u32 index = 0; index < info_.enum_count && index < k_enum_label_max; ++index) {
            ImGui::TextUnformatted(info_.enum_labels[index]);
        }
    }

    ImGui::SeparatorText("Value");
    if (info_.readable) {
        ImGui::TextWrapped("%s", value_.empty() ? "(empty)" : value_.c_str());
    } else {
        ImGui::TextDisabled("(not readable)");
    }

    if (writable_) {
        ImGui::SeparatorText("Edit");

        if (info_.type == VK_KOBJ_TYPE_ENUM && enum_count_ > 0) {
            std::array<const char*, k_enum_label_max> items {};
            for (int index = 0; index < enum_count_; ++index) {
                items[index] = enum_labels_[index].c_str();
            }

            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##kobj_enum", &enum_selected_, items.data(), enum_count_)) {
                if (!vk_kobj_set_value(selected_path_.c_str(), enum_labels_[enum_selected_].c_str())) {
                    status_ = "Set failed.";
                }
                refresh_selected();
            }
        } else if (info_.type == VK_KOBJ_TYPE_BOOL) {
            bool current = string_equals(value_, "yes") || string_equals(value_, "true");
            if (ImGui::Checkbox("Enabled##kobj_bool", &current)) {
                if (!vk_kobj_set_value(selected_path_.c_str(), current ? "true" : "false")) {
                    status_ = "Set failed.";
                }
                refresh_selected();
            }
        } else if ((info_.type == VK_KOBJ_TYPE_U64 || info_.type == VK_KOBJ_TYPE_I64) && has_range_) {
            int current = static_cast<int>(parse_i64(string_view_of(value_)));
            const int min_value = static_cast<int>(range_min_);
            const int max_value = static_cast<int>(range_max_);

            ImGui::SetNextItemWidth(-80.0f);
            if (ImGui::SliderInt("##kobj_slider", &current, min_value, max_value)) {
                edit_value_ = string_from_i64(current);
            }
            ImGui::SameLine();
            if (ImGui::Button("Set##kobj_range")) {
                if (!vk_kobj_set_value(selected_path_.c_str(), edit_value_.c_str())) {
                    status_ = "Set failed.";
                }
                refresh_selected();
            }
        } else {
            ImGui::SetNextItemWidth(-80.0f);
            imgui_input_text("##kobj_edit", edit_value_);
            ImGui::SameLine();
            if (ImGui::Button("Set##kobj_text")) {
                if (!vk_kobj_set_value(selected_path_.c_str(), edit_value_.c_str())) {
                    status_ = "Set failed.";
                }
                refresh_selected();
            }
        }
    }

    ImGui::EndChild();
    ImGui::End();
}

} // namespace vgui
