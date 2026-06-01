#include "frontend.h"

#include <iostream>

int main(int argc, char** argv)
{
    using namespace vspcplay_frontend;

    AppState app {};
    if (!parse_args(&app, argc, argv))
        return 0;

    if (app.config.show_id666) {
        print_id666_info(app);
        return 0;
    }

    if (!init_app(&app))
        return 1;

    if (!app.playlist.empty()) {
        if (!load_current_track(&app)) {
            destroy_app(&app);
            return 1;
        }
    } else if (!app.config.novideo) {
        browser_open(&app);
    } else {
        std::cout << "vspcplay: provide at least one .spc file when running without video\n";
        destroy_app(&app);
        return 1;
    }

    while (!app.quit_requested) {
        pump_input(&app);
        process_requests(&app);
        update_playback(&app);
        render(&app);
        idle_until_next_work(&app);
    }

    destroy_app(&app);
    return 0;
}
