/*
 * vkernel userspace - run
 * Copyright (C) 2026 vkernel authors
 */

#include "applet.h"

#include <string>

namespace applet::run {

static void run_command(const std::string& arg);

extern const shell::command_spec kCommand = {
    "run",
    "Launch a program with args",
    run_command,
};

static void run_command(const std::string& arg)
{
    (void)shell::launch_program(arg, 1);
}

}  // namespace applet::run
