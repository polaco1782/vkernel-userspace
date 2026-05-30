/*
 * vkernel userspace - shell support
 * Copyright (C) 2026 vkernel authors
 */

#include "../include/vk.h"
#include "applets/applet.h"

namespace {

constexpr vk_usize kLineMax = 256;
constexpr vk_usize kPathMax = 256;
constexpr vk_usize kPathComponentMax = 32;
constexpr vk_usize kPathSegmentMax = 64;

template <vk_usize N>
using char_buffer = std::array<char, N>;

struct parsed_token {
    std::string text;
    std::string rest;
    bool valid = false;
};

std::string s_root_path = "/";
std::string s_cwd = "/";

/* Returns true when a path ends with the requested suffix. */
auto ends_with(const std::string& text, const char* suffix) -> bool
{
    vk_usize suffix_length = 0;
    while (suffix[suffix_length] != '\0') {
        ++suffix_length;
    }

    if (suffix_length > text.size()) {
        return false;
    }

    const vk_usize start = text.size() - suffix_length;
    for (vk_usize index = 0; index < suffix_length; ++index) {
        if (text[start + index] != suffix[index]) {
            return false;
        }
    }
    return true;
}

/* Splits the first token from a shell command line, honoring quotes. */
auto read_next_token(const std::string& input) -> parsed_token
{
    parsed_token token{};
    const vk_usize first = input.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return token;
    }

    vk_usize cursor = first;
    char quote = '\0';
    if (input[cursor] == '"' || input[cursor] == '\'') {
        quote = input[cursor++];
    }

    const vk_usize start = cursor;
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

    const vk_usize rest_start = input.find_first_not_of(" \t", cursor);
    token.rest = rest_start == std::string::npos ? std::string() : input.substr(rest_start);
    token.valid = true;
    return token;
}

/* Treats both Unix and DOS separators as valid shell path delimiters. */
auto is_separator(char ch) -> bool
{
    return ch == '/' || ch == '\\';
}

/* Normalizes an absolute path by collapsing repeated separators and dot segments. */
auto normalize_absolute_path(const std::string& path, std::string& out) -> bool
{
    std::vector<std::string> components;
    vk_usize index = 0;

    while (index < path.size()) {
        while (index < path.size() && is_separator(path[index])) {
            ++index;
        }
        if (index >= path.size()) {
            break;
        }

        const vk_usize start = index;
        while (index < path.size() && !is_separator(path[index])) {
            ++index;
        }

        std::string component = path.substr(start, index - start);
        if (component == ".") {
            continue;
        }
        if (component == "..") {
            if (!components.empty()) {
                components.pop_back();
            }
            continue;
        }
        if (component.size() + 1 >= kPathSegmentMax || components.size() >= kPathComponentMax) {
            return false;
        }
        components.push_back(component);
    }

    out = "/";
    for (const auto& component : components) {
        if (out.size() > 1) {
            out += "/";
        }
        out += component;
        if (out.size() + 1 > kPathMax) {
            return false;
        }
    }

    return true;
}

/* Resolves a shell path against the supplied base directory. */
auto resolve_path_from(const std::string& base, const std::string& raw, std::string& out) -> bool
{
    const std::string input = shell::trim_left(raw);
    if (input.empty()) {
        out = base;
        return out.size() + 1 <= kPathMax;
    }

    if (is_separator(input[0])) {
        return normalize_absolute_path(input, out);
    }

    std::string combined = base == "/" ? "/" : base + "/";
    combined += input;
    if (combined.size() + 1 > kPathMax) {
        return false;
    }

    return normalize_absolute_path(combined, out);
}

/* Queries the kernel for the shell's default filesystem root. */
auto query_default_path() -> std::string
{
    char_buffer<kPathMax> out{};

    if (vk_kobj_query("fs/root_path", out.data(), out.size(), nullptr) && out[0] != '\0') {
        return std::string(out.data());
    }

    return "/";
}

}  // namespace

