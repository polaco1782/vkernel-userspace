#ifndef VKGUI_COMMON_H
#define VKGUI_COMMON_H

#include "../include/vk.h"
#include "deps/imgui/imgui.h"
#include "imgui_impl_vk.h"

#include "vkernel/types.h"
#include "vkernel/unique_ptr.h"

#include <array>
#include <stdio.h>
#include <string>

namespace vkgui {

inline constexpr vk_usize k_not_found = static_cast<vk_usize>(-1);

inline auto is_ascii_space(char ch) -> bool
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

inline auto ascii_lower(char ch) -> char
{
    if (ch >= 'A' && ch <= 'Z') {
        return static_cast<char>(ch - 'A' + 'a');
    }
    return ch;
}

inline auto ascii_length(const char* text) -> vk_usize
{
    if (text == nullptr) {
        return 0;
    }

    vk_usize length = 0;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

inline auto string_from_view(vk::string_view view) -> std::string
{
    std::string result;
    if (!view.empty()) {
        result.append(view.data(), view.size());
    }
    return result;
}

inline void append_decimal(std::string& out, unsigned long long value)
{
    if (value >= 10ULL) {
        append_decimal(out, value / 10ULL);
    }
    out.push_back(static_cast<char>('0' + (value % 10ULL)));
}

inline auto string_from_i64(long long value) -> std::string
{
    std::string result;
    if (value < 0) {
        result.push_back('-');
        append_decimal(result, static_cast<unsigned long long>(-(value + 1)) + 1ULL);
    } else {
        append_decimal(result, static_cast<unsigned long long>(value));
    }
    return result;
}

inline auto string_view_of(const std::string& value) -> vk::string_view
{
    return vk::string_view(value.c_str(), value.size());
}

inline auto subview(vk::string_view view, vk_usize start, vk_usize count) -> vk::string_view
{
    if (start > view.size()) {
        return vk::string_view();
    }

    vk_usize available = view.size() - start;
    if (count > available) {
        count = available;
    }

    return vk::string_view(view.data() + start, count);
}

template <size_t N>
inline auto buffer_view(const std::array<char, N>& buffer) -> vk::string_view
{
    if constexpr (N == 0) {
        return vk::string_view();
    }

    return vk::string_view(buffer.data());
}

template <size_t N>
inline auto string_from_buffer(const std::array<char, N>& buffer) -> std::string
{
    if constexpr (N == 0) {
        return std::string();
    }

    return std::string(buffer.data());
}

inline auto string_equals(vk::string_view lhs, vk::string_view rhs) -> bool
{
    return lhs.compare(rhs);
}

inline auto view_equals(vk::string_view lhs, vk::string_view rhs) -> bool
{
    return lhs.compare(rhs);
}

inline auto string_equals(const std::string& lhs, vk::string_view rhs) -> bool
{
    return string_equals(string_view_of(lhs), rhs);
}

inline auto string_equals(vk::string_view lhs, const char* rhs) -> bool
{
    return string_equals(lhs, vk::string_view(rhs));
}

inline auto string_equals(const std::string& lhs, const char* rhs) -> bool
{
    return string_equals(string_view_of(lhs), vk::string_view(rhs));
}

inline auto view_equals(const std::string& lhs, vk::string_view rhs) -> bool
{
    return view_equals(string_view_of(lhs), rhs);
}

inline auto view_equals(vk::string_view lhs, const char* rhs) -> bool
{
    return view_equals(lhs, vk::string_view(rhs));
}

inline auto view_equals(const std::string& lhs, const char* rhs) -> bool
{
    return view_equals(string_view_of(lhs), vk::string_view(rhs));
}

inline auto ends_with(vk::string_view text, vk::string_view suffix) -> bool
{
    if (suffix.size() > text.size()) {
        return false;
    }

    return subview(text, text.size() - suffix.size(), suffix.size()).compare(suffix);
}

inline auto path_basename(vk::string_view path) -> vk::string_view
{
    vk_usize start = 0;
    for (vk_usize index = 0; index < path.size(); ++index) {
        if (path[index] == '/' || path[index] == '\\') {
            start = index + 1;
        }
    }

    return subview(path, start, path.size() - start);
}

inline auto ends_with_ignore_case(vk::string_view text, vk::string_view suffix) -> bool
{
    if (suffix.size() > text.size()) {
        return false;
    }

    const vk_usize start = text.size() - suffix.size();
    for (vk_usize index = 0; index < suffix.size(); ++index) {
        if (ascii_lower(text[start + index]) != ascii_lower(suffix[index])) {
            return false;
        }
    }
    return true;
}

inline auto compare_casefolded(const char* lhs, const char* rhs) -> int
{
    if (lhs == nullptr && rhs == nullptr) {
        return 0;
    }
    if (lhs == nullptr) {
        return -1;
    }
    if (rhs == nullptr) {
        return 1;
    }

    while (*lhs != '\0' || *rhs != '\0') {
        const char lhs_ch = ascii_lower(*lhs);
        const char rhs_ch = ascii_lower(*rhs);
        if (lhs_ch != rhs_ch) {
            return static_cast<unsigned char>(lhs_ch) < static_cast<unsigned char>(rhs_ch) ? -1 : 1;
        }
        if (*lhs != '\0') {
            ++lhs;
        }
        if (*rhs != '\0') {
            ++rhs;
        }
    }

    return 0;
}

inline auto ends_with_casefolded(const char* text, const char* suffix) -> bool
{
    if (text == nullptr || suffix == nullptr) {
        return false;
    }

    const vk_usize text_length = ascii_length(text);
    const vk_usize suffix_length = ascii_length(suffix);
    if (suffix_length == 0 || suffix_length > text_length) {
        return false;
    }

    return compare_casefolded(text + (text_length - suffix_length), suffix) == 0;
}

inline auto path_basename(const char* path) -> const char*
{
    if (path == nullptr || path[0] == '\0') {
        return "";
    }

    const char* basename = path;
    for (const char* cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\') {
            basename = cursor + 1;
        }
    }
    return basename;
}

inline auto is_vbin_program_path(const char* path) -> bool
{
    const char* basename = path_basename(path);
    return basename[0] != '\0' && ends_with_casefolded(basename, ".vbin");
}

inline auto is_vbin_program_path(vk::string_view path) -> bool
{
    const vk::string_view basename = path_basename(path);
    return !basename.empty() && ends_with_ignore_case(basename, ".vbin");
}

inline auto is_vbin_program_path(const std::string& path) -> bool
{
    return is_vbin_program_path(path.c_str());
}

inline auto trim_ascii(vk::string_view text) -> std::string
{
    vk_usize start = 0;
    vk_usize end = text.size();

    while (start < end && is_ascii_space(text[start])) {
        ++start;
    }
    while (end > start && is_ascii_space(text[end - 1])) {
        --end;
    }

    return string_from_view(subview(text, start, end - start));
}

inline auto format_byte_size(vk_u64 size) -> std::string
{
    if (size < 1024ULL) {
        return string_from_i64(static_cast<long long>(size)) + " B";
    }

    static constexpr const char* units[] = {
        "KB",
        "MB",
        "GB",
        "TB",
    };

    vk_u64 whole = size;
    vk_u64 remainder = 0;
    size_t divisions = 0;
    while (whole >= 1024ULL && divisions < std::size(units)) {
        remainder = whole % 1024ULL;
        whole /= 1024ULL;
        ++divisions;
    }
    if (divisions == 0) {
        return string_from_i64(static_cast<long long>(size)) + " B";
    }

    std::array<char, 32> buffer {};
    const unsigned decimal = static_cast<unsigned>((remainder * 10ULL) / 1024ULL);
    if (whole >= 100ULL || decimal == 0U) {
        snprintf(buffer.data(), buffer.size(), "%llu %s",
                 static_cast<unsigned long long>(whole),
                 units[divisions - 1]);
    } else {
        snprintf(buffer.data(), buffer.size(), "%llu.%u %s",
                 static_cast<unsigned long long>(whole),
                 decimal,
                 units[divisions - 1]);
    }
    return string_from_buffer(buffer);
}

inline auto truncate_with_ellipsis(vk::string_view text, vk_usize max_chars) -> std::string
{
    if (text.size() <= max_chars) {
        return string_from_view(text);
    }
    if (max_chars <= 3) {
        return std::string(max_chars, '.');
    }

    std::string shortened = string_from_view(subview(text, 0, max_chars - 3));
    shortened += "...";
    return shortened;
}

inline auto read_file_bytes(vk::string_view path, std::string& bytes) -> bool
{
    bytes.clear();

    const std::string path_string = string_from_view(path);
    const vk_usize expected_size = VK_CALL(file_size, path_string.c_str());
    const vk_file_handle_t handle = VK_CALL(file_open, path_string.c_str(), "r");
    if (handle == static_cast<vk_file_handle_t>(0)) {
        return false;
    }

    if (expected_size != 0) {
        bytes.resize(expected_size);
        vk_usize total = 0;
        while (total < expected_size) {
            const vk_usize count = VK_CALL(file_read_handle,
                                           handle,
                                           bytes.data() + total,
                                           expected_size - total);
            if (count == 0) {
                break;
            }
            total += count;
        }
        bytes.resize(total);
    } else {
        std::array<char, 512> chunk {};
        for (;;) {
            const vk_usize count = VK_CALL(file_read_handle, handle, chunk.data(), chunk.size());
            if (count == 0) {
                break;
            }
            bytes.append(chunk.data(), count);
        }
    }

    const int close_result = VK_CALL(file_close, handle);
    return close_result == 0;
}

inline auto write_file_bytes(vk::string_view path, vk::string_view bytes) -> bool
{
    const std::string path_string = string_from_view(path);
    const vk_file_handle_t handle = VK_CALL(file_open, path_string.c_str(), "w");
    if (handle == static_cast<vk_file_handle_t>(0)) {
        return false;
    }

    vk_usize total = 0;
    while (total < bytes.size()) {
        const vk_usize count = VK_CALL(file_write_handle,
                                       handle,
                                       bytes.data() + total,
                                       bytes.size() - total);
        if (count == 0) {
            break;
        }
        total += count;
    }

    const int close_result = VK_CALL(file_close, handle);
    return total == bytes.size() && close_result == 0;
}

inline auto find_substring(vk::string_view text, vk::string_view needle) -> vk_usize
{
    if (needle.empty()) {
        return 0;
    }
    if (needle.size() > text.size()) {
        return k_not_found;
    }

    for (vk_usize index = 0; index + needle.size() <= text.size(); ++index) {
        bool matches = true;
        for (vk_usize inner = 0; inner < needle.size(); ++inner) {
            if (text[index + inner] != needle[inner]) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return index;
        }
    }

    return k_not_found;
}

inline auto parse_i64(vk::string_view text) -> long long
{
    vk_usize index = 0;
    while (index < text.size() && is_ascii_space(text[index])) {
        ++index;
    }

    long long sign = 1;
    if (index < text.size() && text[index] == '-') {
        sign = -1;
        ++index;
    }

    long long value = 0;
    while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
        value = (value * 10) + static_cast<long long>(text[index] - '0');
        ++index;
    }

