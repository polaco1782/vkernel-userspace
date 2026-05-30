/*
 * vkernel userspace - tree
 * Copyright (C) 2026 vkernel authors
 */

#include "file_utils.h"

#include <string>

namespace applet::tree {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "tree",
    "Display the directory tree",
    run,
};

auto list_directory_tree(const std::string& path, const std::string& prefix, bool is_last) -> bool
{
    return detail::for_each_directory_item(path, [&](const detail::directory_item& item, bool item_is_last) {
        std::cout << prefix << (is_last ? "└─ " : "├─ ") << item.name
                  << (item.is_directory ? "/" : "") << '\n';
        if (!item.is_directory) {
            return true;
        }

        const std::string child_path = path == "/" ? "/" + item.name : path + "/" + item.name;
        const std::string child_prefix = prefix + (is_last ? "   " : "│  ");
        return list_directory_tree(child_path, child_prefix, item_is_last);
    });
}

static void run(const std::string& arg)
{
    (void)arg;
    list_directory_tree(shell::current_working_directory(), "", true);
}

}  // namespace applet::tree
