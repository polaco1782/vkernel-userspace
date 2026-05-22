/*
 * vkernel userspace - reboot
 * Copyright (C) 2026 vkernel authors
 */

#include "applet.h"

#include <string>

namespace applet::reboot {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "reboot",
    "Reboot the machine",
    run,
};

static void run(const std::string& arg)
{
    (void)arg;
    vk_kobj_cmd_json("reboot");
}

}  // namespace applet::reboot
