#include "settings_store.h"

#include <stdio.h>

namespace vkgui {

namespace {

constexpr auto k_settings_schema =
    "CREATE TABLE IF NOT EXISTS settings ("
    " key TEXT PRIMARY KEY NOT NULL,"
    " value TEXT NOT NULL"
    ");";
constexpr auto k_theme_schema =
    "CREATE TABLE IF NOT EXISTS theme_schemes ("
    " id INTEGER PRIMARY KEY NOT NULL,"
    " sort_index INTEGER NOT NULL UNIQUE,"
    " name TEXT NOT NULL,"
    " base_style TEXT NOT NULL,"
    " use_win9x_chrome INTEGER NOT NULL DEFAULT 0,"
    " clear_r INTEGER NOT NULL,"
    " clear_g INTEGER NOT NULL,"
    " clear_b INTEGER NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS theme_color_values ("
    " scheme_id INTEGER NOT NULL,"
    " color_key TEXT NOT NULL,"
    " red REAL NOT NULL,"
    " green REAL NOT NULL,"
    " blue REAL NOT NULL,"
    " PRIMARY KEY (scheme_id, color_key)"
    ");"
    "CREATE TABLE IF NOT EXISTS theme_style_values ("
    " scheme_id INTEGER NOT NULL,"
    " value_key TEXT NOT NULL,"
    " value_kind TEXT NOT NULL,"
    " value_x REAL NOT NULL,"
    " value_y REAL NOT NULL DEFAULT 0,"
    " PRIMARY KEY (scheme_id, value_key)"
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

auto clamp_byte(int value) -> int
{
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return value;
}

auto theme_style_kind_name(ThemeStyleOverrideKind kind) -> const char*
{
    switch (kind) {
    case ThemeStyleOverrideKind::scalar:
        return "scalar";
    case ThemeStyleOverrideKind::vec2:
        return "vec2";
    case ThemeStyleOverrideKind::boolean:
        return "boolean";
    case ThemeStyleOverrideKind::integer:
        return "integer";
    }

    return "scalar";
}

auto parse_theme_style_kind(vk::string_view text, ThemeStyleOverrideKind& kind) -> bool
{
    if (string_equals(text, "scalar")) {
        kind = ThemeStyleOverrideKind::scalar;
        return true;
    }
    if (string_equals(text, "vec2")) {
        kind = ThemeStyleOverrideKind::vec2;
        return true;
    }
    if (string_equals(text, "boolean")) {
        kind = ThemeStyleOverrideKind::boolean;
        return true;
    }
    if (string_equals(text, "integer")) {
        kind = ThemeStyleOverrideKind::integer;
        return true;
    }
    return false;
}

} // namespace

auto SettingsStore::open(vk::string_view path) -> bool
{
    seeded_default_themes_ = false;

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

    if (!ensure_default_themes()) {
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

auto SettingsStore::load_theme_catalog(ThemeCatalog& catalog) -> bool
{
    catalog = ThemeCatalog();

    SQLiteStatement scheme_statement;
    if (!database_.prepare(
            "SELECT id, name, base_style, use_win9x_chrome, clear_r, clear_g, clear_b "
            "FROM theme_schemes ORDER BY sort_index, id;",
            scheme_statement)) {
        set_error_from_db();
        return false;
    }

    while (true) {
        const SQLiteStatement::StepResult result = scheme_statement.step();
        if (result == SQLiteStatement::StepResult::done) {
            last_error_.clear();
            return true;
        }
        if (result == SQLiteStatement::StepResult::error) {
            last_error_ = scheme_statement.last_error();
            return false;
        }

        if (catalog.count >= ThemeCatalog::k_max_schemes) {
            last_error_ = "theme catalog is full";
            return false;
        }

        const int scheme_id = scheme_statement.column_int(0);
        ThemeScheme& scheme = catalog.schemes[catalog.count++];
        scheme = ThemeScheme();
        scheme.name = scheme_statement.column_text(1);

        ThemeBaseStyle base_style = ThemeBaseStyle::dark;
        const std::string base_style_text = scheme_statement.column_text(2);
        if (parse_theme_base_style(string_view_of(base_style_text), base_style)) {
            scheme.base_style = base_style;
        }

        scheme.use_win9x_chrome = scheme_statement.column_int(3) != 0;
        scheme.clear_r = clamp_byte(scheme_statement.column_int(4));
        scheme.clear_g = clamp_byte(scheme_statement.column_int(5));
        scheme.clear_b = clamp_byte(scheme_statement.column_int(6));

        SQLiteStatement color_statement;
        if (!database_.prepare(
            "SELECT color_key, red, green, blue, alpha "
                "FROM theme_color_values WHERE scheme_id = ?1 ORDER BY color_key;",
                color_statement)) {
            set_error_from_db();
            return false;
        }
        if (!color_statement.bind_int(1, scheme_id)) {
            last_error_ = color_statement.last_error();
            return false;
        }

        while (true) {
            const SQLiteStatement::StepResult color_result = color_statement.step();
            if (color_result == SQLiteStatement::StepResult::done) {
                break;
            }
            if (color_result == SQLiteStatement::StepResult::error) {
                last_error_ = color_statement.last_error();
                return false;
            }

            const std::string color_key = color_statement.column_text(0);
            const int color_index = theme_color_index_for_key(string_view_of(color_key));
            if (color_index < 0) {
                continue;
            }

            if (!scheme.add_color_override(color_index,
                                           static_cast<float>(color_statement.column_double(1)),
                                           static_cast<float>(color_statement.column_double(2)),
                                           static_cast<float>(color_statement.column_double(3)),
                                           static_cast<float>(color_statement.column_double(4)))) {
                last_error_ = "theme has too many color overrides";
                return false;
            }
        }

        SQLiteStatement style_statement;
        if (!database_.prepare(
                "SELECT value_key, value_kind, value_x, value_y "
                "FROM theme_style_values WHERE scheme_id = ?1 ORDER BY value_key;",
                style_statement)) {
            set_error_from_db();
            return false;
        }
        if (!style_statement.bind_int(1, scheme_id)) {
            last_error_ = style_statement.last_error();
            return false;
        }

        while (true) {
            const SQLiteStatement::StepResult style_result = style_statement.step();
            if (style_result == SQLiteStatement::StepResult::done) {
                break;
            }
            if (style_result == SQLiteStatement::StepResult::error) {
                last_error_ = style_statement.last_error();
                return false;
            }

            ThemeStyleOverrideKey key;
            const std::string key_text = style_statement.column_text(0);
            if (!parse_theme_style_key(string_view_of(key_text), key)) {
                continue;
            }

            ThemeStyleOverrideKind kind;
            const std::string kind_text = style_statement.column_text(1);
            if (!parse_theme_style_kind(string_view_of(kind_text), kind)) {
                continue;
            }

            const float value_x = static_cast<float>(style_statement.column_double(2));
            const float value_y = static_cast<float>(style_statement.column_double(3));
            bool ok = false;
            switch (kind) {
            case ThemeStyleOverrideKind::scalar:
                ok = scheme.add_style_float_override(key, value_x);
                break;
            case ThemeStyleOverrideKind::vec2:
                ok = scheme.add_style_vec2_override(key, value_x, value_y);
                break;
            case ThemeStyleOverrideKind::boolean:
                ok = scheme.add_style_bool_override(key, value_x != 0.0f);
                break;
            case ThemeStyleOverrideKind::integer:
                ok = scheme.add_style_int_override(key, style_statement.column_int(2));
                break;
            }

            if (!ok) {
                last_error_ = "theme has too many style overrides";
                return false;
            }
        }
    }
}

auto SettingsStore::save_theme_catalog(const ThemeCatalog& catalog) -> bool
{
    return store_theme_catalog(catalog);
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
    if (!database_.exec(k_theme_schema)) {
        set_error_from_db();
        return false;
    }
    if (!ensure_theme_color_alpha_column()) {
        return false;
    }
    return true;
}

auto SettingsStore::ensure_theme_color_alpha_column() -> bool
{
    if (database_.exec("ALTER TABLE theme_color_values ADD COLUMN alpha REAL NOT NULL DEFAULT 1.0;")) {
        last_error_.clear();
        return true;
    }

    const std::string error_text = database_.last_error();
    if (find_substring(string_view_of(error_text), "duplicate column name") != k_not_found) {
        last_error_.clear();
        return true;
    }

    last_error_ = error_text;
    return false;
}

auto SettingsStore::ensure_default_themes() -> bool
{
    const int scheme_count = count_theme_schemes();
    if (scheme_count < 0) {
        return false;
    }
    if (scheme_count > 0) {
        seeded_default_themes_ = false;
        return true;
    }

    const ThemeCatalog defaults = builtin_theme_catalog();
    if (!store_theme_catalog(defaults)) {
        return false;
    }

    seeded_default_themes_ = true;
    last_error_.clear();
    return true;
}

auto SettingsStore::count_theme_schemes() -> int
{
    SQLiteStatement statement;
    if (!database_.prepare("SELECT COUNT(*) FROM theme_schemes;", statement)) {
        set_error_from_db();
        return -1;
    }

    const SQLiteStatement::StepResult result = statement.step();
    if (result == SQLiteStatement::StepResult::row) {
        return statement.column_int(0);
    }
    if (result == SQLiteStatement::StepResult::error) {
        last_error_ = statement.last_error();
        return -1;
    }

    last_error_ = "failed to count theme schemes";
    return -1;
}

auto SettingsStore::store_theme_catalog(const ThemeCatalog& catalog) -> bool
{
    if (!database_.exec("BEGIN;")) {
        set_error_from_db();
        return false;
    }

    bool ok = database_.exec(
        "DELETE FROM theme_style_values;"
        "DELETE FROM theme_color_values;"
        "DELETE FROM theme_schemes;");

    for (int index = 0; ok && index < catalog.count; ++index) {
        const int scheme_id = index + 1;
        const ThemeScheme& scheme = catalog.schemes[index];
        ok = insert_theme_scheme(scheme_id, index, scheme);
        for (int color_index = 0; ok && color_index < scheme.color_override_count; ++color_index) {
            ok = insert_theme_color_override(scheme_id, scheme.color_overrides[color_index]);
        }
        for (int style_index = 0; ok && style_index < scheme.style_override_count; ++style_index) {
            ok = insert_theme_style_override(scheme_id, scheme.style_overrides[style_index]);
        }
    }

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

auto SettingsStore::insert_theme_scheme(int scheme_id, int sort_index, const ThemeScheme& scheme) -> bool
{
    SQLiteStatement statement;
    if (!database_.prepare(
            "INSERT INTO theme_schemes "
            "(id, sort_index, name, base_style, use_win9x_chrome, clear_r, clear_g, clear_b) "
            "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8);",
            statement)) {
        set_error_from_db();
        return false;
    }

    if (!statement.bind_int(1, scheme_id)
        || !statement.bind_int(2, sort_index)
        || !statement.bind_text(3, string_view_of(scheme.name))
        || !statement.bind_text(4, theme_base_style_name(scheme.base_style))
        || !statement.bind_int(5, scheme.use_win9x_chrome ? 1 : 0)
        || !statement.bind_int(6, clamp_byte(scheme.clear_r))
        || !statement.bind_int(7, clamp_byte(scheme.clear_g))
        || !statement.bind_int(8, clamp_byte(scheme.clear_b))) {
        last_error_ = statement.last_error();
        return false;
    }

    if (statement.step() != SQLiteStatement::StepResult::done) {
        last_error_ = statement.last_error();
        return false;
    }

    return true;
}

auto SettingsStore::insert_theme_color_override(int scheme_id, const ThemeColorOverride& color_override) -> bool
{
    const char* color_key = theme_color_key_for_index(color_override.color_index);
    if (color_key == nullptr) {
        last_error_ = "theme color override has unknown key";
        return false;
    }

    SQLiteStatement statement;
    if (!database_.prepare(
            "INSERT INTO theme_color_values (scheme_id, color_key, red, green, blue, alpha) "
            "VALUES (?1, ?2, ?3, ?4, ?5, ?6);",
            statement)) {
        set_error_from_db();
        return false;
    }

    if (!statement.bind_int(1, scheme_id)
        || !statement.bind_text(2, color_key)
        || !statement.bind_double(3, color_override.r)
        || !statement.bind_double(4, color_override.g)
        || !statement.bind_double(5, color_override.b)
        || !statement.bind_double(6, color_override.a)) {
        last_error_ = statement.last_error();
        return false;
    }

    if (statement.step() != SQLiteStatement::StepResult::done) {
        last_error_ = statement.last_error();
        return false;
    }

    return true;
}

auto SettingsStore::insert_theme_style_override(int scheme_id, const ThemeStyleOverride& style_override) -> bool
{
    const char* style_key = theme_style_key_name(style_override.key);
    if (style_key == nullptr) {
        last_error_ = "theme style override has unknown key";
        return false;
    }

    SQLiteStatement statement;
    if (!database_.prepare(
            "INSERT INTO theme_style_values (scheme_id, value_key, value_kind, value_x, value_y) "
            "VALUES (?1, ?2, ?3, ?4, ?5);",
            statement)) {
        set_error_from_db();
        return false;
    }

    const bool bound = statement.bind_int(1, scheme_id)
        && statement.bind_text(2, style_key)
        && statement.bind_text(3, theme_style_kind_name(style_override.kind))
        && (style_override.kind == ThemeStyleOverrideKind::integer
                ? statement.bind_int(4, static_cast<int>(style_override.x))
                : statement.bind_double(4, style_override.x))
        && statement.bind_double(5, style_override.y);

    if (!bound) {
        last_error_ = statement.last_error();
        return false;
    }

    if (statement.step() != SQLiteStatement::StepResult::done) {
        last_error_ = statement.last_error();
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
