/*
 * vkernel userspace - wc
 * Copyright (C) 2026 vkernel authors
 */

#include "file_utils.h"

#include <array>
#include <iostream>
#include <span>
#include <string>

namespace applet::wc {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "wc",
    "Count file lines, words, and bytes",
    run,
};

static auto is_space(char ch) -> bool
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

static void run(const std::string& arg)
{
    const shell::argument_token path_token = shell::next_argument(shell::trim_left(arg));
    if (!path_token.valid || shell::next_argument(path_token.rest).valid) {
        std::cout << "Usage: wc <file>\n";
        return;
    }

    std::string path;
    vk_file_handle_t handle = 0;
    if (!detail::open_input_file("wc", path_token.text, path, handle)) {
        return;
    }

    std::array<char, detail::kIoBufferSize> buffer{};
    vk_usize bytes = 0;
    vk_usize lines = 0;
    vk_usize words = 0;
    bool in_word = false;
    vk_usize read_size = 0;

    while ((read_size = VK_CALL(file_read_handle, handle, buffer.data(), buffer.size())) > 0) {
        bytes += read_size;
        for (char ch : std::span(buffer).first(read_size)) {
            if (ch == '\n') {
                ++lines;
            }

            if (is_space(ch)) {
                in_word = false;
                continue;
            }

            if (!in_word) {
                ++words;
                in_word = true;
            }
        }
    }

    VK_CALL(file_close, handle);

    std::cout << lines << ' ' << words << ' ' << bytes << ' ' << path << '\n';
}

} // namespace applet::wc