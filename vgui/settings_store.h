#ifndef VGUI_SETTINGS_STORE_H
#define VGUI_SETTINGS_STORE_H

#include "sqlite_db.h"

namespace vgui {

struct PersistedSettings {
    int style_index = 0;
    float font_scale = 1.0f;
    bool transparency = false;
    bool show_info = true;
    bool show_console = true;
    bool show_task_manager = false;
    bool show_kobj = false;
    bool show_vkfm = false;

    [[nodiscard]] auto equals(const PersistedSettings& other) const -> bool
    {
        return style_index == other.style_index
            && font_scale == other.font_scale
            && transparency == other.transparency
            && show_info == other.show_info
            && show_console == other.show_console
            && show_task_manager == other.show_task_manager
            && show_kobj == other.show_kobj
            && show_vkfm == other.show_vkfm;
    }
};

class SettingsStore {
public:
    [[nodiscard]] auto open(vk::string_view path) -> bool;
    [[nodiscard]] auto load(PersistedSettings& settings) -> bool;
    [[nodiscard]] auto save(const PersistedSettings& settings) -> bool;

    [[nodiscard]] auto last_error() const -> const std::string& { return last_error_; }

private:
    [[nodiscard]] auto ensure_schema() -> bool;
    [[nodiscard]] auto store_value(vk::string_view key, int value) -> bool;
    [[nodiscard]] auto store_value(vk::string_view key, bool value) -> bool;
    [[nodiscard]] auto store_value(vk::string_view key, float value) -> bool;
    [[nodiscard]] auto store_text(vk::string_view key, vk::string_view value) -> bool;

    void set_error_from_db();

    SQLiteDatabase database_;
    std::string last_error_;
};

} // namespace vgui

#endif // VGUI_SETTINGS_STORE_H
