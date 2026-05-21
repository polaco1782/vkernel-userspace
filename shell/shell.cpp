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

#include <array>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#pragma warning(disable: 4702) /* unreachable code (infinite shell loop) */
#endif

namespace {

using size_type = vk_usize;

constexpr const char* kPrompt = "vk> ";
constexpr size_type kHistoryMax = 8;
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

using command_fn = void (*)(const std::string&);

struct command_spec {
    const char* name;
    const char* help;
    command_fn fn;
};

static void cmd_help(const std::string& arg);
static void cmd_version(const std::string& arg);
static void cmd_pwd(const std::string& arg);
static void cmd_cd(const std::string& arg);
static void cmd_ls(const std::string& arg);
static void cmd_cat(const std::string& arg);
static void cmd_clear(const std::string& arg);
static void cmd_reboot(const std::string& arg);
static void cmd_run(const std::string& arg);
static void cmd_drvload(const std::string& arg);
static void cmd_exit(const std::string& arg);

constexpr std::array<command_spec, 11> kCommands = {{
    {"help", "Show this message", cmd_help},
    {"version", "Show shell version", cmd_version},
    {"pwd", "Print current directory", cmd_pwd},
    {"cd", "Change current directory", cmd_cd},
    {"ls", "List files and directories", cmd_ls},
    {"cat", "Print a file", cmd_cat},
    {"clear", "Clear the screen", cmd_clear},
    {"reboot", "Reboot the machine", cmd_reboot},
    {"run", "Launch a program with args", cmd_run},
    {"drvload", "Load a driver (boot scripts use this)", cmd_drvload},
    {"exit", "Exit the shell", cmd_exit},
}};

static std::string s_root_path = "/";
static std::string s_cwd = "/";
static std::array<std::string, kHistoryMax> s_history{};
static size_type s_history_count = 0;

static void put_text(const char* text)
{
    VK_CALL(puts, text);
}

static void put_text(const std::string& text)
{
    VK_CALL(puts, text.c_str());
}

static auto starts_with(const std::string& text, const char* prefix) -> bool
{
    size_type index = 0;
    while (prefix[index] != '\0') {
        if (index >= text.size() || text[index] != prefix[index]) {
            return false;
        }
        ++index;
    }
    return true;
}

static auto ends_with(const std::string& text, const char* suffix) -> bool
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

static auto trim_left(const std::string& text) -> std::string
{
    const size_type start = text.find_first_not_of(" \t");
    if (start == std::string::npos) {
        return std::string();
    }
    return text.substr(start);
}

static auto contains_token_delimiter(const std::string& text) -> bool
{
    for (char ch : text) {
        if (ch == ' ' || ch == '\t') {
            return true;
        }
    }
    return false;
}

static auto has_spaces(const std::string& text) -> bool
{
    return text.find(" ") != std::string::npos || text.find("\t") != std::string::npos;
}

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

static void put_spaces(size_type count)
{
    while (count-- > 0) {
        VK_CALL(putc, ' ');
    }
}

static void put_padded(const std::string& text, size_type width)
{
    put_text(text);
    if (text.size() < width) {
        put_spaces(width - text.size());
    }
}

static void put_padded(const char* text, size_type width)
{
    size_type length = 0;
    while (text[length] != '\0') {
        ++length;
    }

    put_text(text);
    if (length < width) {
        put_spaces(width - length);
    }
}

static void put_dec_width(vk_u64 value, size_type width)
{
    char_buffer<21> digits{};
    size_type length = 0;

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
        VK_CALL(putc, digits[--length]);
    }
}

static auto shell_has_framebuffer() -> bool
{
    vk_framebuffer_info_t fb{};
    VK_CALL(framebuffer_info, &fb);
    return fb.valid != 0;
}

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

