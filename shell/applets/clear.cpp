/*
 * vkernel userspace - clear
 * Copyright (C) 2026 vkernel authors
 */

#include "applet.h"

#include <string>

namespace applet::clear {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "clear",
    "Clear the screen",
    run,
};

static void run(const std::string& arg)
{
    (void)arg;
    VK_CALL(clear);
}

}  // namespace applet::clear
