#include "frontend.h"

#include <stdio.h>
#include <string.h>

namespace vspcplay_frontend {

namespace {

auto browser_is_supported_file(const char* name, vk_u64 size_bytes) -> bool
{
    if (name == nullptr || name[0] == '\0' || size_bytes == 0)
        return false;

    return ends_with_casefolded(name, ".spc");
}

auto browser_should_include_entry(const BrowserEntry& entry) -> bool
{
    if (entry.is_directory)
        return true;

    return browser_is_supported_file(entry.name.c_str(), entry.size_bytes);
}

auto parse_entry_record(const char* record, BrowserEntry* entry) -> bool
{
    if (record == nullptr || entry == nullptr)
        return false;
    if ((record[0] != 'D' && record[0] != 'F') || record[1] != '\t')
        return false;

    const char* second_tab = strchr(record + 2, '\t');
    if (second_tab == nullptr || second_tab <= record + 2)
        return false;

    entry->name.assign(record + 2, static_cast<size_t>(second_tab - (record + 2)));
    entry->is_directory = record[0] == 'D';
    entry->size_bytes = parse_u64_decimal(second_tab + 1);
    return !entry->name.empty();
}

auto compare_casefolded(const std::string& lhs, const std::string& rhs) -> int
{
    const char* lhs_text = lhs.c_str();
    const char* rhs_text = rhs.c_str();
    while (*lhs_text != '\0' || *rhs_text != '\0') {
        char lhs_ch = *lhs_text;
        char rhs_ch = *rhs_text;
        if (lhs_ch >= 'A' && lhs_ch <= 'Z')
            lhs_ch = static_cast<char>(lhs_ch - 'A' + 'a');
        if (rhs_ch >= 'A' && rhs_ch <= 'Z')
            rhs_ch = static_cast<char>(rhs_ch - 'A' + 'a');
        if (lhs_ch != rhs_ch)
            return lhs_ch < rhs_ch ? -1 : 1;
        if (*lhs_text != '\0')
            ++lhs_text;
        if (*rhs_text != '\0')
            ++rhs_text;
    }
    return 0;
}

void sort_entries(BrowserState* browser)
{
    for (vk_u32 index = 1; index < browser->entry_count; ++index) {
        BrowserEntry entry = browser->entries[index];
        vk_u32 insert_index = index;

        while (insert_index > 0) {
            const BrowserEntry& previous = browser->entries[insert_index - 1];
            const bool directories_first = entry.is_directory && !previous.is_directory;
            const bool names_precede =
                entry.is_directory == previous.is_directory
                && compare_casefolded(entry.name, previous.name) < 0;
            if (!directories_first && !names_precede)
                break;

            browser->entries[insert_index] = previous;
            --insert_index;
        }

        browser->entries[insert_index] = entry;
    }
}

} // namespace

void browser_open(AppState* app)
{
    if (app == nullptr)
        return;

    app->browser.open = true;
    if (app->browser.current_path.empty())
        app->browser.current_path = "/data/vspcplay/tracks";

    if (!browser_refresh_listing(app)) {
        app->browser.current_path = "/data";
        (void)browser_refresh_listing(app);
    }
}

auto browser_refresh_listing(AppState* app) -> bool
{
    if (app == nullptr)
        return false;

    BrowserState* browser = &app->browser;
    memset(browser->response.data(), 0, browser->response.size());
    memset(browser->raw_items.data(), 0, browser->raw_items.size() * sizeof(browser->raw_items[0]));

    vk_kobj_rpc_path_json("fs_list",
                          browser->current_path.c_str(),
                          browser->response.data(),
                          browser->response.size());
    if (!vk_kobj_response_ok(browser->response.data())) {
        char error[kStatusMax] = {};
        browser->entry_count = 0;
        browser->selected_index = 0;
        browser->scroll_index = 0;
        if (vk_json_extract_string_field(browser->response.data(), "error", error, sizeof(error))) {
            browser->status = error;
        } else {
            browser->status = "FAILED TO LIST DIRECTORY";
        }
        return false;
    }

    const int item_count = vk_json_extract_string_array_field(browser->response.data(),
                                                              "items",
                                                              &browser->raw_items[0][0],
                                                              kBrowserItemMax,
                                                              static_cast<int>(kBrowserMaxEntries));

    browser->entry_count = 0;
    browser->selected_index = 0;
    browser->scroll_index = 0;
    for (int index = 0; index < item_count && browser->entry_count < kBrowserMaxEntries; ++index) {
        BrowserEntry entry;
        if (!parse_entry_record(browser->raw_items[static_cast<size_t>(index)].data(), &entry))
            continue;
        if (!browser_should_include_entry(entry))
            continue;
        browser->entries[browser->entry_count++] = entry;
    }

    sort_entries(browser);
    if (browser->entry_count == 0) {
        browser->status = "NO SPC FILES OR DIRECTORIES";
    } else {
        char buffer[kStatusMax] = {};
        snprintf(buffer,
                 sizeof(buffer),
                 "%u ITEM%s",
                 browser->entry_count,
                 browser->entry_count == 1 ? "" : "S");
        browser->status = buffer;
    }
    return true;
}

void browser_navigate_to_parent(AppState* app)
{
    if (app == nullptr)
        return;
    app->browser.current_path = path_parent(app->browser.current_path);
    (void)browser_refresh_listing(app);
}

auto browser_activate_selection(AppState* app) -> bool
{
    if (app == nullptr || app->browser.entry_count == 0)
        return false;

    const BrowserEntry& entry = app->browser.entries[app->browser.selected_index];
    if (entry.is_directory) {
        app->browser.current_path = path_join(app->browser.current_path, entry.name);
        return browser_refresh_listing(app);
    }

    app->playlist.clear();
    app->playlist.push_back(path_join(app->browser.current_path, entry.name));
    app->current_index = 0;
    app->browser.open = false;
    app->request_reload = true;
    return true;
}

void browser_select_relative(AppState* app, int delta)
{
    if (app == nullptr || app->browser.entry_count == 0)
        return;

    int selected = static_cast<int>(app->browser.selected_index) + delta;
    if (selected < 0)
        selected = 0;
    if (selected >= static_cast<int>(app->browser.entry_count))
        selected = static_cast<int>(app->browser.entry_count) - 1;
    app->browser.selected_index = static_cast<vk_u32>(selected);

    if (app->browser.selected_index < app->browser.scroll_index) {
        app->browser.scroll_index = app->browser.selected_index;
        return;
    }

    constexpr vk_u32 kVisibleRows = 26;
    const vk_u32 selected_index = app->browser.selected_index;
    if (selected_index >= app->browser.scroll_index + kVisibleRows)
        app->browser.scroll_index = selected_index - kVisibleRows + 1u;
}

void browser_scroll_relative(AppState* app, int delta)
{
    browser_select_relative(app, delta * 26);
}

} // namespace vspcplay_frontend
