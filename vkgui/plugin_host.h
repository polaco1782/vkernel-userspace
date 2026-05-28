#ifndef VKGUI_PLUGIN_HOST_H
#define VKGUI_PLUGIN_HOST_H

#include "vkgui_common.h"

namespace vkgui {

class ConsoleLog;
class LaunchRegistry;
class WindowManager;

struct PluginHost {
    const vk_framebuffer_info_t& framebuffer;
    LaunchRegistry& launch_registry;
    WindowManager& window_manager;
    ConsoleLog& log;
};

} // namespace vkgui

#endif // VKGUI_PLUGIN_HOST_H
