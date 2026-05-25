#include "frontend.h"

#include "../snes9x-src/cheats.h"
#include "../snes9x-src/movie.h"
#include "../snes9x-src/snapshot.h"
#include "../snes9x-src/statemanager.h"

#include <iostream>

namespace snes9x_frontend {

namespace {

AppState* g_app = nullptr;
constexpr int kAudioChannel = 0;

void blit_scaled_image(AppState* app,
                       const vk_u32* source_pixels,
                       vk_u32 source_width,
                       vk_u32 source_height,
                       vk_u32 source_stride)
{
    if (app == nullptr || source_pixels == nullptr || source_width == 0 || source_height == 0)
        return;

    vk_u32 destination_width = 0;
    vk_u32 destination_height = 0;

    if (static_cast<vk_u64>(app->framebuffer.width) * source_height <=
        static_cast<vk_u64>(app->framebuffer.height) * source_width) {
        destination_width = app->framebuffer.width;
        destination_height = static_cast<vk_u32>(
            (static_cast<vk_u64>(app->framebuffer.width) * source_height) / source_width);
    } else {
        destination_height = app->framebuffer.height;
        destination_width = static_cast<vk_u32>(
            (static_cast<vk_u64>(app->framebuffer.height) * source_width) / source_height);
    }

    if (destination_width == 0)
        destination_width = 1;
    if (destination_height == 0)
        destination_height = 1;

    const vk_u32 x_offset = (app->framebuffer.width - destination_width) / 2u;
    const vk_u32 y_offset = (app->framebuffer.height - destination_height) / 2u;

    for (vk_u32 y = 0; y < destination_height; ++y) {
        const vk_u32 source_y = min_u32(source_height - 1u,
            static_cast<vk_u32>((static_cast<vk_u64>(y) * source_height) / destination_height));
        const vk_u32* source_row = &source_pixels[static_cast<vk_usize>(source_y) * source_stride];
        vk_u32* destination_row =
            &app->present_buffer[(static_cast<vk_usize>(y_offset + y) * app->framebuffer.stride) + x_offset];

        for (vk_u32 x = 0; x < destination_width; ++x) {
            const vk_u32 source_x = min_u32(source_width - 1u,
                static_cast<vk_u32>((static_cast<vk_u64>(x) * source_width) / destination_width));
            destination_row[x] = source_row[source_x];
        }
    }
}

bool refresh_framebuffer_state(AppState* app, bool* changed)
{
    if (app == nullptr)
        return false;

    vk_framebuffer_info_t framebuffer = {};
    VK_CALL(framebuffer_info, &framebuffer);
    if (!framebuffer.valid || framebuffer.base == 0 || framebuffer.width == 0 || framebuffer.height == 0)
        return false;

    const bool framebuffer_changed = app->framebuffer.base != framebuffer.base
                                  || app->framebuffer.width != framebuffer.width
                                  || app->framebuffer.height != framebuffer.height
                                  || app->framebuffer.stride != framebuffer.stride
                                  || app->framebuffer.format != framebuffer.format;

    app->framebuffer = framebuffer;
    if (framebuffer_changed) {
        const vk_usize pixels =
            static_cast<vk_usize>(app->framebuffer.stride) * app->framebuffer.height;
        app->present_buffer.assign(pixels, 0u);
    }

    if (changed != nullptr)
        *changed = framebuffer_changed;

    return true;
}

void prime_audio(AppState* app)
{
    int guard = 0;
    while (app->rom_loaded
           && !app->quit_requested
           && app->audio.queue_count < (kQueuePrimeFrames * 2u)
           && guard < 8) {
        S9xMainLoop();
        ++guard;
    }
    audio_try_submit(app);
}

void configure_defaults()
{
    memset(&Settings, 0, sizeof(Settings));
    Settings.MouseMaster = FALSE;
    Settings.SuperScopeMaster = FALSE;
    Settings.JustifierMaster = FALSE;
    Settings.MultiPlayer5Master = FALSE;
    Settings.MacsRifleMaster = FALSE;
    Settings.FrameTimePAL = 20000;
    Settings.FrameTimeNTSC = 16639;
    Settings.FrameTime = Settings.FrameTimeNTSC;
    Settings.SixteenBitSound = TRUE;
    Settings.Stereo = TRUE;
    Settings.SoundPlaybackRate = kOutputSampleRate;
    Settings.SoundInputRate = 32040;
    Settings.Transparency = TRUE;
    Settings.AutoDisplayMessages = TRUE;
    Settings.InitialInfoStringTimeout = 120;
    Settings.HDMATimingHack = 100;
    Settings.BlockInvalidVRAMAccessMaster = TRUE;
    Settings.BlockInvalidVRAMAccess = TRUE;
    Settings.SeparateEchoBuffer = FALSE;
    Settings.AutoSaveDelay = 1;
    Settings.DontSaveOopsSnapshot = TRUE;
    Settings.SoundSync = FALSE;
    Settings.InterpolationMethod = DSP_INTERPOLATION_GAUSSIAN;
    Settings.SuperFXClockMultiplier = 100;
    Settings.MaxSpriteTilesPerLine = 34;
    Settings.OneClockCycle = 6;
    Settings.OneSlowClockCycle = 8;
    Settings.TwoClockCycles = 12;
    Settings.UpAndDown = FALSE;
    CPU.Flags = 0;
}

void map_controls()
{
    S9xInitInputDevices();
    S9xUnmapAllControls();
    S9xSetController(0, CTL_JOYPAD, 0, 0, 0, 0);
    S9xSetController(1, CTL_NONE, 0, 0, 0, 0);

    S9xMapButton(kButtonB, S9xGetCommandT("Joypad1 B"), false);
    S9xMapButton(kButtonA, S9xGetCommandT("Joypad1 A"), false);
    S9xMapButton(kButtonY, S9xGetCommandT("Joypad1 Y"), false);
    S9xMapButton(kButtonX, S9xGetCommandT("Joypad1 X"), false);
    S9xMapButton(kButtonL, S9xGetCommandT("Joypad1 L"), false);
    S9xMapButton(kButtonR, S9xGetCommandT("Joypad1 R"), false);
    S9xMapButton(kButtonStart, S9xGetCommandT("Joypad1 Start"), false);
    S9xMapButton(kButtonSelect, S9xGetCommandT("Joypad1 Select"), false);
    S9xMapButton(kButtonUp, S9xGetCommandT("Joypad1 Up"), false);
    S9xMapButton(kButtonDown, S9xGetCommandT("Joypad1 Down"), false);
    S9xMapButton(kButtonLeft, S9xGetCommandT("Joypad1 Left"), false);
    S9xMapButton(kButtonRight, S9xGetCommandT("Joypad1 Right"), false);
    (void)S9xVerifyControllers();
}

void set_rom_title(AppState* app)
{
    app->rom_title = Memory.ROMName;
}

void prepare_after_rom_change(AppState* app)
{
    if (app == nullptr)
        return;

    set_rom_title(app);
    app->rom_loaded = true;
    audio_reset(app);
    Settings.Paused = FALSE;
    Settings.StopEmulation = FALSE;
    Settings.Mute = FALSE;
    S9xSetSoundMute(FALSE);
    set_timing(app);
    prime_audio(app);
    schedule_next_frame(app);
}

void autosave_if_needed()
{
    if (g_app != nullptr && g_app->rom_loaded)
        S9xAutoSaveSRAM();
}

} // namespace

bool refresh_framebuffer(AppState* app, bool* changed)
{
    return refresh_framebuffer_state(app, changed);
}

bool init_framebuffer(AppState* app)
{
    if (!refresh_framebuffer(app, nullptr)) {
        std::cout << "No framebuffer available\n";
        return false;
    }

    vk_set_framebuffer_resize_events(1);
    vk_set_startup_window_size(kStartupWindowWidth, kStartupWindowHeight);
    return true;
}

bool load_rom(AppState* app, const char* path)
{
    if (app == nullptr || path == nullptr || path[0] == '\0')
        return false;

    autosave_if_needed();
    app->rom_loaded = false;
    audio_reset(app);

    if (!Memory.LoadROM(path)) {
        std::cout << "Unable to load ROM: " << path << '\n';
        return false;
    }

    app->loaded_rom_path = path;
    Memory.LoadSRAM(S9xGetFilename(".srm", SRAM_DIR).c_str());
    prepare_after_rom_change(app);
    return true;
}

bool init_emulator(AppState* app)
{
    g_app = app;
    configure_defaults();

    if (!Memory.Init() || !S9xInitAPU()) {
        Memory.Deinit();
        S9xDeinitAPU();
        return false;
    }

    if (!S9xInitSound(32)) {
        S9xDeinitAPU();
        Memory.Deinit();
        return false;
    }

    S9xSetSoundMute(FALSE);
    S9xSetSamplesAvailableCallback([](void* data) {
        snes_audio_samples_available(static_cast<AppState*>(data));
    }, app);

    if (!S9xGraphicsInit()) {
        S9xSetSamplesAvailableCallback(nullptr, nullptr);
        S9xDeinitAPU();
        Memory.Deinit();
        return false;
    }

    map_controls();
    S9xCheatsEnable();
    return true;
}

void schedule_next_frame(AppState* app)
{
    app->timing.frame_tick_remainder += app->timing.frame_tick_numerator;
    const vk_u64 step = app->timing.frame_tick_remainder / app->timing.frame_tick_denominator;
    app->timing.frame_tick_remainder %= app->timing.frame_tick_denominator;
    app->timing.next_frame_tick += step == 0 ? 1 : step;
}

void idle_until_next_work(AppState* app)
{
    const vk_u64 now = VK_CALL(tick_count);
    if (now >= app->timing.next_frame_tick) {
        VK_CALL(yield);
        return;
    }

    const vk_u64 ticks_until_frame = app->timing.next_frame_tick - now;
    if (ticks_until_frame > 1
        && VK_CALL(snd_mix_is_playing, kAudioChannel)
        && app->audio.queue_count >= (kPlayBlockFrames * 2u)) {
        VK_CALL(sleep, ticks_until_frame - 1);
        return;
    }

    VK_CALL(yield);
}

void set_timing(AppState* app)
{
    const vk_u64 ticks_per_second = static_cast<vk_u64>(VK_CALL(ticks_per_sec));
    app->timing.next_frame_tick = VK_CALL(tick_count);
    app->timing.frame_tick_remainder = 0;
    app->timing.frame_tick_numerator = ticks_per_second * static_cast<vk_u64>(Settings.FrameTime);
    app->timing.frame_tick_denominator = 1000000u;
}

void reset_emulator(AppState* app)
{
    if (app == nullptr || !app->rom_loaded)
        return;

    S9xReset();
    prepare_after_rom_change(app);
}

void pump_input(AppState* app)
{
    vk_key_event_t event;
    while (VK_CALL(poll_key, &event)) {
        const bool pressed = event.pressed != 0;
        switch (event.scancode) {
            case 0x01:
                if (pressed)
                    app->quit_requested = true;
                break;
            case 0x0F:
                if (pressed)
                    app->loadrom_requested = true;
                break;
            case 0x0E:
                if (pressed)
                    app->reset_requested = true;
                break;
            case 0x39:
                S9xReportButton(kButtonSelect, pressed);
                break;
            case 0x1C:
                S9xReportButton(kButtonStart, pressed);
                break;
            case 0x1E:
                S9xReportButton(kButtonB, pressed);
                break;
            case 0x1F:
                S9xReportButton(kButtonA, pressed);
                break;
            case 0x10:
                S9xReportButton(kButtonY, pressed);
                break;
            case 0x11:
                S9xReportButton(kButtonX, pressed);
                break;
            case 0x20:
                S9xReportButton(kButtonL, pressed);
                break;
            case 0x12:
                S9xReportButton(kButtonR, pressed);
                break;
            case 0xC8:
            case 0x48:
                S9xReportButton(kButtonUp, pressed);
                break;
            case 0xD0:
            case 0x50:
                S9xReportButton(kButtonDown, pressed);
                break;
            case 0xCB:
            case 0x4B:
                S9xReportButton(kButtonLeft, pressed);
                break;
            case 0xCD:
            case 0x4D:
                S9xReportButton(kButtonRight, pressed);
                break;
            default:
                break;
        }
    }
}

void destroy_app(AppState* app)
{
    if (app == nullptr)
        return;

    autosave_if_needed();
    S9xSetSamplesAvailableCallback(nullptr, nullptr);
    VK_CALL(snd_mix_stop, kAudioChannel);
    S9xGraphicsDeinit();
    S9xDeinitAPU();
    Memory.Deinit();
    g_app = nullptr;
}

} // namespace snes9x_frontend

