/*
 * vkernel userspace - tail
 * Copyright (C) 2026 vkernel authors
 */

#include "file_utils.h"

#include <array>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace applet::tail {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "tail",
    "Print the last lines of a file",
    run,
};

static auto parse_count(const std::string& text, vk_usize& out_count) -> bool
{
    if (text.empty()) {
        return false;
    }

    vk_usize value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        value = value * 10 + static_cast<vk_usize>(ch - '0');
    }

    out_count = value;
    return true;
}

static auto parse_arguments(const std::string& arg, vk_usize& line_count, std::string& path_raw) -> bool
{
    std::string remaining = shell::trim_left(arg);
    line_count = 10;

    const shell::argument_token first = shell::next_argument(remaining);
    if (!first.valid) {
        std::cout << "Usage: tail [-n count] <file>\n";
        return false;
    }

    if (first.text == "-n") {
        const shell::argument_token count = shell::next_argument(first.rest);
        const shell::argument_token path = shell::next_argument(count.rest);
        if (!count.valid || !path.valid || shell::next_argument(path.rest).valid
            || !parse_count(count.text, line_count)) {
            std::cout << "Usage: tail [-n count] <file>\n";
            return false;
        }

        path_raw = path.text;
        return true;
    }

    if (shell::next_argument(first.rest).valid) {
        std::cout << "Usage: tail [-n count] <file>\n";
        return false;
    }

    path_raw = first.text;
    return true;
}

static void run(const std::string& arg)
{
    vk_usize line_count = 10;
    std::string path_raw;
    if (!parse_arguments(arg, line_count, path_raw)) {
        return;
    }

    std::string path;
    vk_file_handle_t handle = 0;
    if (!detail::open_input_file("tail", path_raw, path, handle)) {
        return;
    }

    std::array<char, detail::kIoBufferSize> buffer{};
    std::string line;
    std::vector<std::string> ring(line_count == 0 ? 1 : line_count);
    vk_usize total_lines = 0;
    vk_usize read_size = 0;

    auto remember_line = [&](const std::string& text) {
        if (line_count == 0) {
            ++total_lines;
            return;
        }

        ring[total_lines % line_count] = text;
        ++total_lines;
    };

    while ((read_size = VK_CALL(file_read_handle, handle, buffer.data(), buffer.size())) > 0) {
        for (char ch : std::span(buffer).first(read_size)) {
            if (ch == '\r') {
                continue;
            }

            if (ch == '\n') {
                remember_line(line);
                line.clear();
                continue;
            }

            line.push_back(ch);
        }
    }

    if (!line.empty()) {
        remember_line(line);
    }

    VK_CALL(file_close, handle);

    if (line_count == 0 || total_lines == 0) {
        return;
    }

    const vk_usize emit_count = total_lines < line_count ? total_lines : line_count;
    const vk_usize start = total_lines > line_count ? total_lines % line_count : 0;
    for (vk_usize index = 0; index < emit_count; ++index) {
        std::cout << ring[(start + index) % line_count] << '\n';
    }
}

} // namespace applet::tail