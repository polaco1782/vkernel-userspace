#include "frontend.h"

#include <iostream>

int main(int argc, char** argv)
{
    using namespace snes9x_frontend;

    AppState app {};

    if (!init_framebuffer(&app))
        return 1;

    if (!init_emulator(&app)) {
        destroy_app(&app);
        return 1;
    }

    bool loaded = false;
    if (argc > 1)
        loaded = load_rom(&app, argv[1]);

    if (!loaded && !browse_and_load_rom(&app)) {
        const int exit_code = app.quit_requested ? 0 : 1;
        destroy_app(&app);
        return exit_code;
    }

    std::cout << "Snes9x\n";
    if (!app.loaded_rom_path.empty())
        std::cout << "ROM file: " << path_basename(app.loaded_rom_path.c_str()) << '\n';
    if (!app.rom_title.empty())
        std::cout << "Loaded: " << app.rom_title << '\n';
    std::cout << "Controls: arrows, A/S/Q/W/D/E, Enter, Space, Tab=load ROM, Backspace=reset, Escape=quit\n";

    while (!app.quit_requested) {
        pump_input(&app);

        if (app.reset_requested) {
            app.reset_requested = false;
            reset_emulator(&app);
            continue;
        }

        if (app.loadrom_requested) {
            app.loadrom_requested = false;
            if (browse_and_load_rom(&app))
                continue;
            if (app.quit_requested)
                break;
        }

        audio_try_submit(&app);

        vk_u64 now = VK_CALL(tick_count);
        vk_u32 catchup_frames = 0;
        while (app.rom_loaded && now >= app.timing.next_frame_tick && catchup_frames < kMaxCatchupFrames) {
            S9xMainLoop();
            audio_try_submit(&app);
            schedule_next_frame(&app);
            now = VK_CALL(tick_count);
            ++catchup_frames;
        }

        idle_until_next_work(&app);
    }

    destroy_app(&app);
    return 0;
}
