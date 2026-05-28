#include "settings_store.h"

#include <stdio.h>

namespace vkgui {

namespace {

constexpr auto k_settings_schema =
    "CREATE TABLE IF NOT EXISTS settings ("
    " key TEXT PRIMARY KEY NOT NULL,"
    " value TEXT NOT NULL"
    ");";
constexpr auto k_settings_pragmas =
    "PRAGMA journal_mode=MEMORY;"
    "PRAGMA synchronous=OFF;";

auto parse_bool(vk::string_view value) -> bool
{
    return parse_i64(value) != 0;
}

auto parse_float(vk::string_view value, float fallback) -> float
{
    float parsed = fallback;
    const std::string text = string_from_view(value);
    if (sscanf(text.c_str(), "%f", &parsed) == 1) {
        return parsed;
    }
    return fallback;
}

} // namespace

auto SettingsStore::open(vk::string_view path) -> bool
{
    if (!database_.open(path)) {
        set_error_from_db();
        return false;
    }

    /* vkGUI settings favor simple persistence over crash-hard durability.
       Keeping the rollback journal in memory avoids filesystem features
       our current userspace storage layer does not fully emulate yet. */
    if (!database_.exec(k_settings_pragmas)) {
        database_.close();
        set_error_from_db();
        return false;
    }

    if (!ensure_schema()) {
        database_.close();
        return false;
    }

    last_error_.clear();
    return true;
}

auto SettingsStore::load(PersistedSettings& settings) -> bool
{
    SQLiteStatement statement;
    if (!database_.prepare("SELECT key, value FROM settings;", statement)) {
        set_error_from_db();
        return false;
    }

    while (true) {
        const SQLiteStatement::StepResult result = statement.step();
        if (result == SQLiteStatement::StepResult::done) {
            return true;
        }
        if (result == SQLiteStatement::StepResult::error) {
            last_error_ = statement.last_error();
            return false;
        }

        const std::string key = statement.column_text(0);
        const std::string value = statement.column_text(1);
        const vk::string_view key_view = string_view_of(key);
        const vk::string_view value_view = string_view_of(value);

        if (string_equals(key_view, "style_index")) {
            settings.style_index = static_cast<int>(parse_i64(value_view));
        } else if (string_equals(key_view, "font_scale")) {
            settings.font_scale = parse_float(value_view, settings.font_scale);
        } else if (string_equals(key_view, "transparency")) {
            settings.transparency = parse_bool(value_view);
        } else if (string_equals(key_view, "show_info")) {
            settings.show_info = parse_bool(value_view);
        } else if (string_equals(key_view, "show_console")) {
            settings.show_console = parse_bool(value_view);
        } else if (string_equals(key_view, "show_task_manager")) {
            settings.show_task_manager = parse_bool(value_view);
        } else if (string_equals(key_view, "show_kobj")) {
            settings.show_kobj = parse_bool(value_view);
        } else if (string_equals(key_view, "show_vkfm")) {
            settings.show_vkfm = parse_bool(value_view);
        }
    }
}

auto SettingsStore::save(const PersistedSettings& settings) -> bool
{
    if (!database_.exec("BEGIN;")) {
        set_error_from_db();
        return false;
    }

    const bool ok = store_value("style_index", settings.style_index)
        && store_value("font_scale", settings.font_scale)
        && store_value("transparency", settings.transparency)
        && store_value("show_info", settings.show_info)
        && store_value("show_console", settings.show_console)
        && store_value("show_task_manager", settings.show_task_manager)
        && store_value("show_kobj", settings.show_kobj)
        && store_value("show_vkfm", settings.show_vkfm);

    if (!ok) {
        (void)database_.exec("ROLLBACK;");
        return false;
    }

    if (!database_.exec("COMMIT;")) {
        set_error_from_db();
        (void)database_.exec("ROLLBACK;");
        return false;
    }

    last_error_.clear();
    return true;
}

auto SettingsStore::ensure_schema() -> bool
{
    if (!database_.exec(k_settings_schema)) {
        set_error_from_db();
        return false;
    }
    return true;
}

auto SettingsStore::store_value(vk::string_view key, int value) -> bool
{
    const std::string text = string_from_i64(static_cast<long long>(value));
    return store_text(key, string_view_of(text));
}

auto SettingsStore::store_value(vk::string_view key, bool value) -> bool
{
    return store_text(key, value ? "1" : "0");
}

auto SettingsStore::store_value(vk::string_view key, float value) -> bool
{
    std::array<char, 32> buffer {};
    snprintf(buffer.data(), buffer.size(), "%.3f", static_cast<double>(value));
    return store_text(key, buffer_view(buffer));
}

auto SettingsStore::store_text(vk::string_view key, vk::string_view value) -> bool
{
    SQLiteStatement statement;
    if (!database_.prepare(
            "INSERT INTO settings (key, value) VALUES (?1, ?2) "
            "ON CONFLICT(key) DO UPDATE SET value = excluded.value;",
            statement)) {
        set_error_from_db();
        return false;
    }

    if (!statement.bind_text(1, key) || !statement.bind_text(2, value)) {
        last_error_ = statement.last_error();
        return false;
    }

    if (statement.step() != SQLiteStatement::StepResult::done) {
        last_error_ = statement.last_error();
        return false;
    }

    return true;
}

void SettingsStore::set_error_from_db()
{
    last_error_ = database_.last_error();
}

} // namespace vkgui