    return sign * value;
}

inline auto parse_u64(vk::string_view text) -> vk_u64
{
    vk_usize index = 0;
    while (index < text.size() && is_ascii_space(text[index])) {
        ++index;
    }

    vk_u64 value = 0;
    while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
        value = (value * 10ULL) + static_cast<vk_u64>(text[index] - '0');
        ++index;
    }

    return value;
}

struct DirectoryListEntry {
    std::string name;
    bool is_directory = false;
    vk_u64 size = 0;
};

inline auto parse_directory_list_item(const char* record, DirectoryListEntry& out) -> bool
{
    if (record == nullptr || (record[0] != 'D' && record[0] != 'F') || record[1] != '\t') {
        return false;
    }

    const char* second_tab = record + 2;
    while (*second_tab != '\0' && *second_tab != '\t') {
        ++second_tab;
    }
    if (*second_tab != '\t' || second_tab <= record + 2) {
        return false;
    }

    out = DirectoryListEntry {};
    out.name.assign(record + 2, static_cast<size_t>(second_tab - (record + 2)));
    out.is_directory = record[0] == 'D';
    out.size = parse_u64(vk::string_view(second_tab + 1));
    return !out.name.empty();
}

inline auto parse_directory_list_item(vk::string_view record, DirectoryListEntry& out) -> bool
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
    out.size = parse_u64(subview(record, second_tab + 1, record.size() - second_tab - 1));
    return !out.name.empty();
}

