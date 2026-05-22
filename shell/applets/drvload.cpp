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

    vk_kobj_named_cmd_json("drvload", name.c_str());
}

}  // namespace applet::drvload
