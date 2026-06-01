#include "../snes9x-src/apu/apu.h"
#include "../snes9x-src/msu1.h"

struct SSettings Settings = {};
struct SCPUState CPU = {};
struct STimings Timings = {};
struct SSNESGameFixes SNESGameFixes = {};
struct SRegisters Registers = {};
char String[513] = {};
struct SMSU1 MSU1 = {};

bool8 S9xOpenSoundDevice(void)
{
    return TRUE;
}

std::string S9xGetFilenameInc(std::string extension, enum s9x_getdirtype)
{
    return std::string("vspcplay") + extension;
}

void S9xResetMSU(void)
{
}

void S9xMSU1Init(void)
{
}

void S9xMSU1DeInit(void)
{
}

bool S9xMSU1ROMExists(void)
{
    return false;
}

STREAM S9xMSU1OpenFile(const char *, bool)
{
    return nullptr;
}

void S9xMSU1Generate(size_t)
{
}

uint8 S9xMSU1ReadPort(uint8)
{
    return 0xff;
}

void S9xMSU1WritePort(uint8, uint8)
{
}

size_t S9xMSU1Samples(void)
{
    return 0;
}

void S9xMSU1SetOutput(Resampler *)
{
}

void S9xMSU1PostLoadState(void)
{
}
