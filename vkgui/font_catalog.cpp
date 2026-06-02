#include "font_catalog.h"

#include "deps/fonts/dejavu_sans.h"
#include "deps/fonts/liberation_mono_regular.h"
#include "deps/fonts/liberation_sans_regular.h"
#include "deps/imguinotify/fa-solid-900.h"
#include "icons.h"

namespace vkgui {

namespace {

struct FontOption {
    UiFontFamily family;
    const char* name;
    unsigned char* data;
    unsigned int size;
    float size_pixels;
};

constexpr float k_icon_font_size = 13.0f;
static constexpr ImWchar k_icon_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };

const FontOption k_font_options[] = {
    { UiFontFamily::default_builtin, "Default", nullptr, 0, 0.0f },
    { UiFontFamily::dejavu_sans, "DejaVu Sans", dejavu_sans_ttf, dejavu_sans_ttf_len, 15.0f },
    { UiFontFamily::liberation_sans, "Liberation Sans", liberation_sans_regular_ttf, liberation_sans_regular_ttf_len, 15.0f },
    { UiFontFamily::liberation_mono, "Liberation Mono", liberation_mono_regular_ttf, liberation_mono_regular_ttf_len, 15.0f },
};

auto find_font_option(UiFontFamily family) -> const FontOption&
{
    for (const FontOption& option : k_font_options) {
        if (option.family == family) {
            return option;
        }
    }
    return k_font_options[0];
}

void merge_icon_font(ImFontAtlas& atlas)
{
    ImFontConfig icon_cfg;
    icon_cfg.MergeMode = true;
    icon_cfg.PixelSnapH = false;
    icon_cfg.GlyphMinAdvanceX = k_icon_font_size;
    icon_cfg.OversampleH = 2;
    icon_cfg.OversampleV = 2;
    atlas.AddFontFromMemoryCompressedTTF(fa_solid_900_compressed_data,
                                         fa_solid_900_compressed_size,
                                         k_icon_font_size,
                                         &icon_cfg,
                                         k_icon_ranges);
}

} // namespace

auto ui_font_family_count() -> int
{
    return static_cast<int>(std::size(k_font_options));
}

auto ui_font_family_name(UiFontFamily family) -> const char*
{
    return find_font_option(family).name;
}

auto ui_font_family_from_index(int index) -> UiFontFamily
{
    if (index < 0 || index >= ui_font_family_count()) {
        return UiFontFamily::default_builtin;
    }
    return k_font_options[index].family;
}

auto ui_font_family_index(UiFontFamily family) -> int
{
    for (int index = 0; index < ui_font_family_count(); ++index) {
        if (k_font_options[index].family == family) {
            return index;
        }
    }
    return 0;
}

auto configure_ui_fonts(ImGuiIO& io, UiFontFamily family) -> bool
{
    ImFontAtlas& atlas = *io.Fonts;
    atlas.Clear();

    ImFont* base_font = nullptr;
    const FontOption& option = find_font_option(family);
    if (option.data == nullptr || option.size == 0) {
        base_font = atlas.AddFontDefault();
    } else {
        ImFontConfig base_cfg;
        base_cfg.FontDataOwnedByAtlas = false;
        base_cfg.OversampleH = 1;
        base_cfg.OversampleV = 1;
        base_font = atlas.AddFontFromMemoryTTF(option.data,
                                               static_cast<int>(option.size),
                                               option.size_pixels,
                                               &base_cfg,
                                               atlas.GetGlyphRangesDefault());
    }

    if (base_font == nullptr) {
        atlas.Clear();
        base_font = atlas.AddFontDefault();
        family = UiFontFamily::default_builtin;
    }

    merge_icon_font(atlas);
    io.FontDefault = base_font;
    return base_font != nullptr;
}

} // namespace vkgui
