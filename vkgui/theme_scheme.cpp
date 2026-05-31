#include "theme_scheme.h"

namespace vkgui {

namespace {

struct RgbColor {
    unsigned int r;
    unsigned int g;
    unsigned int b;
};

struct ThemeStyleKeyEntry {
    ThemeStyleOverrideKey key;
    const char* name;
};

constexpr ThemeStyleKeyEntry k_theme_style_keys[] = {
    { ThemeStyleOverrideKey::alpha, "Alpha" },
    { ThemeStyleOverrideKey::disabled_alpha, "DisabledAlpha" },
    { ThemeStyleOverrideKey::window_padding, "WindowPadding" },
    { ThemeStyleOverrideKey::window_rounding, "WindowRounding" },
    { ThemeStyleOverrideKey::window_border_size, "WindowBorderSize" },
    { ThemeStyleOverrideKey::window_min_size, "WindowMinSize" },
    { ThemeStyleOverrideKey::window_title_align, "WindowTitleAlign" },
    { ThemeStyleOverrideKey::window_menu_button_position, "WindowMenuButtonPosition" },
    { ThemeStyleOverrideKey::child_rounding, "ChildRounding" },
    { ThemeStyleOverrideKey::child_border_size, "ChildBorderSize" },
    { ThemeStyleOverrideKey::popup_rounding, "PopupRounding" },
    { ThemeStyleOverrideKey::popup_border_size, "PopupBorderSize" },
    { ThemeStyleOverrideKey::frame_padding, "FramePadding" },
    { ThemeStyleOverrideKey::frame_rounding, "FrameRounding" },
    { ThemeStyleOverrideKey::frame_border_size, "FrameBorderSize" },
    { ThemeStyleOverrideKey::item_spacing, "ItemSpacing" },
    { ThemeStyleOverrideKey::item_inner_spacing, "ItemInnerSpacing" },
    { ThemeStyleOverrideKey::cell_padding, "CellPadding" },
    { ThemeStyleOverrideKey::touch_extra_padding, "TouchExtraPadding" },
    { ThemeStyleOverrideKey::indent_spacing, "IndentSpacing" },
    { ThemeStyleOverrideKey::columns_min_spacing, "ColumnsMinSpacing" },
    { ThemeStyleOverrideKey::scrollbar_size, "ScrollbarSize" },
    { ThemeStyleOverrideKey::scrollbar_rounding, "ScrollbarRounding" },
    { ThemeStyleOverrideKey::grab_min_size, "GrabMinSize" },
    { ThemeStyleOverrideKey::grab_rounding, "GrabRounding" },
    { ThemeStyleOverrideKey::log_slider_deadzone, "LogSliderDeadzone" },
    { ThemeStyleOverrideKey::tab_rounding, "TabRounding" },
    { ThemeStyleOverrideKey::tab_border_size, "TabBorderSize" },
    { ThemeStyleOverrideKey::tab_min_width_for_close_button, "TabMinWidthForCloseButton" },
    { ThemeStyleOverrideKey::tab_bar_border_size, "TabBarBorderSize" },
    { ThemeStyleOverrideKey::tab_bar_overline_size, "TabBarOverlineSize" },
    { ThemeStyleOverrideKey::table_angled_headers_angle, "TableAngledHeadersAngle" },
    { ThemeStyleOverrideKey::table_angled_headers_text_align, "TableAngledHeadersTextAlign" },
    { ThemeStyleOverrideKey::color_button_position, "ColorButtonPosition" },
    { ThemeStyleOverrideKey::button_text_align, "ButtonTextAlign" },
    { ThemeStyleOverrideKey::selectable_text_align, "SelectableTextAlign" },
    { ThemeStyleOverrideKey::separator_text_border_size, "SeparatorTextBorderSize" },
    { ThemeStyleOverrideKey::separator_text_align, "SeparatorTextAlign" },
    { ThemeStyleOverrideKey::separator_text_padding, "SeparatorTextPadding" },
    { ThemeStyleOverrideKey::display_window_padding, "DisplayWindowPadding" },
    { ThemeStyleOverrideKey::display_safe_area_padding, "DisplaySafeAreaPadding" },
    { ThemeStyleOverrideKey::mouse_cursor_scale, "MouseCursorScale" },
    { ThemeStyleOverrideKey::anti_aliased_fill, "AntiAliasedFill" },
    { ThemeStyleOverrideKey::anti_aliased_lines, "AntiAliasedLines" },
    { ThemeStyleOverrideKey::anti_aliased_lines_use_tex, "AntiAliasedLinesUseTex" },
    { ThemeStyleOverrideKey::curve_tessellation_tol, "CurveTessellationTol" },
    { ThemeStyleOverrideKey::circle_tessellation_max_error, "CircleTessellationMaxError" },
    { ThemeStyleOverrideKey::hover_stationary_delay, "HoverStationaryDelay" },
    { ThemeStyleOverrideKey::hover_delay_short, "HoverDelayShort" },
    { ThemeStyleOverrideKey::hover_delay_normal, "HoverDelayNormal" },
    { ThemeStyleOverrideKey::hover_flags_for_tooltip_mouse, "HoverFlagsForTooltipMouse" },
    { ThemeStyleOverrideKey::hover_flags_for_tooltip_nav, "HoverFlagsForTooltipNav" },
};

constexpr float k_compare_epsilon = 0.0005f;

[[nodiscard]] constexpr auto hex_digit_value(char digit) -> unsigned int
{
    if (digit >= '0' && digit <= '9') {
        return static_cast<unsigned int>(digit - '0');
    }
    if (digit >= 'a' && digit <= 'f') {
        return static_cast<unsigned int>(digit - 'a' + 10);
    }
    if (digit >= 'A' && digit <= 'F') {
        return static_cast<unsigned int>(digit - 'A' + 10);
    }
    return 0;
}

[[nodiscard]] constexpr auto is_hex_digit(char digit) -> bool
{
    return (digit >= '0' && digit <= '9')
        || (digit >= 'a' && digit <= 'f')
        || (digit >= 'A' && digit <= 'F');
}

[[nodiscard]] auto parse_rgb_hex(vk::string_view hex_string) -> RgbColor
{
    if (hex_string.size() != 7 || hex_string[0] != '#') {
        return {0, 0, 0};
    }
    for (vk_u32 index = 1; index < hex_string.size(); ++index) {
        if (!is_hex_digit(hex_string[index])) {
            return {0, 0, 0};
        }
    }

    return {
        (hex_digit_value(hex_string[1]) << 4) | hex_digit_value(hex_string[2]),
        (hex_digit_value(hex_string[3]) << 4) | hex_digit_value(hex_string[4]),
        (hex_digit_value(hex_string[5]) << 4) | hex_digit_value(hex_string[6]),
    };
}

[[nodiscard]] constexpr auto to_theme_color(const ImVec4& color) -> ThemeColorOverride
{
    return { 0, color.x, color.y, color.z, color.w };
}

[[nodiscard]] auto float_difference(float lhs, float rhs) -> float
{
    return lhs >= rhs ? lhs - rhs : rhs - lhs;
}

[[nodiscard]] auto nearly_equal(float lhs, float rhs) -> bool
{
    return float_difference(lhs, rhs) <= k_compare_epsilon;
}

[[nodiscard]] auto nearly_equal(const ImVec2& lhs, const ImVec2& rhs) -> bool
{
    return nearly_equal(lhs.x, rhs.x) && nearly_equal(lhs.y, rhs.y);
}

[[nodiscard]] auto nearly_equal(const ImVec4& lhs, const ImVec4& rhs) -> bool
{
    return nearly_equal(lhs.x, rhs.x)
        && nearly_equal(lhs.y, rhs.y)
        && nearly_equal(lhs.z, rhs.z)
        && nearly_equal(lhs.w, rhs.w);
}

void apply_base_style(ThemeBaseStyle base_style, ImGuiStyle* dst)
{
    switch (base_style) {
    case ThemeBaseStyle::dark:
        ImGui::StyleColorsDark(dst);
        break;
    case ThemeBaseStyle::light:
        ImGui::StyleColorsLight(dst);
        break;
    case ThemeBaseStyle::classic:
        ImGui::StyleColorsClassic(dst);
        break;
    }
}

[[nodiscard]] auto make_base_style(ThemeBaseStyle base_style) -> ImGuiStyle
{
    ImGuiStyle style;
    apply_base_style(base_style, &style);
    return style;
}

void add_hex_color_override(ThemeScheme* scheme, const ImGuiStyle& base_style, int color_index, vk::string_view hex)
{
    if (scheme == nullptr) {
        return;
    }

    const RgbColor color = parse_rgb_hex(hex);
    (void)scheme->add_color_override(color_index,
                                     static_cast<float>(color.r) / 255.0f,
                                     static_cast<float>(color.g) / 255.0f,
                                     static_cast<float>(color.b) / 255.0f,
                                     base_style.Colors[color_index].w);
}

void add_rgb_color_override(ThemeScheme* scheme,
                            const ImGuiStyle& base_style,
                            int color_index,
                            float r,
                            float g,
                            float b)
{
    if (scheme == nullptr) {
        return;
    }

    (void)scheme->add_color_override(color_index, r, g, b, base_style.Colors[color_index].w);
}

void add_default_renderer_tuning(ThemeScheme* scheme)
{
    if (scheme == nullptr) {
        return;
    }

    (void)scheme->add_style_float_override(ThemeStyleOverrideKey::window_rounding, 0.0f);
    (void)scheme->add_style_float_override(ThemeStyleOverrideKey::child_rounding, 0.0f);
    (void)scheme->add_style_float_override(ThemeStyleOverrideKey::frame_rounding, 0.0f);
    (void)scheme->add_style_float_override(ThemeStyleOverrideKey::popup_rounding, 0.0f);
    (void)scheme->add_style_float_override(ThemeStyleOverrideKey::scrollbar_rounding, 0.0f);
    (void)scheme->add_style_float_override(ThemeStyleOverrideKey::grab_rounding, 0.0f);
    (void)scheme->add_style_float_override(ThemeStyleOverrideKey::tab_rounding, 0.0f);
    (void)scheme->add_style_float_override(ThemeStyleOverrideKey::window_border_size, 1.0f);
    (void)scheme->add_style_float_override(ThemeStyleOverrideKey::frame_border_size, 0.0f);
    (void)scheme->add_style_bool_override(ThemeStyleOverrideKey::anti_aliased_lines, false);
    (void)scheme->add_style_bool_override(ThemeStyleOverrideKey::anti_aliased_lines_use_tex, false);
    (void)scheme->add_style_bool_override(ThemeStyleOverrideKey::anti_aliased_fill, false);
}

void apply_style_override(ImGuiStyle& style, const ThemeStyleOverride& override_value)
{
    switch (override_value.key) {
    case ThemeStyleOverrideKey::alpha:
        style.Alpha = override_value.x;
        break;
    case ThemeStyleOverrideKey::disabled_alpha:
        style.DisabledAlpha = override_value.x;
        break;
    case ThemeStyleOverrideKey::window_padding:
        style.WindowPadding = ImVec2(override_value.x, override_value.y);
        break;
    case ThemeStyleOverrideKey::window_rounding:
        style.WindowRounding = override_value.x;
        break;
    case ThemeStyleOverrideKey::window_border_size:
        style.WindowBorderSize = override_value.x;
        break;
    case ThemeStyleOverrideKey::window_min_size:
        style.WindowMinSize = ImVec2(override_value.x, override_value.y);
        break;
    case ThemeStyleOverrideKey::window_title_align:
        style.WindowTitleAlign = ImVec2(override_value.x, override_value.y);
        break;
    case ThemeStyleOverrideKey::window_menu_button_position:
        style.WindowMenuButtonPosition = static_cast<ImGuiDir>(static_cast<int>(override_value.x));
        break;
    case ThemeStyleOverrideKey::child_rounding:
        style.ChildRounding = override_value.x;
        break;
    case ThemeStyleOverrideKey::child_border_size:
        style.ChildBorderSize = override_value.x;
        break;
    case ThemeStyleOverrideKey::frame_rounding:
        style.FrameRounding = override_value.x;
        break;
    case ThemeStyleOverrideKey::popup_rounding:
        style.PopupRounding = override_value.x;
        break;
    case ThemeStyleOverrideKey::scrollbar_rounding:
        style.ScrollbarRounding = override_value.x;
        break;
    case ThemeStyleOverrideKey::grab_rounding:
        style.GrabRounding = override_value.x;
        break;
    case ThemeStyleOverrideKey::tab_rounding:
        style.TabRounding = override_value.x;
        break;
    case ThemeStyleOverrideKey::frame_border_size:
        style.FrameBorderSize = override_value.x;
        break;
    case ThemeStyleOverrideKey::popup_border_size:
        style.PopupBorderSize = override_value.x;
        break;
    case ThemeStyleOverrideKey::tab_border_size:
        style.TabBorderSize = override_value.x;
        break;
    case ThemeStyleOverrideKey::frame_padding:
        style.FramePadding = ImVec2(override_value.x, override_value.y);
        break;
    case ThemeStyleOverrideKey::item_spacing:
        style.ItemSpacing = ImVec2(override_value.x, override_value.y);
        break;
    case ThemeStyleOverrideKey::item_inner_spacing:
        style.ItemInnerSpacing = ImVec2(override_value.x, override_value.y);
        break;
    case ThemeStyleOverrideKey::cell_padding:
        style.CellPadding = ImVec2(override_value.x, override_value.y);
        break;
    case ThemeStyleOverrideKey::touch_extra_padding:
        style.TouchExtraPadding = ImVec2(override_value.x, override_value.y);
        break;
    case ThemeStyleOverrideKey::indent_spacing:
        style.IndentSpacing = override_value.x;
        break;
    case ThemeStyleOverrideKey::columns_min_spacing:
        style.ColumnsMinSpacing = override_value.x;
        break;
    case ThemeStyleOverrideKey::scrollbar_size:
        style.ScrollbarSize = override_value.x;
        break;
    case ThemeStyleOverrideKey::grab_min_size:
        style.GrabMinSize = override_value.x;
        break;
    case ThemeStyleOverrideKey::log_slider_deadzone:
        style.LogSliderDeadzone = override_value.x;
        break;
    case ThemeStyleOverrideKey::tab_min_width_for_close_button:
        style.TabMinWidthForCloseButton = override_value.x;
        break;
    case ThemeStyleOverrideKey::tab_bar_border_size:
        style.TabBarBorderSize = override_value.x;
        break;
    case ThemeStyleOverrideKey::tab_bar_overline_size:
        style.TabBarOverlineSize = override_value.x;
        break;
    case ThemeStyleOverrideKey::table_angled_headers_angle:
        style.TableAngledHeadersAngle = override_value.x;
        break;
    case ThemeStyleOverrideKey::table_angled_headers_text_align:
        style.TableAngledHeadersTextAlign = ImVec2(override_value.x, override_value.y);
        break;
    case ThemeStyleOverrideKey::color_button_position:
        style.ColorButtonPosition = static_cast<ImGuiDir>(static_cast<int>(override_value.x));
        break;
    case ThemeStyleOverrideKey::button_text_align:
        style.ButtonTextAlign = ImVec2(override_value.x, override_value.y);
        break;
    case ThemeStyleOverrideKey::selectable_text_align:
        style.SelectableTextAlign = ImVec2(override_value.x, override_value.y);
        break;
    case ThemeStyleOverrideKey::separator_text_border_size:
        style.SeparatorTextBorderSize = override_value.x;
        break;
    case ThemeStyleOverrideKey::separator_text_align:
        style.SeparatorTextAlign = ImVec2(override_value.x, override_value.y);
        break;
    case ThemeStyleOverrideKey::separator_text_padding:
        style.SeparatorTextPadding = ImVec2(override_value.x, override_value.y);
        break;
    case ThemeStyleOverrideKey::display_window_padding:
        style.DisplayWindowPadding = ImVec2(override_value.x, override_value.y);
        break;
    case ThemeStyleOverrideKey::display_safe_area_padding:
        style.DisplaySafeAreaPadding = ImVec2(override_value.x, override_value.y);
        break;
    case ThemeStyleOverrideKey::mouse_cursor_scale:
        style.MouseCursorScale = override_value.x;
        break;
    case ThemeStyleOverrideKey::anti_aliased_fill:
        style.AntiAliasedFill = override_value.x != 0.0f;
        break;
    case ThemeStyleOverrideKey::anti_aliased_lines:
        style.AntiAliasedLines = override_value.x != 0.0f;
        break;
    case ThemeStyleOverrideKey::anti_aliased_lines_use_tex:
        style.AntiAliasedLinesUseTex = override_value.x != 0.0f;
        break;
    case ThemeStyleOverrideKey::curve_tessellation_tol:
        style.CurveTessellationTol = override_value.x;
        break;
    case ThemeStyleOverrideKey::circle_tessellation_max_error:
        style.CircleTessellationMaxError = override_value.x;
        break;
    case ThemeStyleOverrideKey::hover_stationary_delay:
        style.HoverStationaryDelay = override_value.x;
        break;
    case ThemeStyleOverrideKey::hover_delay_short:
        style.HoverDelayShort = override_value.x;
        break;
    case ThemeStyleOverrideKey::hover_delay_normal:
        style.HoverDelayNormal = override_value.x;
        break;
    case ThemeStyleOverrideKey::hover_flags_for_tooltip_mouse:
        style.HoverFlagsForTooltipMouse = static_cast<ImGuiHoveredFlags>(static_cast<int>(override_value.x));
        break;
    case ThemeStyleOverrideKey::hover_flags_for_tooltip_nav:
        style.HoverFlagsForTooltipNav = static_cast<ImGuiHoveredFlags>(static_cast<int>(override_value.x));
        break;
    }
}

void append_float_override_if_changed(ThemeScheme& scheme,
                                      ThemeStyleOverrideKey key,
                                      float current,
                                      float reference)
{
    if (!nearly_equal(current, reference)) {
        (void)scheme.add_style_float_override(key, current);
    }
}

void append_vec2_override_if_changed(ThemeScheme& scheme,
                                     ThemeStyleOverrideKey key,
                                     const ImVec2& current,
                                     const ImVec2& reference)
{
    if (!nearly_equal(current, reference)) {
        (void)scheme.add_style_vec2_override(key, current.x, current.y);
    }
}

void append_bool_override_if_changed(ThemeScheme& scheme,
                                     ThemeStyleOverrideKey key,
                                     bool current,
                                     bool reference)
{
    if (current != reference) {
        (void)scheme.add_style_bool_override(key, current);
    }
}

void append_int_override_if_changed(ThemeScheme& scheme,
                                    ThemeStyleOverrideKey key,
                                    int current,
                                    int reference)
{
    if (current != reference) {
        (void)scheme.add_style_int_override(key, current);
    }
}

} // namespace

auto ThemeScheme::add_color_override(int color_index, float r, float g, float b, float a) -> bool
{
    if (color_index < 0 || color_index >= ImGuiCol_COUNT) {
        return false;
    }
    if (color_override_count >= static_cast<int>(color_overrides.size())) {
        return false;
    }

    color_overrides[color_override_count++] = { color_index, r, g, b, a };
    return true;
}

auto ThemeScheme::add_style_float_override(ThemeStyleOverrideKey key, float value) -> bool
{
    if (style_override_count >= static_cast<int>(style_overrides.size())) {
        return false;
    }

    style_overrides[style_override_count++] = { key, ThemeStyleOverrideKind::scalar, value, 0.0f };
    return true;
}

auto ThemeScheme::add_style_vec2_override(ThemeStyleOverrideKey key, float x, float y) -> bool
{
    if (style_override_count >= static_cast<int>(style_overrides.size())) {
        return false;
    }

    style_overrides[style_override_count++] = { key, ThemeStyleOverrideKind::vec2, x, y };
    return true;
}

auto ThemeScheme::add_style_bool_override(ThemeStyleOverrideKey key, bool value) -> bool
{
    if (style_override_count >= static_cast<int>(style_overrides.size())) {
        return false;
    }

    style_overrides[style_override_count++] = {
        key,
        ThemeStyleOverrideKind::boolean,
        value ? 1.0f : 0.0f,
        0.0f,
    };
    return true;
}

auto ThemeScheme::add_style_int_override(ThemeStyleOverrideKey key, int value) -> bool
{
    if (style_override_count >= static_cast<int>(style_overrides.size())) {
        return false;
    }

    style_overrides[style_override_count++] = {
        key,
        ThemeStyleOverrideKind::integer,
        static_cast<float>(value),
        0.0f,
    };
    return true;
}

auto ThemeCatalog::add_scheme(vk::string_view name,
                              ThemeBaseStyle base_style,
                              int clear_r,
                              int clear_g,
                              int clear_b) -> ThemeScheme*
{
    if (count >= static_cast<int>(schemes.size())) {
        return nullptr;
    }

    ThemeScheme& scheme = schemes[count++];
    scheme = ThemeScheme();
    scheme.name = string_from_view(name);
    scheme.base_style = base_style;
    scheme.clear_r = clear_r;
    scheme.clear_g = clear_g;
    scheme.clear_b = clear_b;
    return &scheme;
}

auto builtin_theme_catalog() -> ThemeCatalog
{
    ThemeCatalog catalog;
    const ImGuiStyle dark_base = make_base_style(ThemeBaseStyle::dark);
    const ImGuiStyle classic_base = make_base_style(ThemeBaseStyle::classic);

    add_default_renderer_tuning(catalog.add_scheme("Dark", ThemeBaseStyle::dark, 22, 22, 30));
    add_default_renderer_tuning(catalog.add_scheme("Light", ThemeBaseStyle::light, 22, 22, 30));
    add_default_renderer_tuning(catalog.add_scheme("Classic", ThemeBaseStyle::classic, 22, 22, 30));

    ThemeScheme* ocean = catalog.add_scheme("Ocean", ThemeBaseStyle::dark, 22, 22, 30);
    if (ocean != nullptr) {
        add_default_renderer_tuning(ocean);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_WindowBg, 0.05f, 0.08f, 0.12f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_ChildBg, 0.04f, 0.07f, 0.10f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_PopupBg, 0.06f, 0.09f, 0.14f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_TitleBg, 0.03f, 0.20f, 0.26f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_TitleBgActive, 0.04f, 0.28f, 0.35f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_Header, 0.08f, 0.33f, 0.46f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_HeaderHovered, 0.12f, 0.41f, 0.56f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_HeaderActive, 0.14f, 0.46f, 0.62f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_Button, 0.07f, 0.36f, 0.50f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_ButtonHovered, 0.11f, 0.45f, 0.61f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_ButtonActive, 0.14f, 0.50f, 0.68f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_FrameBg, 0.08f, 0.15f, 0.22f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_FrameBgHovered, 0.12f, 0.23f, 0.32f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_FrameBgActive, 0.13f, 0.27f, 0.38f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_SliderGrab, 0.24f, 0.64f, 0.82f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_SliderGrabActive, 0.30f, 0.74f, 0.92f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_CheckMark, 0.36f, 0.80f, 0.94f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_Separator, 0.18f, 0.34f, 0.44f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_ResizeGrip, 0.22f, 0.56f, 0.72f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_Tab, 0.07f, 0.24f, 0.34f);
        add_rgb_color_override(ocean, dark_base, ImGuiCol_TabActive, 0.10f, 0.35f, 0.49f);
    }