std::string S9xGetDirectory(enum s9x_getdirtype type)
{
    using namespace snes9x_frontend;

    if (g_app == nullptr)
        return ".";

    if (type == BIOS_DIR)
        return path_parent(g_app->loaded_rom_path.empty() ? std::string(".") : g_app->loaded_rom_path);

    if (!g_app->loaded_rom_path.empty())
        return path_parent(g_app->loaded_rom_path);

    if (!g_app->browser.current_path.empty())
        return g_app->browser.current_path;

    return ".";
}

std::string S9xGetFilenameInc(std::string extension, enum s9x_getdirtype dirtype)
{
    using namespace snes9x_frontend;

    const std::string base = S9xGetFilename(extension, dirtype);
    if (base.empty())
        return base;

    for (int index = 0; index < 1000; ++index) {
        char suffix[8];
        snprintf(suffix, sizeof(suffix), "-%03d", index);
        SplitPath path = splitpath(base);
        std::string stem = path.stem;
        stem += suffix;
        const std::string candidate = makepath(path.drive, path.dir, stem, path.ext);
        FILE* file = fopen(candidate.c_str(), "rb");
        if (file == nullptr)
            return candidate;
        fclose(file);
    }

    return base;
}

const char* S9xBasename(const char* in)
{
    static std::string name;
    name = in == nullptr ? "" : snes9x_frontend::path_basename(in);
    return name.c_str();
}

