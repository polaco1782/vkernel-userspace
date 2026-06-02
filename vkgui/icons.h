#ifndef VKGUI_ICONS_H
#define VKGUI_ICONS_H

#include "deps/iconfontcppheaders/IconsFontAwesome6.h"
#include "vkgui_common.h"

namespace vkgui {

inline auto icon_label(vk::string_view icon, vk::string_view label) -> std::string
{
    std::string combined = string_from_view(icon);
    if (!combined.empty()) {
        combined += "  ";
    }
    combined.append(label.data(), label.size());
    return combined;
}

inline auto file_icon_for_entry(vk::string_view name, bool is_directory) -> const char*
{
    if (is_directory) {
        return ICON_FA_FOLDER;
    }
    if (is_vbin_program_path(name)) {
        return ICON_FA_ROCKET;
    }
    if (ends_with(name, ".cpp") || ends_with(name, ".c") || ends_with(name, ".h") || ends_with(name, ".hpp")) {
        return ICON_FA_FILE_CODE;
    }
    if (ends_with(name, ".txt") || ends_with(name, ".md") || ends_with(name, ".ini") || ends_with(name, ".log")) {
        return ICON_FA_FILE_LINES;
    }
    if (ends_with(name, ".png") || ends_with(name, ".bmp") || ends_with(name, ".jpg")) {
        return ICON_FA_FILE_IMAGE;
    }
    if (ends_with(name, ".wav") || ends_with(name, ".mod") || ends_with(name, ".spc") || ends_with(name, ".mp3")) {
        return ICON_FA_FILE_AUDIO;
    }
    if (ends_with(name, ".zip") || ends_with(name, ".pak")) {
        return ICON_FA_FILE_ZIPPER;
    }
    if (ends_with(name, ".db")) {
        return ICON_FA_HARD_DRIVE;
    }
    return ICON_FA_FILE;
}

} // namespace vkgui

#endif // VKGUI_ICONS_H
