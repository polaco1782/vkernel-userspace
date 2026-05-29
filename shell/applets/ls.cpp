/*
 * vkernel userspace - ls
 * Copyright (C) 2026 vkernel authors
 */

#include "applet.h"

#include <iostream>
#include <string>

namespace applet::ls {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "ls",
    "List files and directories",
    run,
};

struct fs_item {
    std::string name;
    vk_u64 size = 0;
    bool is_directory = false;
    bool valid = false;
};

constexpr vk_usize kFsItemsMax = 64;
constexpr vk_usize kFsResponseMax = 4096;
constexpr vk_usize kFsItemMax = 96;

template <vk_usize N>
using char_buffer = std::array<char, N>;

/* Prints one shell ls row for either a file or directory. */
void print_ls_entry(const std::string& name, bool is_directory, vk_u64 size)
{
    std::cout << (is_directory ? "d " : "f ");

    if (is_directory) {
        std::cout << name;
        std::cout << "/\n";
        return;
    }

    std::cout << name << std::string(28 - name.size(), ' ') << "  " << size << '\n';
}

/* Parses one fs_list entry in the compact shell-specific record format. */
auto parse_fs_item(const char* record) -> fs_item
{
    fs_item item{};
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

/* Lists a directory and renders the entries using the shell table format. */
auto list_directory(const std::string& path) -> bool
{
    char_buffer<kFsResponseMax> response{};
    std::array<char_buffer<kFsItemMax>, kFsItemsMax> raw_items{};
    int printed = 0;

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
    for (const auto& raw_item : items) {
        const fs_item item = parse_fs_item(raw_item.data());
        if (!item.valid) {
            continue;
        }

        print_ls_entry(item.name, item.is_directory, item.size);
        printed = 1;
    }

    if (printed == 0) {
        std::cout << "(empty)\n";
    }

    return true;
}

static void run(const std::string& arg)
{
    const std::string raw = shell::trim_left(arg);
    std::string path;

    // check if tree listing was requested (not supported)
    if(!raw.empty() && raw == "-t") {
        std::cout << "ls: option '-t' not supported\n";
        return;
     }

    if (!shell::resolve_path(raw, path)) {
        std::cout << "ls: path too long\n";
        return;
    }

    if (list_directory(path)) {
        return;
    }

    if (VK_CALL(file_exists, path.c_str())) {
        print_ls_entry(shell::basename(path), false, VK_CALL(file_size, path.c_str()));
        return;
    }

    std::cout << "ls: not found: ";
    std::cout << (raw.empty() ? path : raw);
    std::cout << '\n';
}

}  // namespace applet::ls
