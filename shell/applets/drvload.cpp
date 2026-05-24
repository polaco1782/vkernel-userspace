/*
 * vkernel userspace - drvload
 * Copyright (C) 2026 vkernel authors
 */

#include "applet.h"

#include <iostream>
#include <string>

namespace applet::drvload {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "drvload",
    "Load a driver (boot scripts use this)",
    run,
};

static void run(const std::string& arg)
{
    const std::string name = shell::trim_left(arg);
    if (name.empty()) {
        std::cout << "Usage: drvload <driver_name>\n";
        std::cout << "Example: drvload sb16.vko\n";
        return;
    }

    const int rc = vk_driver_load(name.c_str());
    if (rc != 0) {
        std::cout << "drvload: failed to load ";
        std::cout << name;
        std::cout << '\n';
    }
}

}  // namespace applet::drvload
