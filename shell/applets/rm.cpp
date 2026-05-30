/*
 * vkernel userspace - rm
 * Copyright (C) 2026 vkernel authors
 */

#include "file_utils.h"

#include <iostream>
#include <string>

namespace applet::rm {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "rm",
    "Remove files",
    run,
};

static void remove_one(const std::string& raw)
{
    std::string path;
    if (!detail::resolve_path_argument("rm", raw, path)) {
        return;
    }

    if (path == "/") {
        std::cout << "rm: refusing to remove /\n";
        return;
    }

    if (shell::directory_exists(path)) {
        std::cout << "rm: is a directory: " << raw << '\n';
        return;
    }

    if (!VK_CALL(file_exists, path.c_str())) {
        std::cout << "rm: not found: " << raw << '\n';
        return;
    }

    if (VK_CALL(file_remove, path.c_str()) != 0) {
        std::cout << "rm: failed to remove: " << raw << '\n';
    }
}

static void run(const std::string& arg)
{
    std::string remaining = shell::trim_left(arg);
    if (remaining.empty()) {
        std::cout << "Usage: rm <file> [file...]\n";
        return;
    }

    while (true) {
        const shell::argument_token token = shell::next_argument(remaining);
        if (!token.valid) {
            break;
        }

        remove_one(token.text);
        remaining = token.rest;
    }
}

} // namespace applet::rm