/*
 * vkernel userspace - version
 * Copyright (C) 2026 vkernel authors
 */

#include "applet.h"

#include <iostream>
#include <string>

namespace applet::version {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "version",
    "Show shell version",
    run,
};

static void run(const std::string& arg)
{
    (void)arg;
    std::cout << "vkernel shell\n";
    std::cout << "  API version: ";
    shell::put_dec(VK_API_VERSION);
    std::cout << "\n";
}

}  // namespace applet::version