static void shell_history_add(const std::string& line)
{
    if (line.empty()) {
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

static void shell_redraw_line(const char* prompt, const std::string& line, size_type old_len)
{
    VK_CALL(putc, '\r');
    put_text(prompt);
    for (size_type index = 0; index < old_len; ++index) {
        VK_CALL(putc, ' ');
    }
    VK_CALL(putc, '\r');
    put_text(prompt);
    put_text(line);
}

static void shell_complete_line(std::string& line, size_type max, const char* prompt)
{
    if (contains_token_delimiter(line)) {
        return;
    }

    size_type match_count = 0;
    const char* match = nullptr;

    for (const auto& command : kCommands) {
        if (starts_with(std::string(command.name), line.c_str())) {
            match = command.name;
            ++match_count;
        }
    }

    if (match_count == 1 && match != nullptr) {
        size_type index = line.size();
        while (match[index] != '\0' && line.size() + 1 < max) {
            line.push_back(match[index]);
            VK_CALL(putc, match[index]);
            ++index;
        }
        if (line.size() + 1 < max) {
            line.push_back(' ');
            VK_CALL(putc, ' ');
        }
        return;
    }

    if (match_count > 1) {
        VK_CALL(putc, '\n');
        for (const auto& command : kCommands) {
            if (starts_with(std::string(command.name), line.c_str())) {
                put_text(command.name);
                put_text("  ");
            }
        }
        VK_CALL(putc, '\n');
        put_text(prompt);
        put_text(line);
    }
}

static auto console_getline(std::string& line, size_type max, const char* prompt) -> size_type
{
    line.clear();
    size_type history_index = s_history_count;
    size_type old_len = 0;

    while (line.size() + 1 < max) {
        const char ch = VK_CALL(getc);

        if (ch == '\r' || ch == '\n') {
            VK_CALL(putc, '\n');
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
            put_text("\b \b");
            old_len = line.size();
            continue;
        }

        if (ch >= ' ' && ch < 0x7F) {
            line.push_back(ch);
            VK_CALL(putc, ch);
            old_len = line.size();
        }
    }

    shell_history_add(line);
    return line.size();
}

static auto shell_is_separator(char ch) -> bool
{
    return ch == '/' || ch == '\\';
}

static auto shell_basename(const std::string& path) -> std::string
{
    size_type base = 0;
    for (size_type index = 0; index < path.size(); ++index) {
        if (shell_is_separator(path[index])) {
            base = index + 1;
        }
    }
    return path.substr(base);
}

static auto shell_normalize_absolute_path(const std::string& path, std::string& out) -> bool
{
    std::vector<std::string> components;
    size_type index = 0;

    while (index < path.size()) {
        while (index < path.size() && shell_is_separator(path[index])) {
            ++index;
        }
        if (index >= path.size()) {
            break;
        }

        const size_type start = index;
        while (index < path.size() && !shell_is_separator(path[index])) {
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

static auto shell_resolve_path_from(const std::string& base,
                                    const std::string& raw,
                                    std::string& out) -> bool
{
    const std::string input = trim_left(raw);
    if (input.empty()) {
        out = base;
        return out.size() + 1 <= kPathMax;
    }

    if (shell_is_separator(input[0])) {
        return shell_normalize_absolute_path(input, out);
    }

    std::string combined = base == "/" ? "/" : base + "/";
    combined += input;
    if (combined.size() + 1 > kPathMax) {
        return false;
    }

    return shell_normalize_absolute_path(combined, out);
}

static auto shell_resolve_path(const std::string& raw, std::string& out) -> bool
{
    return shell_resolve_path_from(s_cwd, raw, out);
}

static auto shell_query_default_path() -> std::string
{
    char_buffer<160> response{};
    char_buffer<kPathMax> out{};

    vk_kobj_rpc_path_json("get", "fs/root_path", response.data(), response.size());
    if (vk_kobj_response_ok(response.data()) &&
        vk_json_extract_string_field(response.data(), "value", out.data(), out.size()) &&
        out[0] != '\0') {
        return std::string(out.data());
    }

    return "/";
}

static void shell_init_paths()
{
    s_root_path = shell_query_default_path();
    s_cwd = s_root_path;
}

static auto shell_directory_exists(const std::string& path) -> bool
{
    char_buffer<128> response{};
    vk_kobj_rpc_path_json("fs_list", path.c_str(), response.data(), response.size());
    return vk_kobj_response_ok(response.data());
}

static auto shell_parse_fs_item(const char* record) -> fs_item
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

static void shell_print_ls_entry(const std::string& name, bool is_directory, vk_u64 size)
{
    put_text(is_directory ? "d " : "f ");

    if (is_directory) {
        put_text(name);
        put_text("/\n");
        return;
    }

    put_padded(name, 28);
    put_text("  ");
    put_dec_width(size, 8);
    VK_CALL(putc, '\n');
}

static auto shell_list_directory(const std::string& path) -> bool
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

    for (int index = 0; index < item_count; ++index) {
        const fs_item item = shell_parse_fs_item(raw_items[index].data());
        if (!item.valid) {
            continue;
        }

        shell_print_ls_entry(item.name, item.is_directory, item.size);
        printed = 1;
    }

    if (printed == 0) {
        put_text("(empty)\n");
    }

    return true;
}

static void cmd_help(const std::string& arg)
{
    (void)arg;
    put_text("Available commands:\n");
    for (const auto& command : kCommands) {
        put_text("  ");
        put_padded(command.name, 12);
        put_text(" - ");
        put_text(command.help);
        VK_CALL(putc, '\n');
    }
    put_text("  ?            - Alias for help\n");
    put_text("Programs can also be launched directly: foo or foo.vbin\n");
}

static void cmd_version(const std::string& arg)
{
    (void)arg;
    put_text("vkernel shell\n");
    put_text("  API version: ");
    VK_CALL(put_dec, VK_API_VERSION);
    put_text("\n");
}

static void cmd_pwd(const std::string& arg)
{
    (void)arg;
    put_text(s_cwd);
    VK_CALL(putc, '\n');
}

static void cmd_cd(const std::string& arg)
{
    std::string raw = trim_left(arg);
    if (raw.empty()) {
        raw = s_root_path;
    }

    std::string path;
    if (!shell_resolve_path(raw, path)) {
        put_text("cd: path too long\n");
        return;
    }

    if (!shell_directory_exists(path)) {
        put_text("cd: directory not found: ");
        put_text(raw);
        VK_CALL(putc, '\n');
        return;
    }

    s_cwd = path;
}

static void cmd_ls(const std::string& arg)
{
    const std::string raw = trim_left(arg);
    std::string path;

    if (!shell_resolve_path(raw, path)) {
        put_text("ls: path too long\n");
        return;
    }

    if (shell_list_directory(path)) {
        return;
    }

    if (VK_CALL(file_exists, path.c_str())) {
        shell_print_ls_entry(shell_basename(path), false, VK_CALL(file_size, path.c_str()));
        return;
    }

    put_text("ls: not found: ");
    put_text(raw.empty() ? path : raw);
    VK_CALL(putc, '\n');
}

static void cmd_cat(const std::string& arg)
{
    const std::string raw = trim_left(arg);
    if (raw.empty()) {
        put_text("Usage: cat <filename>\n");
        return;
    }

    std::string path;
    if (!shell_resolve_path(raw, path)) {
        put_text("cat: path too long\n");
        return;
    }

    const vk_file_handle_t fh = VK_CALL(file_open, path.c_str(), "r");
    if (fh == static_cast<vk_file_handle_t>(0)) {
        put_text("cat: file not found: ");
        put_text(raw);
        put_text("\n");
        return;
    }

    std::array<unsigned char, 128> buffer{};
    size_type read_size = 0;
    while ((read_size = VK_CALL(file_read_handle, fh, buffer.data(), buffer.size())) > 0) {
        for (size_type index = 0; index < read_size; ++index) {
            VK_CALL(putc, static_cast<char>(buffer[index]));
        }
    }

    VK_CALL(file_close, fh);
    put_text("\n");
}

static void cmd_clear(const std::string& arg)
{
    (void)arg;
    VK_CALL(clear);
}

static void cmd_reboot(const std::string& arg)
{
    (void)arg;
    vk_kobj_cmd_json("reboot");
}

static auto shell_try_resolve_program_path(const std::string& raw_path, std::string& out) -> bool
{
    std::string path;
    if (!shell_resolve_path(raw_path, path)) {
        return false;
    }

    if (VK_CALL(file_exists, path.c_str())) {
        out = path;
        return true;
    }

    if (ends_with(path, ".vbin")) {
        return false;
    }

    path += ".vbin";
    if (path.size() + 1 > kPathMax || !VK_CALL(file_exists, path.c_str())) {
        return false;
    }

    out = path;
    return true;
}

static auto shell_build_resolved_command_line(const std::string& path,
                                              const std::string& rest,
                                              std::string& out) -> bool
{
    out.clear();
    if (has_spaces(path)) {
        out += "\"";
        out += path;
        out += "\"";
    } else {
        out = path;
    }

    if (!rest.empty()) {
        out += " ";
        out += rest;
    }

    return out.size() + 1 <= kLineMax;
}

static auto shell_launch_program(const std::string& command_line, int verbose) -> int
{
    const parsed_token program = read_next_token(command_line);
    if (!program.valid) {
        if (verbose) {
            put_text("Usage: run <program> [args...]\n");
        }
        return -1;
    }

    std::string resolved_path;
    if (!shell_try_resolve_program_path(program.text, resolved_path)) {
        if (verbose) {
            put_text("run: program not found: ");
            put_text(program.text);
            VK_CALL(putc, '\n');
        }
        return -1;
    }

    std::string resolved_cmdline;
    if (!shell_build_resolved_command_line(resolved_path, program.rest, resolved_cmdline)) {
        if (verbose) {
            put_text("run: command line too long\n");
        }
        return -1;
    }

    const vk_i64 task_id = vk_get_api()->vk_run_cmdline(resolved_cmdline.c_str());
    if (task_id < 0) {
        if (verbose) {
            put_text("run: failed to launch ");
            put_text(resolved_path);
            VK_CALL(putc, '\n');
        }
        return -1;
    }

    if (verbose) {
        put_text("run: spawned task ");
        VK_CALL(put_dec, static_cast<vk_u64>(task_id));
        put_text("\n");
    }

    VK_CALL(wait_task, task_id);
    return 0;
}

static void cmd_run(const std::string& arg)
{
    (void)shell_launch_program(arg, 1);
}

static void cmd_drvload(const std::string& arg)
{
    const std::string name = trim_left(arg);
    if (name.empty()) {
        put_text("Usage: drvload <driver_name>\n");
        put_text("Example: drvload sb16.vko\n");
        return;
    }

    vk_kobj_named_cmd_json("drvload", name.c_str());
}

static void cmd_exit(const std::string& arg)
{
    (void)arg;
    VK_CALL(exit, 0);
}

static auto parse_cmdline(const std::string& cmdline) -> int
{
    const parsed_token command = read_next_token(cmdline);
    if (!command.valid) {
        return 0;
    }

    if (command.text == "?") {
        cmd_help(std::string());
        return 0;
    }

    for (const auto& spec : kCommands) {
        if (command.text == spec.name) {
            spec.fn(command.rest);
            return 0;
        }
    }

    if (shell_launch_program(cmdline, 0) == 0) {
        return 0;
    }

    put_text("Command not found: ");
    put_text(cmdline);
    put_text("\n");
    put_text("Type 'help' for available commands.\n");
    return -1;
}

static void run_script_line(const std::string& line)
{
    const std::string command = trim_left(line);
    if (command.empty() || command[0] == '#') {
        return;
    }

    put_text(kPrompt);
    put_text(command);
    put_text("\n");
    parse_cmdline(command);
}

static void read_startup_script()
{
    const vk_file_handle_t fh = VK_CALL(file_open, "shell.txt", "r");
    if (fh == static_cast<vk_file_handle_t>(0)) {
        put_text("No startup script found (shell.txt), skipping...\n");
        return;
    }

    char_buffer<kLineMax> chunk{};
    std::string line;
    size_type read_size = 0;

    while ((read_size = VK_CALL(file_read_handle, fh, chunk.data(), chunk.size())) > 0) {
        for (size_type index = 0; index < read_size; ++index) {
            const char ch = chunk[index];
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

    if (!line.empty()) {
        run_script_line(line);
    }

    VK_CALL(file_close, fh);
}

static void print_banner()
{
    put_text("\n\n");
    put_text("+----------------------------------+\n");
    put_text("|     vkernel userspace shell      |\n");
    put_text("+----------------------------------+\n");
    put_text("Type 'help' for available commands.\n\n");
}

}  // namespace

extern "C" int _start(const vk_api_t* api)
{
    vk_init(api);
    shell_init_paths();

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
        put_text(kPrompt);
        if (console_getline(line, kLineMax, kPrompt) == 0) {
            continue;
        }

        const std::string command = trim_left(line);
        if (command.empty()) {
            continue;
        }

        parse_cmdline(command);
    }

    return 0;
}
