/*
 * vkernel userspace - ls
 * Copyright (C) 2026 vkernel authors
 */

#include "file_utils.h"

#include <iostream>
#include <string>

namespace applet::ls {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "ls",
    "List files and directories",
    run,
};

/* Prints one shell ls row for either a file or directory. */
void print_ls_entry(const std::string& name, bool is_directory, vk_u64 size)
{
    std::cout << (is_directory ? "d " : "f ");

    if (is_directory) {
        std::cout << name;
        std::cout << "/\n";
        return;
    }

    std::cout << name;
    if (name.size() < 28) {
        std::cout << std::string(28 - name.size(), ' ');
    } else {
        std::cout << ' ';
    }

    std::cout << size << '\n';
}

/* Lists a directory and renders the entries using the shell table format. */
auto list_directory(const std::string& path) -> bool
{
    int printed = 0;

    if (!detail::for_each_directory_item(path, [&](const detail::directory_item& item, bool) {
            print_ls_entry(item.name, item.is_directory, item.size);
            printed = 1;
            return true;
        })) {
        return false;
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
