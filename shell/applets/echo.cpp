/*
 * vkernel userspace - echo
 * Copyright (C) 2026 vkernel authors
 */

#include "applet.h"

#include <iostream>
#include <string>

namespace applet::echo {

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "echo",
    "Print text",
    run,
};

static void run(const std::string& arg)
{
    std::string remaining = shell::trim_left(arg);
    bool newline = true;

    const shell::argument_token first = shell::next_argument(remaining);
    if (first.valid && first.text == "-n") {
        newline = false;
        remaining = first.rest;
    }

    bool needs_space = false;
    while (true) {
        const shell::argument_token token = shell::next_argument(remaining);
        if (!token.valid) {
            break;
        }

        if (needs_space) {
            std::cout << ' ';
        }
        std::cout << token.text;
        needs_space = true;
        remaining = token.rest;
    }

    if (newline) {
        std::cout << '\n';
    }
}

} // namespace applet::echo