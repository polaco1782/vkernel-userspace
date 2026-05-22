/*
 * vkernel userspace - cd
 * Copyright (C) 2026 vkernel authors
 */

#include "applet.h"

#include <iostream>
#include <string>

namespace applet::cd {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "cd",
    "Change current directory",
    run,
};

static void run(const std::string& arg)
{
    std::string raw = shell::trim_left(arg);
    if (raw.empty()) {
        raw = shell::root_path();
    }

    std::string path;
    if (!shell::resolve_path(raw, path)) {
        std::cout << "cd: path too long\n";
        return;
    }

    if (!shell::directory_exists(path)) {
        std::cout << "cd: directory not found: ";
        std::cout << raw;
        shell::put_char('\n');
        return;
    }

    shell::set_current_working_directory(path);
}

}  // namespace applet::cd
