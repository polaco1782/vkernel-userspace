#pragma once

#include "../../include/vk.h"

#include <string>

namespace shell {

using command_fn = void (*)(const std::string&);

struct command_spec {
    const char* name;
    const char* help;
    command_fn fn;
};

struct command_list_view {
    const command_spec* const* data;
    vk_usize count;
};

auto applet_commands() -> command_list_view;
void init_paths();
auto trim_left(const std::string& text) -> std::string;
void put_char(char ch);
void put_dec(vk_u64 value);
void put_spaces(vk_usize count);
void put_padded(const std::string& text, vk_usize width);
void put_padded(const char* text, vk_usize width);
void put_dec_width(vk_u64 value, vk_usize width);
auto root_path() -> const std::string&;
auto current_working_directory() -> const std::string&;
void set_current_working_directory(const std::string& path);
auto basename(const std::string& path) -> std::string;
auto resolve_path(const std::string& raw, std::string& out) -> bool;
auto directory_exists(const std::string& path) -> bool;
void print_ls_entry(const std::string& name, bool is_directory, vk_u64 size);
auto list_directory(const std::string& path) -> bool;
auto launch_program(const std::string& command_line, int verbose) -> int;

} // namespace shell
