/*
 * vkernel userspace - shell
 * Copyright (C) 2026 vkernel authors
 *
 * shell.cpp - Freestanding userspace shell for vkernel
 *
 * Build: see Makefile (Linux) or shell.vcxproj (Visual Studio).
 * Run:   launched automatically by the kernel as shell.elf / shell.exe
 */

#include "../include/vk.h"
#include "applets/applet.h"

#include <array>
#include <iostream>
#include <ranges>
#include <span>
#include <string>

#if defined(_MSC_VER)
#pragma warning(disable: 4702) /* unreachable code (infinite shell loop) */
#endif

namespace {

using size_type = vk_usize;

constexpr const char* kPrompt = "vk> ";
constexpr size_type kHistoryMax = 8;
constexpr size_type kLineMax = 256;

template <size_type N>
using char_buffer = std::array<char, N>;

struct parsed_token {
    std::string text;
    std::string rest;
    bool valid = false;
};

static std::array<std::string, kHistoryMax> s_history{};
static size_type s_history_count = 0;

template <typename Fn>
static void shell_for_each_command(Fn&& fn)
{
    const shell::command_list_view commands = shell::applet_commands();
    for (const shell::command_spec* command : std::span(commands.data, commands.count)) {
        if (command != nullptr) {
            fn(*command);
        }
    }
}

/* Returns true when a command name begins with the supplied prefix. */
static auto starts_with(const std::string& text, const std::string& prefix) -> bool
{
    if (prefix.size() > text.size()) {
        return false;
    }

    for (size_type index = 0; index < prefix.size(); ++index) {
        if (text[index] != prefix[index]) {
            return false;
        }
    }

    return true;
}

/* Returns true when the input already contains argument separators. */
static auto contains_token_delimiter(const std::string& text) -> bool
{
    for (char ch : text) {
        if (ch == ' ' || ch == '\t') {
            return true;
        }
    }
    return false;
}

/* Splits the first token from a shell command line, honoring quotes. */
static auto read_next_token(const std::string& input) -> parsed_token
{
    parsed_token token{};
    const size_type first = input.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return token;
    }

    size_type cursor = first;
    char quote = '\0';
    if (input[cursor] == '"' || input[cursor] == '\'') {
        quote = input[cursor++];
    }

    const size_type start = cursor;
    while (cursor < input.size()) {
        const char ch = input[cursor];
        if (quote != '\0') {
            if (ch == quote) {
                break;
            }
        } else if (ch == ' ' || ch == '\t') {
            break;
        }
        ++cursor;
    }

    token.text = input.substr(start, cursor - start);
    if (token.text.empty()) {
        return token;
    }

    if (quote != '\0' && cursor < input.size() && input[cursor] == quote) {
        ++cursor;
    }

    const size_type rest_start = input.find_first_not_of(" \t", cursor);
    token.rest = rest_start == std::string::npos ? std::string() : input.substr(rest_start);
    token.valid = true;
    return token;
}

/* Detects whether the shell is running against a graphical console. */
static auto shell_has_framebuffer() -> bool
{
    vk_framebuffer_info_t fb{};
    VK_CALL(framebuffer_info, &fb);
    return fb.valid != 0;
}

/* Checks whether the process command line includes a specific startup flag. */
static auto shell_cmdline_has_flag(const char* flag) -> bool
{
    if (!vk_get_api()->vk_get_cmdline) {
        return false;
    }

    char_buffer<kLineMax> cmdline{};
    vk_get_api()->vk_get_cmdline(cmdline.data(), cmdline.size());

    std::string remaining(cmdline.data());
    while (true) {
        const parsed_token token = read_next_token(remaining);
        if (!token.valid) {
            return false;
        }
        if (token.text == flag) {
            return true;
        }
        remaining = token.rest;
    }
}

/* Adds a command to history while suppressing empty and duplicate entries. */
static void shell_history_add(const std::string& line)
{
    if (std::ranges::empty(line)) {
        return;
    }

    if (s_history_count > 0 && s_history[s_history_count - 1] == line) {
        return;
    }

    if (s_history_count < s_history.size()) {
        s_history[s_history_count++] = line;
        return;
    }

    for (size_type index = 1; index < s_history.size(); ++index) {
        s_history[index - 1] = s_history[index];
    }
    s_history[s_history.size() - 1] = line;
}

/* Repaints the editable command line after history navigation. */
static void shell_redraw_line(const char* prompt, const std::string& line, size_type old_len)
{
    std::cout << '\r';
    std::cout << prompt;
    for (size_type index = 0; index < old_len; ++index) {
        std::cout << ' ';
    }
    std::cout << '\r';
    std::cout << prompt;
    std::cout << line;
}

/* Completes a command name or prints matching command candidates. */
static void shell_complete_line(std::string& line, size_type max, const char* prompt)
{
    if (contains_token_delimiter(line)) {
        return;
    }

    size_type match_count = 0;
    const char* match = nullptr;

    shell_for_each_command([&](const shell::command_spec& command) {
        if (command.name != nullptr && starts_with(std::string(command.name), line)) {
            match = command.name;
            ++match_count;
        }
    });

    if (match_count == 1 && match != nullptr) {
        size_type index = line.size();
        while (match[index] != '\0' && line.size() + 1 < max) {
            line.push_back(match[index]);
            std::cout << match[index];
            ++index;
        }
        if (line.size() + 1 < max) {
            line.push_back(' ');
            std::cout << ' ';
        }
        return;
    }

    if (match_count > 1) {
        std::cout << '\n';
        shell_for_each_command([&](const shell::command_spec& command) {
            if (command.name != nullptr && starts_with(std::string(command.name), line)) {
                std::cout << command.name;
                std::cout << "  ";
            }
        });
        std::cout << '\n';
        std::cout << prompt;
        std::cout << line;
    }
}

