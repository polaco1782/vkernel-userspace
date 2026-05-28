/*
 * vkernel userspace - shell support
 * Copyright (C) 2026 vkernel authors
 */

#include "../include/vk.h"
#include "applets/applet.h"

#include <array>
#include <iostream>
#include <ranges>
#include <span>
#include <string>
#include <vector>

namespace {

using size_type = vk_usize;

constexpr size_type kLineMax = 256;
constexpr size_type kPathMax = 256;
constexpr size_type kFsResponseMax = 4096;
constexpr size_type kFsItemsMax = 64;
constexpr size_type kFsItemMax = 96;
constexpr size_type kPathComponentMax = 32;
constexpr size_type kPathSegmentMax = 64;

template <size_type N>
using char_buffer = std::array<char, N>;

struct parsed_token {
    std::string text;
    std::string rest;
    bool valid = false;
};

struct fs_item {
    std::string name;
    vk_u64 size = 0;
    bool is_directory = false;
    bool valid = false;
};

std::string s_root_path = "/";
std::string s_cwd = "/";

/* Returns true when a path ends with the requested suffix. */
auto ends_with(const std::string& text, const char* suffix) -> bool
{
    size_type suffix_length = 0;
    while (suffix[suffix_length] != '\0') {
        ++suffix_length;
    }

    if (suffix_length > text.size()) {
        return false;
    }

    const size_type start = text.size() - suffix_length;
    for (size_type index = 0; index < suffix_length; ++index) {
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

/* Treats both Unix and DOS separators as valid shell path delimiters. */
auto is_separator(char ch) -> bool
{
    return ch == '/' || ch == '\\';
}

/* Normalizes an absolute path by collapsing repeated separators and dot segments. */
auto normalize_absolute_path(const std::string& path, std::string& out) -> bool
{
    std::vector<std::string> components;
    size_type index = 0;

    while (index < path.size()) {
        while (index < path.size() && is_separator(path[index])) {
            ++index;
        }
        if (index >= path.size()) {
            break;
        }

        const size_type start = index;
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

/* Parses one fs_list entry in the compact shell-specific record format. */
auto parse_fs_item(const char* record) -> fs_item
{
    fs_item item{};
    if (record == nullptr || record[0] == '\0' || record[1] != '\t') {
        return item;
    }

    item.is_directory = record[0] == 'D';

    size_type cursor = 2;
    while (record[cursor] != '\0' && record[cursor] != '\t') {
        item.name.push_back(record[cursor++]);
    }

    if (record[cursor] != '\t' || item.name.empty()) {
        item.name.clear();
        return item;
    }

    ++cursor;
    while (record[cursor] >= '0' && record[cursor] <= '9') {
        item.size = item.size * 10ULL + static_cast<vk_u64>(record[cursor] - '0');
        ++cursor;
    }

    item.valid = true;
    return item;
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
    const size_type start = text.find_first_not_of(" \t");
    if (start == std::string::npos) {
        return std::string();
    }
    return text.substr(start);
}

/* Writes a single character to the shell console. */
void put_char(char ch)
{
    std::cout.put(ch);
}

/* Writes an unsigned decimal value to the shell console. */
void put_dec(vk_u64 value)
{
    std::cout << value;
}

/* Writes a repeated space run for simple text alignment. */
void put_spaces(vk_usize count)
{
    while (count-- > 0) {
        put_char(' ');
    }
}

/* Writes a string padded to a fixed display width. */
void put_padded(const std::string& text, vk_usize width)
{
    std::cout << text;
    if (text.size() < width) {
        put_spaces(width - text.size());
    }
}

/* Writes a C string padded to a fixed display width. */
void put_padded(const char* text, vk_usize width)
{
    vk_usize length = 0;
    while (text[length] != '\0') {
        ++length;
    }

    std::cout << text;
    if (length < width) {
        put_spaces(width - length);
    }
}

/* Prints an unsigned value with left padding to a target column width. */
void put_dec_width(vk_u64 value, vk_usize width)
{
    char_buffer<21> digits{};
    vk_usize length = 0;

    if (value == 0) {
        digits[length++] = '0';
    } else {
        while (value > 0 && length < digits.size()) {
            digits[length++] = static_cast<char>('0' + (value % 10ULL));
            value /= 10ULL;
        }
    }

    if (length < width) {
        put_spaces(width - length);
    }

    while (length > 0) {
        put_char(digits[--length]);
    }
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
    size_type base = 0;
    for (size_type index = 0; index < path.size(); ++index) {
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

/* Prints one shell ls row for either a file or directory. */
void print_ls_entry(const std::string& name, bool is_directory, vk_u64 size)
{
    std::cout << (is_directory ? "d " : "f ");

    if (is_directory) {
        std::cout << name;
        std::cout << "/\n";
        return;
    }

    put_padded(name, 28);
    std::cout << "  ";
    put_dec_width(size, 8);
    put_char('\n');
}

/* Lists a directory and renders the entries using the shell table format. */
auto list_directory(const std::string& path) -> bool
{
    char_buffer<kFsResponseMax> response{};
    std::array<char_buffer<kFsItemMax>, kFsItemsMax> raw_items{};
    int printed = 0;

    vk_kobj_rpc_path_json("fs_list", path.c_str(), response.data(), response.size());
    if (!vk_kobj_response_ok(response.data())) {
        return false;
    }

    const int item_count = vk_json_extract_string_array_field(response.data(),
                                                              "items",
                                                              raw_items[0].data(),
                                                              kFsItemMax,
                                                              kFsItemsMax);

    const auto items = std::span(raw_items).first(static_cast<size_type>(item_count));
    for (const auto& raw_item : items) {
        const fs_item item = parse_fs_item(raw_item.data());
        if (!item.valid) {
            continue;
        }

        print_ls_entry(item.name, item.is_directory, item.size);
        printed = 1;
    }

    if (printed == 0) {
        std::cout << "(empty)\n";
    }

    return true;
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
            put_char('\n');
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
                put_char('\n');
            }
            return -1;
        }

        path += ".vbin";
        if (path.size() + 1 > kPathMax || !VK_CALL(file_exists, path.c_str())) {
            if (verbose) {
                std::cout << "run: program not found: ";
                std::cout << program.text;
                put_char('\n');
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
            put_char('\n');
        }
        return -1;
    }

    if (verbose) {
        std::cout << "run: spawned task ";
        put_dec(static_cast<vk_u64>(task_id));
        std::cout << "\n";
    }

    VK_CALL(wait_task, task_id);
    return 0;
}

}  // namespace shell