namespace shell {

/* Initializes the shell's root and current working directory. */
void init_paths()
{
    s_root_path = query_default_path();
    s_cwd = s_root_path;
}

/* Removes leading ASCII whitespace from a shell argument string. */
auto trim_left(const std::string& text) -> std::string
{
    const vk_usize start = text.find_first_not_of(" \t");
    if (start == std::string::npos) {
        return std::string();
    }
    return text.substr(start);
}

/* Splits the next applet argument while honoring shell quotes. */
auto next_argument(const std::string& input) -> argument_token
{
    const parsed_token token = read_next_token(input);
    return {token.text, token.rest, token.valid};
}

/* Returns the shell's configured root directory. */
auto root_path() -> const std::string&
{
    return s_root_path;
}

/* Returns the shell's current working directory. */
auto current_working_directory() -> const std::string&
{
    return s_cwd;
}

/* Updates the shell's current working directory. */
void set_current_working_directory(const std::string& path)
{
    s_cwd = path;
}

/* Extracts the final path component from a filesystem path. */
auto basename(const std::string& path) -> std::string
{
    vk_usize base = 0;
    for (vk_usize index = 0; index < path.size(); ++index) {
        if (is_separator(path[index])) {
            base = index + 1;
        }
    }
    return path.substr(base);
}

/* Resolves a user-supplied path against the shell's current directory. */
auto resolve_path(const std::string& raw, std::string& out) -> bool
{
    return resolve_path_from(s_cwd, raw, out);
}

/* Finds a registered shell builtin by exact command name. */
auto find_builtin_command(const std::string& name) -> const command_spec*
{
    const command_list_view commands = applet_commands();
    for (const command_spec* command : std::span(commands.data, commands.count)) {
        if (command != nullptr && name == std::string(command->name)) {
            return command;
        }
    }
    return nullptr;
}

/* Returns true when the user already provided a path instead of a bare command. */
auto is_explicit_program_path(const std::string& program) -> bool
{
    if (program.empty()) {
        return false;
    }

    if (is_separator(program[0]) || program[0] == '.') {
        return true;
    }

    for (char ch : program) {
        if (is_separator(ch)) {
            return true;
        }
    }

    return false;
}

/* Resolves bare command names into /bin while preserving explicit paths. */
auto resolve_program_path(const std::string& raw, std::string& out) -> bool
{
    if (is_explicit_program_path(raw)) {
        return resolve_path(raw, out);
    }

    out = "/bin/";
    out += raw;
    return out.size() + 1 <= kPathMax;
}

/* Checks whether a path can be listed as a directory. */
auto directory_exists(const std::string& path) -> bool
{
    char_buffer<128> response{};
    vk_kobj_rpc_path_json("fs_list", path.c_str(), response.data(), response.size());
    return vk_kobj_response_ok(response.data());
}

/* Resolves and launches a userspace program, then waits for it to finish. */
auto launch_program(const std::string& command_line, int verbose) -> int
{
    const parsed_token program = read_next_token(command_line);
    if (!program.valid) {
        if (verbose) {
            std::cout << "Usage: run <program> [args...]\n";
        }
        return -1;
    }

    std::string path;
    if (!resolve_program_path(program.text, path)) {
        if (verbose) {
            std::cout << "run: program not found: ";
            std::cout << program.text;
            std::cout << '\n';
        }
        return -1;
    }

    std::string resolved_path;
    if (VK_CALL(file_exists, path.c_str())) {
        resolved_path = path;
    } else {
        if (ends_with(path, ".vbin")) {
            if (verbose) {
                std::cout << "run: program not found: ";
                std::cout << program.text;
                std::cout << '\n';
            }
            return -1;
        }

        path += ".vbin";
        if (path.size() + 1 > kPathMax || !VK_CALL(file_exists, path.c_str())) {
            if (verbose) {
                std::cout << "run: program not found: ";
                std::cout << program.text;
                std::cout << '\n';
            }
            return -1;
        }
        resolved_path = path;
    }

    std::string resolved_cmdline;
    if (resolved_path.find(" ") != std::string::npos
        || resolved_path.find("\t") != std::string::npos) {
        resolved_cmdline += "\"";
        resolved_cmdline += resolved_path;
        resolved_cmdline += "\"";
    } else {
        resolved_cmdline = resolved_path;
    }

    if (!program.rest.empty()) {
        resolved_cmdline += " ";
        resolved_cmdline += program.rest;
    }

    if (resolved_cmdline.size() + 1 > kLineMax) {
        if (verbose) {
            std::cout << "run: command line too long\n";
        }
        return -1;
    }

    const vk_i64 task_id = vk_get_api()->vk_run_cmdline(resolved_cmdline.c_str());
    if (task_id < 0) {
        if (verbose) {
            std::cout << "run: failed to launch ";
            std::cout << resolved_path;
            std::cout << '\n';
        }
        return -1;
    }

    if (verbose) {
        std::cout << "run: spawned task ";
        std::cout << task_id;
        std::cout << "\n";
    }

    VK_CALL(wait_task, task_id);
    return 0;
}

}  // namespace shell
