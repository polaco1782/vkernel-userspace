#pragma once

#include "applet.h"

#include <array>
#include <iostream>
#include <span>
#include <string>

namespace applet::detail {

constexpr vk_usize kIoBufferSize = 512;
constexpr vk_usize kPathMax = 256;
constexpr vk_usize kFsItemsMax = 64;
constexpr vk_usize kFsResponseMax = 4096;
constexpr vk_usize kFsItemMax = 96;

template <vk_usize N>
using io_buffer = std::array<unsigned char, N>;

template <vk_usize N>
using char_buffer = std::array<char, N>;

struct directory_item {
    std::string name;
    vk_u64 size = 0;
    bool is_directory = false;
    bool valid = false;
};

inline auto parse_directory_item(const char* record) -> directory_item
{
    directory_item item{};
    if (record == nullptr || record[0] == '\0' || record[1] != '\t') {
        return item;
    }

    item.is_directory = record[0] == 'D';

    vk_usize cursor = 2;
    while (record[cursor] != '\0' && record[cursor] != '\t') {
        item.name.push_back(record[cursor++]);
    }

    if (record[cursor] != '\t' || item.name.empty()) {
        item.name.clear();
        return item;
    }

    ++cursor;
    while (record[cursor] >= '0' && record[cursor] <= '9') {
        item.size = item.size * 10ULL + static_cast<vk_u64>(record[cursor] - '0');
        ++cursor;
    }

    item.valid = true;
    return item;
}

template <typename Fn>
inline auto for_each_directory_item(const std::string& path, Fn&& fn) -> bool
{
    char_buffer<kFsResponseMax> response{};
    std::array<char_buffer<kFsItemMax>, kFsItemsMax> raw_items{};

    vk_kobj_rpc_path_json("fs_list", path.c_str(), response.data(), response.size());
    if (!vk_kobj_response_ok(response.data())) {
        return false;
    }

    const int item_count = vk_json_extract_string_array_field(response.data(),
                                                              "items",
                                                              raw_items[0].data(),
                                                              kFsItemMax,
                                                              kFsItemsMax);

    const auto items = std::span(raw_items).first(static_cast<vk_usize>(item_count));
    for (vk_usize index = 0; index < items.size(); ++index) {
        const directory_item item = parse_directory_item(items[index].data());
        if (!item.valid) {
            continue;
        }

        if (!fn(item, index + 1 == items.size())) {
            return false;
        }
    }

    return true;
}

inline auto resolve_path_argument(const char* command,
                                  const std::string& raw,
                                  std::string& out_path) -> bool
{
    if (!shell::resolve_path(raw, out_path)) {
        std::cout << command << ": path too long\n";
        return false;
    }

    return true;
}

inline auto open_input_file(const char* command,
                            const std::string& raw,
                            std::string& out_path,
                            vk_file_handle_t& out_handle) -> bool
{
    out_handle = 0;
    if (!resolve_path_argument(command, raw, out_path)) {
        return false;
    }

    if (shell::directory_exists(out_path)) {
        std::cout << command << ": is a directory: " << raw << '\n';
        return false;
    }

    out_handle = VK_CALL(file_open, out_path.c_str(), "r");
    if (out_handle == static_cast<vk_file_handle_t>(0)) {
        std::cout << command << ": file not found: " << raw << '\n';
        return false;
    }

    return true;
}

inline auto resolve_output_path(const char* command,
                                const std::string& source_path,
                                const std::string& raw_destination,
                                std::string& out_destination) -> bool
{
    if (!resolve_path_argument(command, raw_destination, out_destination)) {
        return false;
    }

    if (!shell::directory_exists(out_destination)) {
        return true;
    }

    const std::string base = shell::basename(source_path);
    if (base.empty()) {
        std::cout << command << ": invalid source path\n";
        return false;
    }

    const vk_usize needed = out_destination.size() + base.size() + 2;
    if (needed > kPathMax) {
        std::cout << command << ": path too long\n";
        return false;
    }

    out_destination = out_destination == "/" ? "/" + base : out_destination + "/" + base;
    return true;
}

inline auto copy_file_contents(const char* command,
                               const std::string& source_path,
                               const std::string& destination_path) -> bool
{
    const vk_file_handle_t source = VK_CALL(file_open, source_path.c_str(), "r");
    if (source == static_cast<vk_file_handle_t>(0)) {
        std::cout << command << ": file not found: " << source_path << '\n';
        return false;
    }

    const vk_file_handle_t destination = VK_CALL(file_open, destination_path.c_str(), "w");
    if (destination == static_cast<vk_file_handle_t>(0)) {
        VK_CALL(file_close, source);
        std::cout << command << ": failed to open destination: " << destination_path << '\n';
        return false;
    }

    io_buffer<kIoBufferSize> buffer{};
    bool ok = true;
    vk_usize read_size = 0;

    while ((read_size = VK_CALL(file_read_handle, source, buffer.data(), buffer.size())) > 0) {
        const vk_usize written = VK_CALL(file_write_handle, destination, buffer.data(), read_size);
        if (written != read_size) {
            ok = false;
            break;
        }
    }

    VK_CALL(file_close, source);
    VK_CALL(file_close, destination);

    if (ok) {
        return true;
    }

    VK_CALL(file_remove, destination_path.c_str());
    std::cout << command << ": write failed: " << destination_path << '\n';
    return false;
}

} // namespace applet::detail