template <size_t N>
inline auto json_extract_string(const char* json, const char* key, std::array<char, N>& out) -> bool
{
    if constexpr (N == 0) {
        return false;
    }

    return vk_json_extract_string_field(json, key, out.data(), static_cast<vk_usize>(out.size())) != 0;
}

struct ImGuiStringInputUserData {
    std::string* string = nullptr;
    ImGuiInputTextCallback chained_callback = nullptr;
    void* chained_user_data = nullptr;
};

inline auto imgui_string_input_callback(ImGuiInputTextCallbackData* data) -> int
{
    auto* user_data = static_cast<ImGuiStringInputUserData*>(data->UserData);
    if (user_data == nullptr || user_data->string == nullptr) {
        return 0;
    }

    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        std::string& value = *user_data->string;
        const size_t requested_capacity = data->BufSize > 0 ? static_cast<size_t>(data->BufSize - 1) : 0;
        if (requested_capacity > value.capacity()) {
            value.reserve(requested_capacity);
        }
        value.resize(static_cast<size_t>(data->BufTextLen));
        data->Buf = value.data();
        return 0;
    }

    if (user_data->chained_callback != nullptr) {
        data->UserData = user_data->chained_user_data;
        return user_data->chained_callback(data);
    }

    return 0;
}

inline auto imgui_input_text(const char* label,
                             std::string& value,
                             ImGuiInputTextFlags flags = 0,
                             ImGuiInputTextCallback callback = nullptr,
                             void* user_data = nullptr) -> bool
{
    ImGuiStringInputUserData callback_data = { &value, callback, user_data };
    return ImGui::InputText(label,
                            value.data(),
                            value.capacity() + 1,
                            flags | ImGuiInputTextFlags_CallbackResize,
                            imgui_string_input_callback,
                            &callback_data);
}

