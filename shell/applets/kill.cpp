/*
 * vkernel userspace - kill
 * Copyright (C) 2026 vkernel authors
 */

#include "applet.h"

#include <iostream>
#include <string>

namespace applet::kill {

static void run(const std::string& arg);

/* Parses a decimal task id and rejects trailing non-whitespace input. */
auto parse_task_id(const std::string& arg, vk_u64& out_task_id) -> bool
{
    const std::string text = shell::trim_left(arg);
    if (text.empty()) {
        return false;
    }

    vk_u64 value = 0;
    vk_usize index = 0;
    while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
        value = value * 10ULL + static_cast<vk_u64>(text[index] - '0');
        ++index;
    }

    if (index == 0) {
        return false;
    }

    const std::string rest = shell::trim_left(text.substr(index));
    if (!rest.empty()) {
        return false;
    }

    out_task_id = value;
    return true;
}

extern const shell::command_spec kCommand = {
    "kill",
    "Terminate a task by id",
    run,
};

static void run(const std::string& arg)
{
    vk_u64 task_id = 0;
    if (!parse_task_id(arg, task_id)) {
        std::cout << "Usage: kill <task_id>\n";
        return;
    }

    if (!vk_terminate_task(task_id)) {
        std::cout << "kill: failed to terminate task ";
        shell::put_dec(task_id);
        std::cout << "\n";
        return;
    }

    std::cout << "kill: termination requested for task ";
    shell::put_dec(task_id);
    std::cout << "\n";
}

}  // namespace applet::kill
