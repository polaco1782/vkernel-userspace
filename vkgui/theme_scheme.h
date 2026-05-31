#ifndef VKGUI_THEME_SCHEME_H
#define VKGUI_THEME_SCHEME_H

#include "vkgui_common.h"

namespace vkgui {

enum class ThemeBaseStyle {
    dark,
    light,
    classic,
};

enum class ThemeStyleOverrideKind {
    scalar,
    vec2,
    boolean,
    integer,
};

enum class ThemeStyleOverrideKey {
    alpha,
    disabled_alpha,
    window_padding,
    window_rounding,
    window_border_size,
    window_min_size,
    window_title_align,
    window_menu_button_position,
    child_rounding,
    child_border_size,
    frame_rounding,
    popup_rounding,
    scrollbar_rounding,
    grab_rounding,
    tab_rounding,
    frame_border_size,
    popup_border_size,
    tab_border_size,
    frame_padding,
    item_spacing,
    item_inner_spacing,
    cell_padding,
    touch_extra_padding,
    indent_spacing,
    columns_min_spacing,
    scrollbar_size,
    grab_min_size,
    log_slider_deadzone,
    tab_min_width_for_close_button,
    tab_bar_border_size,
    tab_bar_overline_size,
    table_angled_headers_angle,
    table_angled_headers_text_align,
    color_button_position,
    button_text_align,
    selectable_text_align,
    separator_text_border_size,
    separator_text_align,
    separator_text_padding,
    display_window_padding,
    display_safe_area_padding,
    mouse_cursor_scale,
    anti_aliased_fill,
    anti_aliased_lines,
    anti_aliased_lines_use_tex,
    curve_tessellation_tol,
    circle_tessellation_max_error,
    hover_stationary_delay,
    hover_delay_short,
    hover_delay_normal,
    hover_flags_for_tooltip_mouse,
    hover_flags_for_tooltip_nav,
};

struct ThemeColorOverride {
    int color_index = 0;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

struct ThemeStyleOverride {
    ThemeStyleOverrideKey key = ThemeStyleOverrideKey::window_rounding;
    ThemeStyleOverrideKind kind = ThemeStyleOverrideKind::scalar;
    float x = 0.0f;
    float y = 0.0f;
};

struct ThemeScheme {
    static constexpr int k_max_color_overrides = ImGuiCol_COUNT;
    static constexpr int k_max_style_overrides = 80;

    std::string name;
    ThemeBaseStyle base_style = ThemeBaseStyle::dark;
    int clear_r = 22;
    int clear_g = 22;
    int clear_b = 30;
    std::array<ThemeColorOverride, k_max_color_overrides> color_overrides {};
    int color_override_count = 0;
    std::array<ThemeStyleOverride, k_max_style_overrides> style_overrides {};
    int style_override_count = 0;

    [[nodiscard]] auto add_color_override(int color_index, float r, float g, float b, float a = 1.0f) -> bool;
    [[nodiscard]] auto add_style_float_override(ThemeStyleOverrideKey key, float value) -> bool;
    [[nodiscard]] auto add_style_vec2_override(ThemeStyleOverrideKey key, float x, float y) -> bool;
    [[nodiscard]] auto add_style_bool_override(ThemeStyleOverrideKey key, bool value) -> bool;
    [[nodiscard]] auto add_style_int_override(ThemeStyleOverrideKey key, int value) -> bool;
};

struct ThemeCatalog {
    static constexpr int k_max_schemes = 16;

    std::array<ThemeScheme, k_max_schemes> schemes {};
    int count = 0;

    [[nodiscard]] auto add_scheme(vk::string_view name,
                                  ThemeBaseStyle base_style,
                                  int clear_r,
                                  int clear_g,
                                  int clear_b) -> ThemeScheme*;
};

[[nodiscard]] auto builtin_theme_catalog() -> ThemeCatalog;
[[nodiscard]] auto theme_base_style_name(ThemeBaseStyle base_style) -> const char*;
[[nodiscard]] auto parse_theme_base_style(vk::string_view text, ThemeBaseStyle& base_style) -> bool;
[[nodiscard]] auto theme_color_key_for_index(int color_index) -> const char*;
[[nodiscard]] auto theme_color_index_for_key(vk::string_view key) -> int;
[[nodiscard]] auto theme_style_key_name(ThemeStyleOverrideKey key) -> const char*;
[[nodiscard]] auto parse_theme_style_key(vk::string_view text, ThemeStyleOverrideKey& key) -> bool;
[[nodiscard]] auto theme_scheme_from_style(vk::string_view name,
                                           ThemeBaseStyle base_style,
                                           int clear_r,
                                           int clear_g,
                                           int clear_b,
                                           const ImGuiStyle& style) -> ThemeScheme;
void apply_theme_scheme(const ThemeScheme& scheme);

} // namespace vkgui

#endif // VKGUI_THEME_SCHEME_H