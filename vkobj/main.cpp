#include "../include/vk.h"

#include <array>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

namespace {

constexpr vk_usize kValueMax = 2048;
constexpr vk_u64 kDefaultWatchIntervalMs = 500;

enum class command_kind {
    invalid,
    get,
    set,
    list,
    list_tree,
};

enum class list_status {
    ok,
    missing,
    not_container,
};

struct command_line {
    command_kind kind = command_kind::invalid;
    std::string path;
    std::string value;
    vk_u64 interval_ms = kDefaultWatchIntervalMs;
    bool watch = false;
};

struct child_entry {
    std::string name;
    vk_u32 type = VK_KOBJ_TYPE_ERR;
};

auto to_string(std::string_view text) -> std::string
{
    return std::string(text.data(), text.size());
}

void put_view(std::string_view text)
{
    std::cout.write(text.data(), text.size());
}

auto type_name(vk_u32 type) -> const char*
{
    switch (type) {
    case VK_KOBJ_TYPE_U64:
        return "u64";
    case VK_KOBJ_TYPE_I64:
        return "i64";
    case VK_KOBJ_TYPE_BOOL:
        return "bool";
    case VK_KOBJ_TYPE_STR:
        return "str";
    case VK_KOBJ_TYPE_ENUM:
        return "enum";
    case VK_KOBJ_TYPE_STRUCT:
        return "struct";
    case VK_KOBJ_TYPE_STREAM:
        return "stream";
    case VK_KOBJ_TYPE_ERR:
        return "err";
    default:
        return "unknown";
    }
}

auto sort_before(const child_entry& lhs, const child_entry& rhs) -> bool
{
    const bool lhs_struct = lhs.type == VK_KOBJ_TYPE_STRUCT;
    const bool rhs_struct = rhs.type == VK_KOBJ_TYPE_STRUCT;
    if (lhs_struct != rhs_struct) {
        return lhs_struct;
    }
    return lhs.name < rhs.name;
}

void sort_children(std::vector<child_entry>& children)
{
    for (vk_usize index = 1; index < children.size(); ++index) {
        child_entry current = children[index];
        vk_usize insert = index;
        while (insert > 0 && sort_before(current, children[insert - 1])) {
            children[insert] = children[insert - 1];
            --insert;
        }
        children[insert] = current;
    }
}

auto parse_u64_decimal(std::string_view text, vk_u64& value_out) -> bool
{
    if (text.empty()) {
        return false;
    }

    vk_u64 value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        value = value * 10ULL + static_cast<vk_u64>(ch - '0');
    }

    value_out = value;
    return true;
}

auto display_value(const vk_kobj_node_info_t& info, std::string_view raw_value) -> std::string
{
    if (info.readable != 0) {
        return to_string(raw_value);
    }
    if (info.type == VK_KOBJ_TYPE_STRUCT) {
        return "(struct)";
    }
    return "(not readable)";
}

auto join_path(std::string_view parent, std::string_view child) -> std::string
{
    if (parent.empty()) {
        return to_string(child);
    }

    std::string result = to_string(parent);
    result.push_back('/');
    result.append(child.data(), child.size());
    return result;
}

auto query_node(std::string_view path,
                std::array<char, kValueMax>& value_buffer,
                vk_kobj_node_info_t& info_out) -> bool
{
    std::string path_text = to_string(path);
    value_buffer.fill('\0');
    info_out = {};
    return vk_kobj_query(path_text.c_str(), value_buffer.data(), value_buffer.size(), &info_out) != 0;
}

auto get_display_value(std::string_view path,
                       std::array<char, kValueMax>& value_buffer,
                       vk_kobj_node_info_t& info_out,
                       std::string& display_out) -> bool
{
    if (!query_node(path, value_buffer, info_out)) {
        return false;
    }
    display_out = display_value(info_out, std::string_view(value_buffer.data()));
    return true;
}

auto collect_children(std::string_view path, std::vector<child_entry>& children) -> list_status
{
    children.clear();

    std::string path_text = to_string(path);
    const vk_usize total = vk_kobj_list(path_text.c_str(), nullptr, 0);
    if (total == 0) {
        if (path.empty()) {
            return list_status::ok;
        }

        std::array<char, 1> value_buffer {};
        vk_kobj_node_info_t info {};
        if (!vk_kobj_query(path_text.c_str(), value_buffer.data(), value_buffer.size(), &info)) {
            return list_status::missing;
        }
        return info.type == VK_KOBJ_TYPE_STRUCT ? list_status::ok : list_status::not_container;
    }

    std::vector<vk_kobj_child_t> raw_items(total);
    const vk_usize count = vk_kobj_list(path_text.c_str(), raw_items.data(), raw_items.size());
    for (const auto& raw_item : std::span(raw_items.data(), count < raw_items.size() ? count : raw_items.size())) {
        child_entry entry {};
        entry.name = raw_item.name;
        entry.type = raw_item.type;
        children.push_back(std::move(entry));
    }

    sort_children(children);
    return list_status::ok;
}

