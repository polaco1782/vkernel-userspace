#ifndef VKGUI_TEXT_EDITOR_PANEL_H
#define VKGUI_TEXT_EDITOR_PANEL_H

#include "vkgui_common.h"

#include "TextEditor.h"

namespace vkgui {

class ConsoleLog;
class WindowManager;

class TextEditorPanel {
public:
    TextEditorPanel();

    void draw_window(bool& visible, WindowManager& window_manager, ConsoleLog& log);

private:
    void apply_theme_palette();
    void set_language_for_path(vk::string_view path);
    auto load_from_path(vk::string_view path, ConsoleLog& log) -> bool;
    auto save_to_path(vk::string_view path, ConsoleLog& log) -> bool;

    TextEditor editor_;
    TextEditor::Palette palette_ {};
    std::string path_input_ = "/data/vkgui/notes.txt";
    std::string status_ = "Ready.";
    bool dirty_ = false;
    bool read_only_ = false;
    bool show_whitespace_ = false;
};

} // namespace vkgui

#endif // VKGUI_TEXT_EDITOR_PANEL_H