    ThemeScheme* forest = catalog.add_scheme("Forest", ThemeBaseStyle::dark, 22, 22, 30);
    if (forest != nullptr) {
        add_default_renderer_tuning(forest);
        add_rgb_color_override(forest, dark_base, ImGuiCol_WindowBg, 0.07f, 0.10f, 0.08f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_ChildBg, 0.05f, 0.08f, 0.06f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_PopupBg, 0.08f, 0.12f, 0.09f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_TitleBg, 0.12f, 0.22f, 0.15f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_TitleBgActive, 0.15f, 0.30f, 0.19f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_Header, 0.17f, 0.32f, 0.20f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_HeaderHovered, 0.22f, 0.40f, 0.25f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_HeaderActive, 0.25f, 0.45f, 0.27f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_Button, 0.16f, 0.36f, 0.22f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_ButtonHovered, 0.22f, 0.45f, 0.27f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_ButtonActive, 0.26f, 0.52f, 0.31f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_FrameBg, 0.11f, 0.17f, 0.12f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_FrameBgHovered, 0.16f, 0.25f, 0.17f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_FrameBgActive, 0.19f, 0.29f, 0.20f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_SliderGrab, 0.45f, 0.71f, 0.35f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_SliderGrabActive, 0.55f, 0.80f, 0.44f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_CheckMark, 0.63f, 0.86f, 0.45f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_Separator, 0.24f, 0.35f, 0.25f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_ResizeGrip, 0.33f, 0.55f, 0.36f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_Tab, 0.12f, 0.21f, 0.14f);
        add_rgb_color_override(forest, dark_base, ImGuiCol_TabActive, 0.21f, 0.36f, 0.24f);
    }

