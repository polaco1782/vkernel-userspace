/*
 * vkernel userspace - ps
 * Copyright (C) 2026 vkernel authors
 *
 * ps.cpp - ps applet for vkernel shell
 *
 * Build: see Makefile (Linux) or shell.vcxproj (Visual Studio).
 * Run:   vk> ps
 */

#include "applet.h"

#include <array>
#include <iostream>
#include <string>

namespace applet::ps {

namespace {

constexpr vk_usize kMaxTasks = 64;

static void put_text(const char* text)
{
    std::cout << text;
}

static void put_char(char ch)
{
    std::cout.put(ch);
}

static void put_dec(vk_u64 value)
{
    std::cout << value;
}

static void put_spaces(vk_usize count)
{
    while (count-- > 0) {
        put_char(' ');
    }
}

static auto c_string_length(const char* text) -> vk_usize
{
    vk_usize length = 0;
    while (text != nullptr && text[length] != '\0') {
        ++length;
    }
    return length;
}

static void put_padded(const char* text, vk_usize width)
{
    const vk_usize length = c_string_length(text);
    put_text(text != nullptr ? text : "");
    if (length < width) {
        put_spaces(width - length);
    }
}

static auto trim_left(const std::string& text) -> std::string
{
    const vk_usize start = text.find_first_not_of(" \t");
    if (start == std::string::npos) {
        return std::string();
    }
    return text.substr(start);
}

static auto task_state_label(vk_u32 state) -> const char*
{
    switch (state) {
    case 0u:
        return "ready";
    case 1u:
        return "run";
    case 2u:
        return "sleep";
    case 3u:
        return "done";
    default:
        return "?";
    }
}

static void put_cpu_label(vk_u32 cpu)
{
    if (cpu == VK_TASK_CPU_NONE) {
        put_char('-');
        return;
    }

    put_dec(cpu);
}

} // namespace

static void run(const std::string& arg);

extern const shell::command_spec kCommand = {
    "ps",
    "List running tasks",
    run,
};

static void run(const std::string& arg)
{
    if (!trim_left(arg).empty()) {
        put_text("Usage: ps\n");
        return;
    }

    std::array<vk_task_info_t, kMaxTasks> tasks{};
    const vk_usize total = VK_CALL(task_snapshot, tasks.data(), tasks.size());
    const vk_usize count = total < tasks.size() ? total : tasks.size();

    put_padded("ID", 6);
    put_padded("STATE", 8);
    put_padded("CPU", 6);
    put_padded("TICKS", 12);
    put_text("NAME\n");

    for (vk_usize index = 0; index < count; ++index) {
        const vk_task_info_t& task = tasks[index];

        put_dec(task.id);
        if (task.id < 10ULL) {
            put_spaces(5);
        } else if (task.id < 100ULL) {
            put_spaces(4);
        } else if (task.id < 1000ULL) {
            put_spaces(3);
        } else if (task.id < 10000ULL) {
            put_spaces(2);
        } else if (task.id < 100000ULL) {
            put_spaces(1);
        }

        put_padded(task_state_label(task.state), 8);

        put_cpu_label(task.cpu);
        if (task.cpu == VK_TASK_CPU_NONE) {
            put_spaces(5);
        } else if (task.cpu < 10u) {
            put_spaces(5);
        } else if (task.cpu < 100u) {
            put_spaces(4);
        } else if (task.cpu < 1000u) {
            put_spaces(3);
        } else if (task.cpu < 10000u) {
            put_spaces(2);
        } else if (task.cpu < 100000u) {
            put_spaces(1);
        }

        put_dec(task.cpu_ticks);
        if (task.cpu_ticks < 10ULL) {
            put_spaces(11);
        } else if (task.cpu_ticks < 100ULL) {
            put_spaces(10);
        } else if (task.cpu_ticks < 1000ULL) {
            put_spaces(9);
        } else if (task.cpu_ticks < 10000ULL) {
            put_spaces(8);
        } else if (task.cpu_ticks < 100000ULL) {
            put_spaces(7);
        } else if (task.cpu_ticks < 1000000ULL) {
            put_spaces(6);
        } else if (task.cpu_ticks < 10000000ULL) {
            put_spaces(5);
        } else if (task.cpu_ticks < 100000000ULL) {
            put_spaces(4);
        } else if (task.cpu_ticks < 1000000000ULL) {
            put_spaces(3);
        } else if (task.cpu_ticks < 10000000000ULL) {
            put_spaces(2);
        } else if (task.cpu_ticks < 100000000000ULL) {
            put_spaces(1);
        }

        put_text(task.name);
        put_char('\n');
    }

    if (count < total) {
        put_text("ps: output truncated\n");
    }
}

} // namespace applet::ps