void print_usage()
{
    std::cout
        << "Usage:\n"
        << "  vkobj get <path> [--watch[=interval_ms]]\n"
        << "  vkobj set <path> <value>\n"
        << "  vkobj list [path]\n"
        << "  vkobj list-tree [path]\n"
        << "  vkobj watch <path> [interval_ms]\n";
}

auto join_args(int start, int argc, char** argv) -> std::string
{
    std::string result;
    for (int index = start; index < argc; ++index) {
        if (index > start) {
            result.push_back(' ');
        }
        result += argv[index];
    }
    return result;
}

auto parse_watch_suffix(std::string_view text, vk_u64& interval_out) -> bool
{
    if (text.empty()) {
        return true;
    }
    return parse_u64_decimal(text, interval_out);
}

auto parse_get_watch_args(command_line& command, int argc, char** argv, int start_index) -> bool
{
    for (int index = start_index; index < argc; ++index) {
        const std::string_view arg = argv[index];
        if (arg == "watch" || arg == "--watch" || arg == "-w") {
            command.watch = true;
            if (index + 1 < argc) {
                const std::string_view next = argv[index + 1];
                vk_u64 interval_ms = 0;
                if (parse_u64_decimal(next, interval_ms)) {
                    command.interval_ms = interval_ms;
                    ++index;
                }
            }
            continue;
        }

        if (arg.starts_with("--watch=")) {
            command.watch = true;
            if (!parse_watch_suffix(arg.substr(8), command.interval_ms)) {
                std::cout << "vkobj: invalid watch interval: ";
                put_view(arg.substr(8));
                std::cout << '\n';
                return false;
            }
            continue;
        }

        std::cout << "vkobj: unexpected argument: ";
        put_view(arg);
        std::cout << '\n';
        return false;
    }

    return true;
}

auto parse_command_line(int argc, char** argv, command_line& command) -> bool
{
    if (argc < 2) {
        print_usage();
        return false;
    }

    const std::string_view subcommand = argv[1];
    if (subcommand == "get") {
        if (argc < 3) {
            print_usage();
            return false;
        }

        command.kind = command_kind::get;
        command.path = argv[2];
        return parse_get_watch_args(command, argc, argv, 3);
    }

    if (subcommand == "watch") {
        if (argc < 3 || argc > 4) {
            print_usage();
            return false;
        }

        command.kind = command_kind::get;
        command.path = argv[2];
        command.watch = true;
        if (argc == 4 && !parse_u64_decimal(argv[3], command.interval_ms)) {
            std::cout << "vkobj: invalid watch interval: " << argv[3] << '\n';
            return false;
        }
        return true;
    }

    if (subcommand == "set") {
        if (argc < 4) {
            print_usage();
            return false;
        }

        command.kind = command_kind::set;
        command.path = argv[2];
        command.value = join_args(3, argc, argv);
        return true;
    }

    if (subcommand == "list") {
        if (argc > 3) {
            print_usage();
            return false;
        }

        command.kind = command_kind::list;
        if (argc == 3) {
            command.path = argv[2];
        }
        return true;
    }

    if (subcommand == "list-tree") {
        if (argc > 3) {
            print_usage();
            return false;
        }

        command.kind = command_kind::list_tree;
        if (argc == 3) {
            command.path = argv[2];
        }
        return true;
    }

    if (subcommand == "--help" || subcommand == "-h" || subcommand == "help") {
        print_usage();
        return false;
    }

    std::cout << "vkobj: unknown command: ";
    put_view(subcommand);
    std::cout << '\n';
    print_usage();
    return false;
}

auto print_get_once(std::string_view path) -> bool
{
    std::array<char, kValueMax> value_buffer {};
    vk_kobj_node_info_t info {};
    std::string display;
    if (!get_display_value(path, value_buffer, info, display)) {
        std::cout << "vkobj: path not found: ";
        put_view(path);
        std::cout << '\n';
        return false;
    }

    std::cout << display;
    if (display.empty() || display.back() != '\n') {
        std::cout << '\n';
    }
    return true;
}

void sleep_interval(vk_u64 interval_ms)
{
    if (interval_ms == 0) {
        VK_CALL(yield);
        return;
    }
    (void)usleep(static_cast<useconds_t>(interval_ms * 1000ULL));
}

