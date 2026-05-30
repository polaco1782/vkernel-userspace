/*
 * vkernel userspace - touch
 * Copyright (C) 2026 vkernel authors
 */

#include "file_utils.h"

#include <iostream>
#include <string>

namespace applet::touch {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "touch",
    "Create files if missing",
    run,
};

static void touch_one(const std::string& raw)
{
    std::string path;
    if (!detail::resolve_path_argument("touch", raw, path)) {
        return;
    }

    if (shell::directory_exists(path)) {
        std::cout << "touch: is a directory: " << raw << '\n';
        return;
    }

    const vk_file_handle_t handle = VK_CALL(file_open, path.c_str(), "a");
    if (handle == static_cast<vk_file_handle_t>(0)) {
        std::cout << "touch: failed to create: " << raw << '\n';
        return;
    }

    VK_CALL(file_close, handle);
}

static void run(const std::string& arg)
{
    std::string remaining = shell::trim_left(arg);
    if (remaining.empty()) {
        std::cout << "Usage: touch <file> [file...]\n";
        return;
    }

    while (true) {
        const shell::argument_token token = shell::next_argument(remaining);
        if (!token.valid) {
            break;
        }

        touch_one(token.text);
        remaining = token.rest;
    }
}

} // namespace applet::touch