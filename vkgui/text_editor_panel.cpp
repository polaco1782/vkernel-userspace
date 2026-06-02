#include "text_editor_panel.h"

#include "console_log.h"
#include "window_manager.h"

namespace vkgui {

namespace {

auto color_u32(const ImVec4& color) -> ImU32
{
    return ImGui::ColorConvertFloat4ToU32(color);
}

void set_palette_color(TextEditor::Palette& palette, TextEditor::Color slot, const ImVec4& color)
{
    palette[static_cast<size_t>(slot)] = color_u32(color);
}

auto detect_language_for_path(vk::string_view path) -> const TextEditor::Language*
{
    if (ends_with(path, ".c")
        || ends_with(path, ".cc")
        || ends_with(path, ".cpp")
        || ends_with(path, ".cxx")
        || ends_with(path, ".h")
        || ends_with(path, ".hh")
        || ends_with(path, ".hpp")
        || ends_with(path, ".hxx")
        || ends_with(path, ".qc")) {
        return TextEditor::Language::Cpp();
    }
    if (ends_with(path, ".lua")) {
        return TextEditor::Language::Lua();
    }
    if (ends_with(path, ".py")) {
        return TextEditor::Language::Python();
    }
    if (ends_with(path, ".glsl")
        || ends_with(path, ".vert")
        || ends_with(path, ".frag")
        || ends_with(path, ".geom")
        || ends_with(path, ".comp")) {
        return TextEditor::Language::Glsl();
    }
    if (ends_with(path, ".json")) {
        return TextEditor::Language::Json();
    }
    if (ends_with(path, ".md")) {
        return TextEditor::Language::Markdown();
    }
    if (ends_with(path, ".sql")) {
        return TextEditor::Language::Sql();
    }
    return nullptr;
}

} // namespace

TextEditorPanel::TextEditorPanel()
{
    editor_.SetAutoIndentEnabled(true);
    editor_.SetCompletePairedGlyphs(true);
    editor_.SetShowMatchingBrackets(true);
    editor_.SetShowLineNumbersEnabled(true);
    editor_.SetShowScrollbarMiniMapEnabled(false);
    editor_.SetChangeCallback([this]() { dirty_ = true; });
    set_language_for_path(string_view_of(path_input_));
}

void TextEditorPanel::apply_theme_palette()
{
    const ImGuiStyle& style = ImGui::GetStyle();

    set_palette_color(palette_, TextEditor::Color::text, style.Colors[ImGuiCol_Text]);
    set_palette_color(palette_, TextEditor::Color::keyword, style.Colors[ImGuiCol_CheckMark]);
    set_palette_color(palette_, TextEditor::Color::declaration, style.Colors[ImGuiCol_HeaderActive]);
    set_palette_color(palette_, TextEditor::Color::number, style.Colors[ImGuiCol_SliderGrabActive]);
    set_palette_color(palette_, TextEditor::Color::string, style.Colors[ImGuiCol_PlotLines]);
    set_palette_color(palette_, TextEditor::Color::punctuation, style.Colors[ImGuiCol_Text]);
    set_palette_color(palette_, TextEditor::Color::preprocessor, style.Colors[ImGuiCol_ButtonHovered]);
    set_palette_color(palette_, TextEditor::Color::identifier, style.Colors[ImGuiCol_Text]);
    set_palette_color(palette_, TextEditor::Color::knownIdentifier, style.Colors[ImGuiCol_ButtonActive]);
    set_palette_color(palette_, TextEditor::Color::comment, style.Colors[ImGuiCol_TextDisabled]);
    set_palette_color(palette_, TextEditor::Color::background, style.Colors[ImGuiCol_ChildBg]);
    set_palette_color(palette_, TextEditor::Color::cursor, style.Colors[ImGuiCol_Text]);
    set_palette_color(palette_, TextEditor::Color::selection, style.Colors[ImGuiCol_Header]);
    set_palette_color(palette_, TextEditor::Color::whitespace, style.Colors[ImGuiCol_TextDisabled]);
    set_palette_color(palette_, TextEditor::Color::matchingBracketBackground, style.Colors[ImGuiCol_FrameBgHovered]);
    set_palette_color(palette_, TextEditor::Color::matchingBracketActive, style.Colors[ImGuiCol_CheckMark]);
    set_palette_color(palette_, TextEditor::Color::matchingBracketLevel1, style.Colors[ImGuiCol_ButtonHovered]);
    set_palette_color(palette_, TextEditor::Color::matchingBracketLevel2, style.Colors[ImGuiCol_HeaderHovered]);
    set_palette_color(palette_, TextEditor::Color::matchingBracketLevel3, style.Colors[ImGuiCol_SliderGrab]);
    set_palette_color(palette_, TextEditor::Color::matchingBracketError, style.Colors[ImGuiCol_PlotHistogram]);
    set_palette_color(palette_, TextEditor::Color::lineNumber, style.Colors[ImGuiCol_TextDisabled]);
    set_palette_color(palette_, TextEditor::Color::currentLineNumber, style.Colors[ImGuiCol_Text]);
    editor_.SetPalette(palette_);
}

void TextEditorPanel::set_language_for_path(vk::string_view path)
{
    editor_.SetLanguage(detect_language_for_path(path));
}

auto TextEditorPanel::load_from_path(vk::string_view path, ConsoleLog& log) -> bool
{
    std::string file_bytes;
    if (!read_file_bytes(path, file_bytes)) {
        status_ = "Open failed.";
        log.addf("Text editor: failed to open %.*s.",
                 static_cast<int>(path.size()),
                 path.data());
        return false;
    }

    editor_.SetText(file_bytes);
    editor_.SetFocus();
    set_language_for_path(path);
    dirty_ = false;
    status_ = "Opened file.";
    log.addf("Text editor: opened %.*s (%zu byte%s).",
             static_cast<int>(path.size()),
             path.data(),
             file_bytes.size(),
             file_bytes.size() == 1 ? "" : "s");
    return true;
}

auto TextEditorPanel::save_to_path(vk::string_view path, ConsoleLog& log) -> bool
{
    const std::string text = editor_.GetText();
    if (!write_file_bytes(path, string_view_of(text))) {
        status_ = "Save failed.";
        log.addf("Text editor: failed to save %.*s.",
                 static_cast<int>(path.size()),
                 path.data());
        return false;
    }

    dirty_ = false;
    set_language_for_path(path);
    status_ = "Saved file.";
    log.addf("Text editor: saved %.*s (%zu byte%s).",
             static_cast<int>(path.size()),
             path.data(),
             text.size(),
             text.size() == 1 ? "" : "s");
    return true;
}

void TextEditorPanel::draw_window(bool& visible, WindowManager& window_manager, ConsoleLog& log)
{
    if (!visible) {
        return;
    }

    apply_theme_palette();
    editor_.SetReadOnlyEnabled(read_only_);
    editor_.SetShowWhitespacesEnabled(show_whitespace_);

    std::string title = dirty_ ? "Text Editor*" : "Text Editor";
    ImGui::SetNextWindowPos(ImVec2(280.0f, 30.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(760.0f, 560.0f), ImGuiCond_FirstUseEver);

    if (!imgui_begin_window_readable_caption(title.c_str(), &visible)) {
        ImGui::End();
        return;
    }
    window_manager.clear_focus_if_host_window_focused();

    ImGui::SetNextItemWidth(-1.0f);
    imgui_input_text("Path", path_input_);

    if (ImGui::Button("New")) {
        editor_.ClearText();
        editor_.SetFocus();
        dirty_ = false;
        status_ = "New buffer.";
        set_language_for_path(string_view_of(path_input_));
        log.add("Text editor: created a new buffer.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Open")) {
        (void)load_from_path(string_view_of(path_input_), log);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        (void)load_from_path(string_view_of(path_input_), log);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        (void)save_to_path(string_view_of(path_input_), log);
    }

    ImGui::SameLine();
    ImGui::Checkbox("Read Only", &read_only_);
    ImGui::SameLine();
    ImGui::Checkbox("Whitespace", &show_whitespace_);

    const std::string language_name = editor_.HasLanguage() ? editor_.GetLanguageName() : "Plain Text";
    const int line_count = editor_.GetLineCount();
    ImGui::TextDisabled("%s | %s | %d line%s",
                        status_.c_str(),
                        language_name.c_str(),
                        line_count,
                        line_count == 1 ? "" : "s");

    editor_.Render("##text_editor_surface", ImVec2(-1.0f, -1.0f), true);
    ImGui::End();
}

} // namespace vkgui