auto watch_path(std::string_view path, vk_u64 interval_ms) -> bool
{
    std::array<char, kValueMax> value_buffer {};
    vk_kobj_node_info_t info {};
    std::string display;
    if (!get_display_value(path, value_buffer, info, display)) {
        std::cout << "vkobj: path not found: ";
        put_view(path);
        std::cout << '\n';
        return false;
    }

    for (;;) {
        if (!get_display_value(path, value_buffer, info, display)) {
            std::cout << "[" << VK_CALL(tick_count) << "] (unavailable)\n";
        } else if (display.find('\n') != std::string::npos) {
            std::cout << "[" << VK_CALL(tick_count) << "]\n" << display;
            if (display.back() != '\n') {
                std::cout << '\n';
            }
        } else {
            std::cout << "[" << VK_CALL(tick_count) << "] " << display << '\n';
        }
        sleep_interval(interval_ms);
    }
}

auto set_path_value(std::string_view path, std::string_view value) -> bool
{
    std::string path_text = to_string(path);
    std::string value_text = to_string(value);
    if (!vk_kobj_set_value(path_text.c_str(), value_text.c_str())) {
        std::cout << "vkobj: set failed: ";
        put_view(path);
        std::cout << '\n';
        return false;
    }

    return print_get_once(path);
}

auto print_list(std::string_view path) -> bool
{
    std::vector<child_entry> children;
    switch (collect_children(path, children)) {
    case list_status::missing:
        std::cout << "vkobj: path not found: ";
        put_view(path);
        std::cout << '\n';
        return false;
    case list_status::not_container:
        std::cout << "vkobj: not a container: ";
        put_view(path);
        std::cout << '\n';
        return false;
    case list_status::ok:
        break;
    }

    if (children.empty()) {
        std::cout << "(empty)\n";
        return true;
    }

    for (const child_entry& child : children) {
        std::cout << child.name << " [" << type_name(child.type) << "]\n";
    }
    return true;
}

auto print_tree_node(std::string_view full_path,
                     std::string_view label,
                     std::string_view prefix,
                     bool show_branch,
                     bool is_last) -> bool
{
    std::array<char, kValueMax> value_buffer {};
    vk_kobj_node_info_t info {};
    std::string display;
    if (!get_display_value(full_path, value_buffer, info, display)) {
        std::cout << "vkobj: path not found: ";
        put_view(full_path);
        std::cout << '\n';
        return false;
    }

    put_view(prefix);
    if (show_branch) {
        std::cout << (is_last ? "`- " : "|- ");
    }
    put_view(label);
    std::cout << " [" << type_name(info.type) << "]";
    if (info.type != VK_KOBJ_TYPE_STRUCT && info.readable != 0) {
        std::cout << " = " << display;
    }
    std::cout << '\n';

    if (info.type != VK_KOBJ_TYPE_STRUCT) {
        return true;
    }

    std::vector<child_entry> children;
    if (collect_children(full_path, children) != list_status::ok) {
        return false;
    }

    std::string child_prefix = to_string(prefix);
    if (show_branch) {
        child_prefix += is_last ? "   " : "|  ";
    }

    for (vk_usize index = 0; index < children.size(); ++index) {
        const child_entry& child = children[index];
        const bool child_last = index + 1 == children.size();
        if (!print_tree_node(join_path(full_path, child.name),
                             child.name,
                             child_prefix,
                             true,
                             child_last)) {
            return false;
        }
    }

    return true;
}

auto print_tree(std::string_view path) -> bool
{
    if (!path.empty()) {
        return print_tree_node(path, path, std::string_view(), false, true);
    }

    std::vector<child_entry> children;
    if (collect_children(std::string_view(), children) != list_status::ok) {
        std::cout << "vkobj: unable to list root\n";
        return false;
    }

    if (children.empty()) {
        std::cout << "(empty)\n";
        return true;
    }

    for (vk_usize index = 0; index < children.size(); ++index) {
        const child_entry& child = children[index];
        const bool is_last = index + 1 == children.size();
        if (!print_tree_node(child.name, child.name, std::string_view(), false, is_last)) {
            return false;
        }
    }

    return true;
}

} // namespace

int main(int argc, char** argv)
{
    command_line command;
    if (!parse_command_line(argc, argv, command)) {
        if (argc > 1) {
            const std::string_view subcommand = argv[1];
            if (subcommand == "--help" || subcommand == "-h" || subcommand == "help") {
                return 0;
            }
        }
        return 1;
    }

    switch (command.kind) {
    case command_kind::get:
        return command.watch ? (watch_path(command.path, command.interval_ms) ? 0 : 1)
                             : (print_get_once(command.path) ? 0 : 1);
    case command_kind::set:
        return set_path_value(command.path, command.value) ? 0 : 1;
    case command_kind::list:
        return print_list(command.path) ? 0 : 1;
    case command_kind::list_tree:
        return print_tree(command.path) ? 0 : 1;
    case command_kind::invalid:
        break;
    }

    return 1;
}