bool8 S9xOpenSnapshotFile(const char* filepath, bool8 read_only, STREAM* file)
{
    if (read_only) {
        if ((*file = OPEN_STREAM(filepath, "rb")) != nullptr)
            return TRUE;
    } else {
        if ((*file = OPEN_STREAM(filepath, "wb")) != nullptr)
            return TRUE;
    }

    return FALSE;
}

void S9xCloseSnapshotFile(STREAM file)
{
    CLOSE_STREAM(file);
}

void S9xAutoSaveSRAM()
{
    using namespace snes9x_frontend;

    if (g_app == nullptr || !g_app->rom_loaded)
        return;

    Memory.SaveSRAM(S9xGetFilename(".srm", SRAM_DIR).c_str());
}

void S9xExit()
{
    using namespace snes9x_frontend;

    if (g_app != nullptr)
        g_app->quit_requested = true;
}

bool8 S9xInitUpdate()
{
    using namespace snes9x_frontend;
    return refresh_framebuffer(g_app) ? TRUE : FALSE;
}

bool8 S9xDeinitUpdate(int width, int height)
{
    using namespace snes9x_frontend;

    if (g_app == nullptr || !refresh_framebuffer(g_app))
        return TRUE;

    if (width <= 0 || height <= 0)
        return TRUE;

    g_app->present_buffer.assign(g_app->present_buffer.size(), 0u);
    g_app->converted_frame.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

    const uint16* screen = GFX.Screen;
    const uint32 pitch_pixels = GFX.Pitch / sizeof(uint16);
    for (int y = 0; y < height; ++y) {
        const uint16* source_row = screen + static_cast<size_t>(y) * pitch_pixels;
        vk_u32* dest_row = &g_app->converted_frame[static_cast<size_t>(y) * static_cast<size_t>(width)];
        for (int x = 0; x < width; ++x)
            dest_row[x] = rgb565_to_pixel(source_row[x], g_app->framebuffer.format);
    }

    blit_scaled_image(g_app,
                      g_app->converted_frame.data(),
                      static_cast<vk_u32>(width),
                      static_cast<vk_u32>(height),
                      static_cast<vk_u32>(width));
    memcpy(reinterpret_cast<void*>(static_cast<uintptr_t>(g_app->framebuffer.base)),
           g_app->present_buffer.data(),
           g_app->present_buffer.size() * sizeof(vk_u32));
    return TRUE;
}

