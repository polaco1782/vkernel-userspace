/*
 * vkernel userspace - cp
 * Copyright (C) 2026 vkernel authors
 */

#include "file_utils.h"

#include <iostream>
#include <string>

namespace applet::cp {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "cp",
    "Copy a file",
    run,
};

static auto parse_arguments(const std::string& arg,
                            std::string& source_raw,
                            std::string& destination_raw) -> bool
{
    const shell::argument_token source = shell::next_argument(shell::trim_left(arg));
    if (!source.valid) {
        std::cout << "Usage: cp <source> <destination>\n";
        return false;
    }

    const shell::argument_token destination = shell::next_argument(source.rest);
    if (!destination.valid || shell::next_argument(destination.rest).valid) {
        std::cout << "Usage: cp <source> <destination>\n";
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
    if (!detail::resolve_path_argument("cp", source_raw, source_path)) {
        return;
    }

    if (shell::directory_exists(source_path)) {
        std::cout << "cp: omitting directory: " << source_raw << '\n';
        return;
    }

    if (!VK_CALL(file_exists, source_path.c_str())) {
        std::cout << "cp: file not found: " << source_raw << '\n';
        return;
    }

    std::string destination_path;
    if (!detail::resolve_output_path("cp", source_path, destination_raw, destination_path)) {
        return;
    }

    if (source_path == destination_path) {
        std::cout << "cp: source and destination are the same file\n";
        return;
    }

    (void)detail::copy_file_contents("cp", source_path, destination_path);
}

} // namespace applet::cp