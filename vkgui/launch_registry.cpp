#include "launch_registry.h"

#include "console_log.h"

#include <string.h>

namespace vkgui {

namespace {

constexpr auto k_bin_directory_path = "/bin";

} // namespace

auto LaunchRegistry::load_from_bin_directory(ConsoleLog& log) -> bool
{
    std::string first_file_name;
    int file_count = 0;

    memset(response_.data(), 0, response_.size());
    memset(raw_items_.data(), 0, sizeof(raw_items_));

    vk_kobj_rpc_path_json("fs_list", k_bin_directory_path, response_.data(), response_.size());
    if (!vk_kobj_response_ok(response_.data())) {
        std::array<char, 128> error {};
        if (json_extract_string(response_.data(), "error", error)) {
            log.addf("Launch menu: failed to scan %s (%s).", k_bin_directory_path, error.data());
        } else {
            log.addf("Launch menu: failed to scan %s.", k_bin_directory_path);
        }
        return false;
    }

    const int item_count = vk_json_extract_string_array_field(response_.data(),
                                                              "items",
                                                              &raw_items_[0][0],
                                                              static_cast<vk_usize>(k_item_len),
                                                              k_items_max);
    for (int index = 0; index < item_count; ++index) {
        DirectoryListEntry item {};
        if (!parse_directory_list_item(raw_items_[index].data(), item)) {
            continue;
        }
        if (item.is_directory) {
            continue;
        }

        ++file_count;
        if (first_file_name.empty()) {
            first_file_name = item.name;
        }
        if (!is_vbin_program_path(item.name)) {
            continue;
        }

        std::string path = k_bin_directory_path;
        path.push_back('/');
        path += item.name;
        vk::string_view p = string_view_of(path);
        add_app(p);
    }

    if (count_ == 0 && file_count > 0) {
        log.addf("Launch menu: scanned %d file%s in %s but none matched executable naming; first entry was '%s'.",
                 file_count,
                 file_count == 1 ? "" : "s",
                 k_bin_directory_path,
                 first_file_name.c_str());
    }

    return true;
}

void LaunchRegistry::add_app(vk::string_view path)
{
    // Keep the launch menu stable when the directory scan returns duplicates or non-program entries.
    if (path.empty() || !is_vbin_program_path(path) || count_ >= k_capacity || exists(path)) {
        return;
    }

    LaunchMenuEntry& entry = entries_[count_++];
    entry.path = string_from_view(path);
    entry.label = string_from_view(path_basename(path));
}

auto LaunchRegistry::exists(vk::string_view path) const -> bool
{
    for (int index = 0; index < count_; ++index) {
        if (view_equals(entries_[index].path, path)) {
            return true;
        }
    }

    return false;
}

void LaunchRegistry::sort()
{
    for (int index = 1; index < count_; ++index) {
        LaunchMenuEntry entry = entries_[index];
        int insert_index = index;
        while (insert_index > 0 && entries_[insert_index - 1].label.compare(entry.label) > 0) {
            entries_[insert_index] = entries_[insert_index - 1];
            --insert_index;
        }
        entries_[insert_index] = entry;
    }
}

void LaunchRegistry::refresh(ConsoleLog& log)
{
    count_ = 0;

    const bool scanned = load_from_bin_directory(log);

    sort();

    if (scanned && count_ > 0) {
        log.addf("Launch menu loaded %d app%s from %s.",
                 count_,
                 count_ == 1 ? "" : "s",
                 k_bin_directory_path);
    } else if (scanned) {
        log.addf("Launch menu scan of %s did not find any runnable apps.", k_bin_directory_path);
    } else {
        log.addf("Launch menu scan of %s failed.", k_bin_directory_path);
    }
}

} // namespace vkgui
