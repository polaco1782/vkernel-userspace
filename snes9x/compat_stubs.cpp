#include "frontend.h"

#include "../snes9x-src/cheats.h"

#include <string>

void S9xInitWatchedAddress(void)
{
    memset(watches, 0, sizeof(watches));
    memset(Cheat.CWatchRAM, 0, sizeof(Cheat.CWatchRAM));
}

void S9xInitCheatData(void)
{
    Cheat.group.clear();
    Cheat.enabled = FALSE;
}

int S9xAddCheatGroup(const std::string&, const std::string&)
{
    return -1;
}

int S9xModifyCheatGroup(uint32_t, const std::string&, const std::string&)
{
    return -1;
}

void S9xEnableCheatGroup(uint32_t)
{
}

void S9xDisableCheatGroup(uint32_t)
{
}

void S9xDeleteCheatGroup(uint32_t)
{
}

void S9xDeleteCheats(void)
{
    Cheat.group.clear();
}

void S9xUpdateCheatsInMemory(void)
{
}

std::string S9xCheatGroupToText(const SCheatGroup&)
{
    return std::string();
}

std::string S9xCheatGroupToText(uint32_t)
{
    return std::string();
}

bool8 S9xLoadCheatFile(const std::string&)
{
    return FALSE;
}

bool8 S9xSaveCheatFile(const std::string&)
{
    return FALSE;
}

void S9xCheatsDisable(void)
{
    Cheat.enabled = FALSE;
    Settings.ApplyCheats = FALSE;
}

void S9xCheatsEnable(void)
{
    Cheat.enabled = TRUE;
    Settings.ApplyCheats = TRUE;
}

std::string S9xCheatValidate(const std::string&)
{
    return std::string();
}

int S9xImportCheatsFromDatabase(const std::string&)
{
    return 0;
}

bool8 S9xDoScreenshot(int, int)
{
    S9xMessage(S9X_WARNING, 0, "Screenshot support is disabled in this VK port build.");
    Settings.TakeScreenshot = FALSE;
    return FALSE;
}

extern "C" int ftruncate(int, long)
{
    return -1;
}
