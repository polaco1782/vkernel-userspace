/*
 * vkernel userspace - help
 * Copyright (C) 2026 vkernel authors
 */

#include "applet.h"

#include <iostream>
#include <span>
#include <string>

namespace applet::help {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "help",
    "Show this message",
    run,
};

static void run(const std::string& arg)
{
    (void)arg;
    std::cout << "Available commands:\n";
    const shell::command_list_view commands = shell::applet_commands();
    for (const auto& command : std::span(commands.data, commands.count)) {
        std::cout << "  ";
        shell::put_padded(command.name, 12);
        std::cout << " - ";
        std::cout << command.help;
        shell::put_char('\n');
    }
    std::cout << "  ?            - Alias for help\n";
    std::cout << "Programs can also be launched directly: foo or foo.vbin\n";
}

}  // namespace applet::help