/* Reads one interactive shell line with tab completion and history navigation. */
static auto console_getline(std::string& line, size_type max, const char* prompt) -> size_type
{
    line.clear();
    size_type history_index = s_history_count;
    size_type old_len = 0;

    while (line.size() + 1 < max) {
        const char ch = VK_CALL(getc);

        if (ch == '\r' || ch == '\n') {
            std::cout << '\n';
            break;
        }

        if (ch == '\t') {
            shell_complete_line(line, max, prompt);
            old_len = line.size();
            continue;
        }

        if (ch == 27) {
            const char c1 = VK_CALL(try_getc);
            const char c2 = VK_CALL(try_getc);
            if (c1 == '[' && (c2 == 'A' || c2 == 'B')) {
                if (c2 == 'A' && history_index > 0) {
                    --history_index;
                    line = s_history[history_index];
                    shell_redraw_line(prompt, line, old_len);
                    old_len = line.size();
                } else if (c2 == 'B') {
                    if (history_index + 1 < s_history_count) {
                        ++history_index;
                        line = s_history[history_index];
                    } else {
                        history_index = s_history_count;
                        line.clear();
                    }
                    shell_redraw_line(prompt, line, old_len);
                    old_len = line.size();
                }
            }
            continue;
        }

        if ((ch == 0x7F || ch == '\b') && !line.empty()) {
            line.pop_back();
            std::cout << "\b \b";
            old_len = line.size();
            continue;
        }

        if (ch >= ' ' && ch < 0x7F) {
            line.push_back(ch);
            std::cout << ch;
            old_len = line.size();
        }
    }

    shell_history_add(line);
    return line.size();
}

/* Dispatches one shell command against the applet registry. */
static auto parse_cmdline(const std::string& cmdline) -> int
{
    const parsed_token command = read_next_token(cmdline);
    if (!command.valid) {
        return 0;
    }

    if (command.text == "?") {
        for (const shell::command_spec* spec : std::span(shell::applet_commands().data,
                                                         shell::applet_commands().count)) {
            if (spec != nullptr && std::string(spec->name) == "help") {
                spec->fn(std::string());
                return 0;
            }
        }
        return 0;
    }

    int handled = 0;
    shell_for_each_command([&](const shell::command_spec& spec) {
        if (handled != 0 || spec.name == nullptr) {
            return;
        }

        if (command.text != std::string(spec.name)) {
            return;
        }

        spec.fn(command.rest);
        handled = 1;
    });

    if (handled != 0) {
        return 0;
    }

    if (shell::launch_program(cmdline, 0) == 0) {
        return 0;
    }

    std::cout << "Command not found: ";
    std::cout << cmdline;
    std::cout << "\n";
    std::cout << "Type 'help' for available commands.\n";
    return -1;
}

/* Executes one startup-script line after trimming comments and whitespace. */
static void run_script_line(const std::string& line)
{
    const std::string command = shell::trim_left(line);
    if (command.empty() || command[0] == '#') {
        return;
    }

    std::cout << kPrompt;
    std::cout << command;
    std::cout << "\n";
    parse_cmdline(command);
}

/* Streams and executes the optional shell startup script line by line. */
static void read_startup_script()
{
    const vk_file_handle_t fh = VK_CALL(file_open, "/data/shell/shell.txt", "r");
    if (fh == static_cast<vk_file_handle_t>(0)) {
        std::cout << "No startup script found (/data/shell/shell.txt), skipping...\n";
        return;
    }

    char_buffer<kLineMax> chunk{};
    std::string line;
    size_type read_size = 0;

    while ((read_size = VK_CALL(file_read_handle, fh, chunk.data(), chunk.size())) > 0) {
        for (char ch : std::span(chunk).first(read_size)) {
            if (ch == '\n') {
                run_script_line(line);
                line.clear();
                continue;
            }

            if (line.size() + 1 < kLineMax) {
                line.push_back(ch);
            }
        }
    }

    if (!std::ranges::empty(line)) {
        run_script_line(line);
    }

    VK_CALL(file_close, fh);
}

/* Prints the initial shell banner shown at process startup. */
static void print_banner()
{
    std::cout << "\n\n";
    std::cout << "+----------------------------------+\n";
    std::cout << "|     vkernel userspace shell      |\n";
    std::cout << "+----------------------------------+\n";
    std::cout << "Type 'help' for available commands.\n\n";
}

}  // namespace

extern "C" int _start(const vk_api_t* api)
{
    vk_init(api);
    shell::init_paths();

    print_banner();

    const bool has_framebuffer = shell_has_framebuffer();
    int run_startup_script = 0;

    if (vk_get_api()->vk_get_cmdline) {
        run_startup_script = shell_cmdline_has_flag("--startup") ? 1 : 0;
    } else {
        run_startup_script = has_framebuffer ? 1 : 0;
    }

    if (run_startup_script && has_framebuffer) {
        read_startup_script();
    }

    std::string line;
    for (;;) {
        std::cout << kPrompt;
        if (console_getline(line, kLineMax, kPrompt) == 0) {
            continue;
        }

        const std::string command = shell::trim_left(line);
        if (command.empty()) {
            continue;
        }

        parse_cmdline(command);
    }

    return 0;
}
