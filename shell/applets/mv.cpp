/*
 * vkernel userspace - mv
 * Copyright (C) 2026 vkernel authors
 */

#include "file_utils.h"

#include <iostream>
#include <string>

namespace applet::mv {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "mv",
    "Move or rename a file",
    run,
};

static auto parse_arguments(const std::string& arg,
                            std::string& source_raw,
                            std::string& destination_raw) -> bool
{
    const shell::argument_token source = shell::next_argument(shell::trim_left(arg));
    if (!source.valid) {
        std::cout << "Usage: mv <source> <destination>\n";
        return false;
    }

    const shell::argument_token destination = shell::next_argument(source.rest);
    if (!destination.valid || shell::next_argument(destination.rest).valid) {
        std::cout << "Usage: mv <source> <destination>\n";
        return false;
    }

    source_raw = source.text;
    destination_raw = destination.text;
    return true;
}

static void run(const std::string& arg)
{
    std::string source_raw;
    std::string destination_raw;
    if (!parse_arguments(arg, source_raw, destination_raw)) {
        return;
    }

    std::string source_path;
    if (!detail::resolve_path_argument("mv", source_raw, source_path)) {
        return;
    }

    if (shell::directory_exists(source_path)) {
        std::cout << "mv: omitting directory: " << source_raw << '\n';
        return;
    }

    if (!VK_CALL(file_exists, source_path.c_str())) {
        std::cout << "mv: file not found: " << source_raw << '\n';
        return;
    }

    std::string destination_path;
    if (!detail::resolve_output_path("mv", source_path, destination_raw, destination_path)) {
        return;
    }

    if (source_path == destination_path) {
        return;
    }

    if (!detail::copy_file_contents("mv", source_path, destination_path)) {
        return;
    }

    if (VK_CALL(file_remove, source_path.c_str()) != 0) {
        std::cout << "mv: failed to remove source: " << source_raw << '\n';
    }
}

} // namespace applet::mv