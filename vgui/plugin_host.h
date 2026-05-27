#ifndef VGUI_PLUGIN_HOST_H
#define VGUI_PLUGIN_HOST_H

#include "vgui_common.h"

namespace vgui {

class ConsoleLog;
class LaunchRegistry;
class WindowManager;

struct PluginHost {
    const vk_framebuffer_info_t& framebuffer;
    LaunchRegistry& launch_registry;
    WindowManager& window_manager;
    ConsoleLog& log;
};

} // namespace vgui

#endif // VGUI_PLUGIN_HOST_H