    ThemeScheme* sunset = catalog.add_scheme("Sunset", ThemeBaseStyle::dark, 22, 22, 30);
    if (sunset != nullptr) {
        add_default_renderer_tuning(sunset);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_WindowBg, 0.14f, 0.09f, 0.10f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_ChildBg, 0.12f, 0.07f, 0.08f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_PopupBg, 0.16f, 0.10f, 0.11f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_TitleBg, 0.31f, 0.15f, 0.12f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_TitleBgActive, 0.40f, 0.20f, 0.16f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_Header, 0.45f, 0.24f, 0.17f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_HeaderHovered, 0.53f, 0.30f, 0.20f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_HeaderActive, 0.59f, 0.35f, 0.22f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_Button, 0.47f, 0.24f, 0.15f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_ButtonHovered, 0.56f, 0.30f, 0.18f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_ButtonActive, 0.64f, 0.35f, 0.20f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_FrameBg, 0.20f, 0.12f, 0.11f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_FrameBgHovered, 0.29f, 0.17f, 0.14f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_FrameBgActive, 0.35f, 0.20f, 0.16f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_SliderGrab, 0.88f, 0.52f, 0.23f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_SliderGrabActive, 0.97f, 0.61f, 0.28f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_CheckMark, 0.99f, 0.73f, 0.35f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_Separator, 0.41f, 0.24f, 0.17f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_ResizeGrip, 0.67f, 0.37f, 0.21f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_Tab, 0.24f, 0.13f, 0.11f);
        add_rgb_color_override(sunset, dark_base, ImGuiCol_TabActive, 0.40f, 0.22f, 0.15f);
    }

    ThemeScheme* win9x = catalog.add_scheme("Win9x", ThemeBaseStyle::classic, 0x38, 0x6e, 0xa5);
    if (win9x != nullptr) {
        add_hex_color_override(win9x, classic_base, ImGuiCol_Text, "#000000");
        add_hex_color_override(win9x, classic_base, ImGuiCol_TextDisabled, "#7F7F7F");
        add_hex_color_override(win9x, classic_base, ImGuiCol_WindowBg, "#d4d0c8");
        add_hex_color_override(win9x, classic_base, ImGuiCol_ChildBg, "#FFFFFF");
        add_hex_color_override(win9x, classic_base, ImGuiCol_PopupBg, "#d4d0c8");
        add_hex_color_override(win9x, classic_base, ImGuiCol_Border, "#4F4F4F");
        add_hex_color_override(win9x, classic_base, ImGuiCol_BorderShadow, "#FFFFFF");
        add_hex_color_override(win9x, classic_base, ImGuiCol_FrameBg, "#FFFFFF");
        add_hex_color_override(win9x, classic_base, ImGuiCol_FrameBgHovered, "#DCDCDC");
        add_hex_color_override(win9x, classic_base, ImGuiCol_FrameBgActive, "#A8A8A8");
        add_hex_color_override(win9x, classic_base, ImGuiCol_TitleBg, "#4040A0");
        add_hex_color_override(win9x, classic_base, ImGuiCol_TitleBgActive, "#0A246A");
        add_hex_color_override(win9x, classic_base, ImGuiCol_MenuBarBg, "#d4d0c8");
        add_hex_color_override(win9x, classic_base, ImGuiCol_ScrollbarBg, "#d4d0c8");
        add_hex_color_override(win9x, classic_base, ImGuiCol_ScrollbarGrab, "#AEAEAE");
        add_hex_color_override(win9x, classic_base, ImGuiCol_ScrollbarGrabHovered, "#949494");
        add_hex_color_override(win9x, classic_base, ImGuiCol_ScrollbarGrabActive, "#737373");
        add_hex_color_override(win9x, classic_base, ImGuiCol_CheckMark, "#000000");
        add_hex_color_override(win9x, classic_base, ImGuiCol_SliderGrab, "#A0A0A0");
        add_hex_color_override(win9x, classic_base, ImGuiCol_SliderGrabActive, "#737373");
        add_hex_color_override(win9x, classic_base, ImGuiCol_Button, "#d4d0c8");
        add_hex_color_override(win9x, classic_base, ImGuiCol_ButtonHovered, "#DCDCDC");
        add_hex_color_override(win9x, classic_base, ImGuiCol_ButtonActive, "#A8A8A8");
        add_hex_color_override(win9x, classic_base, ImGuiCol_TableRowBg, "#FFFFFF");
        add_hex_color_override(win9x, classic_base, ImGuiCol_Header, "#C6D2EA");
        add_hex_color_override(win9x, classic_base, ImGuiCol_HeaderHovered, "#ACCAE6");
        add_hex_color_override(win9x, classic_base, ImGuiCol_HeaderActive, "#8EADDF");
        add_hex_color_override(win9x, classic_base, ImGuiCol_Separator, "#606060");
        add_hex_color_override(win9x, classic_base, ImGuiCol_ResizeGrip, "#A0A0A0");
        add_hex_color_override(win9x, classic_base, ImGuiCol_ResizeGripHovered, "#808080");
        add_hex_color_override(win9x, classic_base, ImGuiCol_ResizeGripActive, "#5A5A5A");
        add_hex_color_override(win9x, classic_base, ImGuiCol_Tab, "#d4d0c8");
        add_hex_color_override(win9x, classic_base, ImGuiCol_TabHovered, "#DCDCDC");
        add_hex_color_override(win9x, classic_base, ImGuiCol_TabActive, "#A8A8A8");

        (void)win9x->add_style_float_override(ThemeStyleOverrideKey::window_rounding, 0.0f);
        (void)win9x->add_style_float_override(ThemeStyleOverrideKey::child_rounding, 0.0f);
        (void)win9x->add_style_float_override(ThemeStyleOverrideKey::frame_rounding, 0.0f);
        (void)win9x->add_style_float_override(ThemeStyleOverrideKey::popup_rounding, 0.0f);
        (void)win9x->add_style_float_override(ThemeStyleOverrideKey::scrollbar_rounding, 0.0f);
        (void)win9x->add_style_float_override(ThemeStyleOverrideKey::grab_rounding, 0.0f);
        (void)win9x->add_style_float_override(ThemeStyleOverrideKey::tab_rounding, 0.0f);
        (void)win9x->add_style_float_override(ThemeStyleOverrideKey::window_border_size, 1.0f);
        (void)win9x->add_style_float_override(ThemeStyleOverrideKey::frame_border_size, 1.0f);
        (void)win9x->add_style_float_override(ThemeStyleOverrideKey::popup_border_size, 1.0f);
        (void)win9x->add_style_float_override(ThemeStyleOverrideKey::tab_border_size, 1.0f);
        (void)win9x->add_style_vec2_override(ThemeStyleOverrideKey::frame_padding, 5.0f, 3.0f);
        (void)win9x->add_style_float_override(ThemeStyleOverrideKey::scrollbar_size, 20.0f);
        (void)win9x->add_style_float_override(ThemeStyleOverrideKey::grab_min_size, 15.0f);
        (void)win9x->add_style_bool_override(ThemeStyleOverrideKey::anti_aliased_fill, true);
        (void)win9x->add_style_bool_override(ThemeStyleOverrideKey::anti_aliased_lines, true);
        (void)win9x->add_style_float_override(ThemeStyleOverrideKey::separator_text_border_size, 1.0f);
    }

    return catalog;
}

