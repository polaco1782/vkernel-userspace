/*
 * vkernel userspace - cat
 * Copyright (C) 2026 vkernel authors
 */

#include "applet.h"

#include <array>
#include <iostream>
#include <span>
#include <string>

namespace applet::cat {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "cat",
    "Print a file",
    run,
};

static void run(const std::string& arg)
{
    const std::string raw = shell::trim_left(arg);
    if (raw.empty()) {
        std::cout << "Usage: cat <filename>\n";
        return;
    }

    std::string path;
    if (!shell::resolve_path(raw, path)) {
        std::cout << "cat: path too long\n";
        return;
    }

    const vk_file_handle_t fh = VK_CALL(file_open, path.c_str(), "r");
    if (fh == static_cast<vk_file_handle_t>(0)) {
        std::cout << "cat: file not found: ";
        std::cout << raw;
        std::cout << "\n";
        return;
    }

    std::array<unsigned char, 128> buffer{};
    vk_usize read_size = 0;
    while ((read_size = VK_CALL(file_read_handle, fh, buffer.data(), buffer.size())) > 0) {
        for (unsigned char byte : std::span(buffer).first(read_size)) {
            std::cout << static_cast<char>(byte);
        }
    }

    VK_CALL(file_close, fh);
    std::cout << "\n";
}

}  // namespace applet::cat
