/*
 * vkernel userspace - pwd
 * Copyright (C) 2026 vkernel authors
 */

#include "applet.h"

#include <iostream>
#include <string>

namespace applet::pwd {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "pwd",
    "Print current directory",
    run,
};

static void run(const std::string& arg)
{
    (void)arg;
    std::cout << shell::current_working_directory();
    std::cout << '\n';
}

}  // namespace applet::pwd
