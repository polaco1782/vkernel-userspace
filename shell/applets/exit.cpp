/*
 * vkernel userspace - exit
 * Copyright (C) 2026 vkernel authors
 */

#include "applet.h"

#include <string>

namespace applet::exit {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "exit",
    "Exit the shell",
    run,
};

static void run(const std::string& arg)
{
    (void)arg;
    VK_CALL(exit, 0);
}

}  // namespace applet::exit
