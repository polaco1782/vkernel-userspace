#include "../plugin_api.h"

namespace {

struct runtime_demo_state {
    vk_u32 button_presses = 0;
};

runtime_demo_state g_state {};

void runtime_demo_shutdown(void* /*user_data*/)
{
}

void runtime_demo_draw(const vkgui_plugin_host_api_t* host, vk_u32* visible, void* user_data)
{
    if (host == nullptr || host->abi_version != VKGUI_PLUGIN_HOST_API_VERSION || visible == nullptr) {
        return;
    }

    auto* state = static_cast<runtime_demo_state*>(user_data);
    host->set_next_window_pos_first_use(360.0f, 60.0f);
    host->set_next_window_size_first_use(320.0f, 180.0f);

    if (!host->begin_window("Runtime Plugin Demo", visible)) {
        host->end_window();
        return;
    }

    host->clear_focus_if_host_window_focused();
    host->separator_text("Dynamic Library");
    host->text("Loaded from a separate ET_DYN plugin.");
    host->textf("Kernel ABI version: %llu",
                static_cast<unsigned long long>(host->vk_api->api_version));
    host->textf("Ticks: %llu",
                static_cast<unsigned long long>(host->vk_api->vk_tick_count()));
    host->spacing();

    if (host->button("Ping Log", 110.0f, 0.0f)) {
        ++state->button_presses;
        host->logf("Runtime demo plugin clicked %u time(s).",
                   static_cast<unsigned>(state->button_presses));
    }
    host->same_line();
    host->textf("Clicks: %u", static_cast<unsigned>(state->button_presses));

    host->end_window();
}

} // namespace

extern "C" __attribute__((visibility("default")))
int vkgui_plugin_init(const vkgui_plugin_host_api_t* host,
                     vkgui_plugin_descriptor_t* out_descriptor)
{
    if (host == nullptr
        || host->abi_version != VKGUI_PLUGIN_HOST_API_VERSION
        || host->vk_api == nullptr
        || out_descriptor == nullptr) {
        return 0;
    }

    out_descriptor->abi_version = VKGUI_PLUGIN_ABI_VERSION;
    out_descriptor->id = "runtime_demo";
    out_descriptor->menu_label = "Runtime Demo";
    out_descriptor->default_visible = 0u;
    out_descriptor->user_data = &g_state;
    out_descriptor->shutdown = runtime_demo_shutdown;
    out_descriptor->draw_window = runtime_demo_draw;
    return 1;
}
