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

static void run(const std::string& arg)
{
    const std::string raw = shell::trim_left(arg);
    std::string path;

    if (!shell::resolve_path(raw, path)) {
        std::cout << "ls: path too long\n";
        return;
    }

    if (shell::list_directory(path)) {
        return;
    }

    if (VK_CALL(file_exists, path.c_str())) {
        shell::print_ls_entry(shell::basename(path), false, VK_CALL(file_size, path.c_str()));
        return;
    }

    std::cout << "ls: not found: ";
    std::cout << (raw.empty() ? path : raw);
    shell::put_char('\n');
}

}  // namespace applet::ls