inline auto imgui_input_text_multiline(const char* label,
                                       std::string& value,
                                       const ImVec2& size = ImVec2(0.0f, 0.0f),
                                       ImGuiInputTextFlags flags = 0,
                                       ImGuiInputTextCallback callback = nullptr,
                                       void* user_data = nullptr) -> bool
{
    ImGuiStringInputUserData callback_data = { &value, callback, user_data };
    return ImGui::InputTextMultiline(label,
                                     value.data(),
                                     value.capacity() + 1,
                                     size,
                                     flags | ImGuiInputTextFlags_CallbackResize,
                                     imgui_string_input_callback,
                                     &callback_data);
}

inline auto imgui_title_should_use_light_text() -> bool
{
    const ImVec4& title_bg = ImGui::GetStyle().Colors[ImGuiCol_TitleBgActive];
    const float luminance = (title_bg.x * 0.2126f) + (title_bg.y * 0.7152f) + (title_bg.z * 0.0722f);
    return luminance < 0.45f;
}

inline auto imgui_begin_window_readable_caption(const char* name,
                                                bool* p_open = nullptr,
                                                ImGuiWindowFlags flags = 0) -> bool
{
    const bool use_light_title = imgui_title_should_use_light_text();
    if (use_light_title) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    const bool open = ImGui::Begin(name, p_open, flags);

    if (use_light_title) {
        ImGui::PopStyleColor();
    }

    return open;
}

} // namespace vkgui

#endif // VKGUI_COMMON_H