bool8 S9xContinueUpdate(int width, int height)
{
    return S9xDeinitUpdate(width, height);
}

void S9xMessage(int type, int, const char* message)
{
    const char* level = "info";
    switch (type) {
        case S9X_DEBUG: level = "debug"; break;
        case S9X_WARNING: level = "warn"; break;
        case S9X_ERROR:
        case S9X_FATAL_ERROR: level = "error"; break;
        default: break;
    }

    std::cout << "[snes9x][" << level << "] " << (message ? message : "") << '\n';
}

bool8 S9xOpenSoundDevice()
{
    return TRUE;
}

void S9xToggleSoundChannel(int)
{
}

void S9xSyncSpeed()
{
}

void S9xInitInputDevices()
{
}

void S9xHandlePortCommand(s9xcommand_t, int16, int16)
{
}

bool S9xPollButton(unsigned int, bool*)
{
    return false;
}

bool S9xPollAxis(unsigned int, int16*)
{
    return false;
}

bool S9xPollPointer(unsigned int, int16*, int16*)
{
    return false;
}

void S9xExtraUsage()
{
}

void S9xExtraDisplayUsage()
{
}

void S9xParseArg(char**, int&, int)
{
}

void S9xParseDisplayArg(char**, int&, int)
{
}

void S9xInitDisplay(int, char**)
{
}

void S9xDeinitDisplay()
{
}

void S9xPutImage(int width, int height)
{
    (void)S9xDeinitUpdate(width, height);
}

void S9xTextMode()
{
}

void S9xGraphicsMode()
{
}

const char* S9xStringInput(const char* in)
{
    return in;
}

void S9xProcessEvents(bool8)
{
}

void S9xSetTitle(const char*)
{
}

const char* S9xSelectFilename(const char*, const char*, const char*, const char*)
{
    return nullptr;
}