auto theme_base_style_name(ThemeBaseStyle base_style) -> const char*
{
    switch (base_style) {
    case ThemeBaseStyle::dark:
        return "dark";
    case ThemeBaseStyle::light:
        return "light";
    case ThemeBaseStyle::classic:
        return "classic";
    }

    return "dark";
}

auto parse_theme_base_style(vk::string_view text, ThemeBaseStyle& base_style) -> bool
{
    if (string_equals(text, "dark")) {
        base_style = ThemeBaseStyle::dark;
        return true;
    }
    if (string_equals(text, "light")) {
        base_style = ThemeBaseStyle::light;
        return true;
    }
    if (string_equals(text, "classic")) {
        base_style = ThemeBaseStyle::classic;
        return true;
    }
    return false;
}

auto theme_color_key_for_index(int color_index) -> const char*
{
    if (color_index < 0 || color_index >= ImGuiCol_COUNT) {
        return nullptr;
    }
    return ImGui::GetStyleColorName(color_index);
}

auto theme_color_index_for_key(vk::string_view key) -> int
{
    for (int index = 0; index < ImGuiCol_COUNT; ++index) {
        const char* color_name = ImGui::GetStyleColorName(index);
        if (color_name != nullptr && string_equals(key, color_name)) {
            return index;
        }
    }
    return -1;
}

