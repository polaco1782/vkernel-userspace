#ifndef VKGUI_SETTINGS_STORE_H
#define VKGUI_SETTINGS_STORE_H

#include "sqlite_db.h"
#include "theme_scheme.h"

namespace vkgui {

struct PersistedSettings {
    int style_index = 0;
    float font_scale = 1.0f;
    bool transparency = false;
    bool show_info = true;
    bool show_console = true;
    bool show_task_manager = false;
    bool show_kobj = false;
    bool show_vkfm = false;
    bool show_text_editor = false;

    [[nodiscard]] auto compare(const PersistedSettings& other) const -> bool
    {
        return style_index == other.style_index
            && font_scale == other.font_scale
            && transparency == other.transparency
            && show_info == other.show_info
            && show_console == other.show_console
            && show_task_manager == other.show_task_manager
            && show_kobj == other.show_kobj
            && show_vkfm == other.show_vkfm
            && show_text_editor == other.show_text_editor;
    }
};

class SettingsStore {
public:
    [[nodiscard]] auto open(vk::string_view path) -> bool;
    [[nodiscard]] auto load(PersistedSettings& settings) -> bool;
    [[nodiscard]] auto load_font_family(int& font_family_index) -> bool;
    [[nodiscard]] auto load_theme_catalog(ThemeCatalog& catalog) -> bool;
    [[nodiscard]] auto save_theme_catalog(const ThemeCatalog& catalog) -> bool;
    [[nodiscard]] auto save(const PersistedSettings& settings) -> bool;
    [[nodiscard]] auto save_font_family(int font_family_index) -> bool;

    [[nodiscard]] auto last_error() const -> const std::string& { return last_error_; }
    [[nodiscard]] auto seeded_default_themes() const -> bool { return seeded_default_themes_; }

private:
    [[nodiscard]] auto ensure_schema() -> bool;
    [[nodiscard]] auto ensure_theme_color_alpha_column() -> bool;
    [[nodiscard]] auto ensure_default_themes() -> bool;
    [[nodiscard]] auto count_theme_schemes() -> int;
    [[nodiscard]] auto store_theme_catalog(const ThemeCatalog& catalog) -> bool;
    [[nodiscard]] auto insert_theme_scheme(int scheme_id, int sort_index, const ThemeScheme& scheme) -> bool;
    [[nodiscard]] auto insert_theme_color_override(int scheme_id, const ThemeColorOverride& color_override) -> bool;
    [[nodiscard]] auto insert_theme_style_override(int scheme_id, const ThemeStyleOverride& style_override) -> bool;
    [[nodiscard]] auto store_value(vk::string_view key, int value) -> bool;
    [[nodiscard]] auto store_value(vk::string_view key, bool value) -> bool;
    [[nodiscard]] auto store_value(vk::string_view key, float value) -> bool;
    [[nodiscard]] auto store_text(vk::string_view key, vk::string_view value) -> bool;

    void set_error_from_db();

    SQLiteDatabase database_;
    std::string last_error_;
    bool seeded_default_themes_ = false;
};

} // namespace vkgui

#endif // VKGUI_SETTINGS_STORE_H
