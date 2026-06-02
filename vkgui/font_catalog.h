#ifndef VKGUI_FONT_CATALOG_H
#define VKGUI_FONT_CATALOG_H

#include "vkgui_common.h"

namespace vkgui {

enum class UiFontFamily {
    default_builtin = 0,
    dejavu_sans,
    liberation_sans,
    liberation_mono,
};

[[nodiscard]] auto ui_font_family_count() -> int;
[[nodiscard]] auto ui_font_family_name(UiFontFamily family) -> const char*;
[[nodiscard]] auto ui_font_family_from_index(int index) -> UiFontFamily;
[[nodiscard]] auto ui_font_family_index(UiFontFamily family) -> int;
[[nodiscard]] auto configure_ui_fonts(ImGuiIO& io, UiFontFamily family) -> bool;

} // namespace vkgui

#endif // VKGUI_FONT_CATALOG_H
