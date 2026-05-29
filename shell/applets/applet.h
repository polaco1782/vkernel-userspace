#pragma once

#include "../../include/vk.h"

#include <array>
#include <iostream>
#include <ranges>
#include <span>
#include <string>
#include <vector>

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
auto root_path() -> const std::string&;
auto current_working_directory() -> const std::string&;
void set_current_working_directory(const std::string& path);
auto basename(const std::string& path) -> std::string;
auto resolve_path(const std::string& raw, std::string& out) -> bool;
auto directory_exists(const std::string& path) -> bool;
auto launch_program(const std::string& command_line, int verbose) -> int;

} // namespace shell
