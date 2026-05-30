/*
 * vkernel userspace - which
 * Copyright (C) 2026 vkernel authors
 */

#include "applet.h"

#include <iostream>
#include <string>

namespace applet::which {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "which",
    "Show command resolution",
    run,
};

static void show_one(const std::string& raw)
{
    if (shell::find_builtin_command(raw) != nullptr) {
        std::cout << raw << ": shell builtin\n";
        return;
    }

    std::string path;
    if (!shell::resolve_program_path(raw, path)) {
        std::cout << "which: path too long: " << raw << '\n';
        return;
    }

    if (VK_CALL(file_exists, path.c_str())) {
        std::cout << path << '\n';
        return;
    }

    if (path.size() + 5 <= 256) {
        const std::string with_suffix = path + ".vbin";
        if (VK_CALL(file_exists, with_suffix.c_str())) {
            std::cout << with_suffix << '\n';
            return;
        }
    }

    std::cout << "which: not found: " << raw << '\n';
}

static void run(const std::string& arg)
{
    std::string remaining = shell::trim_left(arg);
    if (remaining.empty()) {
        std::cout << "Usage: which <command> [command...]\n";
        return;
    }

    while (true) {
        const shell::argument_token token = shell::next_argument(remaining);
        if (!token.valid) {
            break;
        }

        show_one(token.text);
        remaining = token.rest;
    }
}

} // namespace applet::which