auto theme_style_key_name(ThemeStyleOverrideKey key) -> const char*
{
    for (const ThemeStyleKeyEntry& entry : k_theme_style_keys) {
        if (entry.key == key) {
            return entry.name;
        }
    }
    return nullptr;
}

auto parse_theme_style_key(vk::string_view text, ThemeStyleOverrideKey& key) -> bool
{
    for (const ThemeStyleKeyEntry& entry : k_theme_style_keys) {
        if (string_equals(text, entry.name)) {
            key = entry.key;
            return true;
        }
    }
    return false;
}

auto theme_scheme_from_style(vk::string_view name,
                             ThemeBaseStyle base_style,
                             int clear_r,
                             int clear_g,
                             int clear_b,
                             const ImGuiStyle& style) -> ThemeScheme
{
    ThemeScheme scheme;
    scheme.name = string_from_view(name);
    scheme.base_style = base_style;
    scheme.clear_r = clear_r;
    scheme.clear_g = clear_g;
    scheme.clear_b = clear_b;

    const ImGuiStyle base = make_base_style(base_style);
    for (int index = 0; index < ImGuiCol_COUNT; ++index) {
        if (!nearly_equal(style.Colors[index], base.Colors[index])) {
            const ThemeColorOverride color = to_theme_color(style.Colors[index]);
            (void)scheme.add_color_override(index, color.r, color.g, color.b, color.a);
        }
    }

#define APPEND_FLOAT(key_name, field_name) \
    append_float_override_if_changed(scheme, ThemeStyleOverrideKey::key_name, style.field_name, base.field_name)
#define APPEND_VEC2(key_name, field_name) \
    append_vec2_override_if_changed(scheme, ThemeStyleOverrideKey::key_name, style.field_name, base.field_name)
#define APPEND_BOOL(key_name, field_name) \
    append_bool_override_if_changed(scheme, ThemeStyleOverrideKey::key_name, style.field_name, base.field_name)
#define APPEND_INT(key_name, field_name) \
    append_int_override_if_changed(scheme, ThemeStyleOverrideKey::key_name, static_cast<int>(style.field_name), static_cast<int>(base.field_name))

    APPEND_FLOAT(alpha, Alpha);
    APPEND_FLOAT(disabled_alpha, DisabledAlpha);
    APPEND_VEC2(window_padding, WindowPadding);
    APPEND_FLOAT(window_rounding, WindowRounding);
    APPEND_FLOAT(window_border_size, WindowBorderSize);
    APPEND_VEC2(window_min_size, WindowMinSize);
    APPEND_VEC2(window_title_align, WindowTitleAlign);
    APPEND_INT(window_menu_button_position, WindowMenuButtonPosition);
    APPEND_FLOAT(child_rounding, ChildRounding);
    APPEND_FLOAT(child_border_size, ChildBorderSize);
    APPEND_FLOAT(popup_rounding, PopupRounding);
    APPEND_FLOAT(popup_border_size, PopupBorderSize);
    APPEND_VEC2(frame_padding, FramePadding);
    APPEND_FLOAT(frame_rounding, FrameRounding);
    APPEND_FLOAT(frame_border_size, FrameBorderSize);
    APPEND_VEC2(item_spacing, ItemSpacing);
    APPEND_VEC2(item_inner_spacing, ItemInnerSpacing);
    APPEND_VEC2(cell_padding, CellPadding);
    APPEND_VEC2(touch_extra_padding, TouchExtraPadding);
    APPEND_FLOAT(indent_spacing, IndentSpacing);
    APPEND_FLOAT(columns_min_spacing, ColumnsMinSpacing);
    APPEND_FLOAT(scrollbar_size, ScrollbarSize);
    APPEND_FLOAT(scrollbar_rounding, ScrollbarRounding);
    APPEND_FLOAT(grab_min_size, GrabMinSize);
    APPEND_FLOAT(grab_rounding, GrabRounding);
    APPEND_FLOAT(log_slider_deadzone, LogSliderDeadzone);
    APPEND_FLOAT(tab_rounding, TabRounding);
    APPEND_FLOAT(tab_border_size, TabBorderSize);
    APPEND_FLOAT(tab_min_width_for_close_button, TabMinWidthForCloseButton);
    APPEND_FLOAT(tab_bar_border_size, TabBarBorderSize);
    APPEND_FLOAT(tab_bar_overline_size, TabBarOverlineSize);
    APPEND_FLOAT(table_angled_headers_angle, TableAngledHeadersAngle);
    APPEND_VEC2(table_angled_headers_text_align, TableAngledHeadersTextAlign);
    APPEND_INT(color_button_position, ColorButtonPosition);
    APPEND_VEC2(button_text_align, ButtonTextAlign);
    APPEND_VEC2(selectable_text_align, SelectableTextAlign);
    APPEND_FLOAT(separator_text_border_size, SeparatorTextBorderSize);
    APPEND_VEC2(separator_text_align, SeparatorTextAlign);
    APPEND_VEC2(separator_text_padding, SeparatorTextPadding);
    APPEND_VEC2(display_window_padding, DisplayWindowPadding);
    APPEND_VEC2(display_safe_area_padding, DisplaySafeAreaPadding);
    APPEND_FLOAT(mouse_cursor_scale, MouseCursorScale);
    APPEND_BOOL(anti_aliased_fill, AntiAliasedFill);
    APPEND_BOOL(anti_aliased_lines, AntiAliasedLines);
    APPEND_BOOL(anti_aliased_lines_use_tex, AntiAliasedLinesUseTex);
    APPEND_FLOAT(curve_tessellation_tol, CurveTessellationTol);
    APPEND_FLOAT(circle_tessellation_max_error, CircleTessellationMaxError);
    APPEND_FLOAT(hover_stationary_delay, HoverStationaryDelay);
    APPEND_FLOAT(hover_delay_short, HoverDelayShort);
    APPEND_FLOAT(hover_delay_normal, HoverDelayNormal);
    APPEND_INT(hover_flags_for_tooltip_mouse, HoverFlagsForTooltipMouse);
    APPEND_INT(hover_flags_for_tooltip_nav, HoverFlagsForTooltipNav);

#undef APPEND_FLOAT
#undef APPEND_VEC2
#undef APPEND_BOOL
#undef APPEND_INT

    return scheme;
}

void apply_theme_scheme(const ThemeScheme& scheme)
{
    apply_base_style(scheme.base_style, nullptr);

    ::ImGui_ImplVK_SetClearColor(static_cast<unsigned int>(scheme.clear_r),
                                 static_cast<unsigned int>(scheme.clear_g),
                                 static_cast<unsigned int>(scheme.clear_b));

    ImGuiStyle& style = ImGui::GetStyle();
    for (int index = 0; index < scheme.color_override_count; ++index) {
        const ThemeColorOverride& color = scheme.color_overrides[index];
        if (color.color_index < 0 || color.color_index >= ImGuiCol_COUNT) {
            continue;
        }
        style.Colors[color.color_index] = ImVec4(color.r, color.g, color.b, color.a);
    }

    for (int index = 0; index < scheme.style_override_count; ++index) {
        apply_style_override(style, scheme.style_overrides[index]);
    }
}

} // namespace vkgui