#include "libspc.h"

#include "../snes9x-src/apu/apu.h"
#include "../snes9x-src/apu/bapu/snes/snes.hpp"
#include "../snes9x-src/msu1.h"
#include "spc_backend.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

constexpr int kMixRate = 100;
constexpr uint64_t kNtscMasterClockNumerator = 236250000ull;
constexpr uint64_t kNtscMasterClockDenominator = 11ull * kMixRate;
constexpr uint8 kEchoEnableReg = 0x4d;
constexpr uint8 kEchoVolumeLeftReg = 0x2c;
constexpr uint8 kEchoVolumeRightReg = 0x3c;

struct SpcFile {
    unsigned char header[33];
    unsigned char idtag[3];
    unsigned char version_minor;
    unsigned char pc_low;
    unsigned char pc_high;
    unsigned char a;
    unsigned char x;
    unsigned char y;
    unsigned char psw;
    unsigned char sp;
    unsigned char unused_a[2];
    unsigned char id666[210];
    unsigned char apuram[65536];
    unsigned char dsp_registers[128];
    unsigned char unused_b[64];
    unsigned char iplrom[64];
};

SPC_Config g_config = { 44100, 16, 2, 0, 0 };
int g_samples_per_mix = 0;
int g_buffer_size_bytes = 0;
uint64_t g_cpu_cycle_fraction = 0;

void configure_settings(const SPC_Config& config)
{
    memset(&Settings, 0, sizeof(Settings));
    memset(&CPU, 0, sizeof(CPU));
    memset(&Timings, 0, sizeof(Timings));
    memset(&SNESGameFixes, 0, sizeof(SNESGameFixes));
    memset(&Registers, 0, sizeof(Registers));
    memset(&MSU1, 0, sizeof(MSU1));
    memset(String, 0, sizeof(String));

    Settings.Stereo = config.channels == 2 ? TRUE : FALSE;
    Settings.SixteenBitSound = config.resolution == 16 ? TRUE : FALSE;
    Settings.SoundPlaybackRate = config.sampling_rate;
    Settings.SoundInputRate = 32040;
    Settings.SoundSync = FALSE;
    Settings.DynamicRateControl = FALSE;
    Settings.Mute = FALSE;
    Settings.PAL = FALSE;
    Settings.MSU1 = FALSE;
    Settings.ReverseStereo = FALSE;
    Settings.InterpolationMethod =
        config.is_interpolation ? DSP_INTERPOLATION_GAUSSIAN : DSP_INTERPOLATION_NONE;
    Settings.OneClockCycle = 6;
    Settings.OneSlowClockCycle = 8;
    Settings.TwoClockCycles = 12;

    Timings.H_Max = SNES_CYCLES_PER_SCANLINE;
    Timings.V_Max = SNES_MAX_NTSC_VCOUNTER;

    S9xAPUTimingSetSpeedup(0);
}

void apply_echo_preference(void)
{
    if (g_config.is_echo)
        return;

    SNES::dsp.spc_dsp.write(kEchoEnableReg, 0x00);
    SNES::dsp.spc_dsp.write(kEchoVolumeLeftReg, 0x00);
    SNES::dsp.spc_dsp.write(kEchoVolumeRightReg, 0x00);
}

void run_apu_batch(void)
{
    const uint64_t scaled_cycles = g_cpu_cycle_fraction + kNtscMasterClockNumerator;
    const int32 cpu_cycles = static_cast<int32>(scaled_cycles / kNtscMasterClockDenominator);
    g_cpu_cycle_fraction = scaled_cycles % kNtscMasterClockDenominator;

    CPU.Cycles = cpu_cycles;
    S9xAPUExecute();
    SNES::dsp.synchronize();
    CPU.Cycles = 0;
    S9xAPUSetReferenceTime(0);
}

void restore_spc(const SpcFile& spc)
{
    memcpy(SNES::smp.apuram, spc.apuram, sizeof(spc.apuram));
    SNES::cpu.reset();

    SNES::smp.clock = 0;
    SNES::dsp.clock = 0;
    SNES::smp.opcode_number = 0;
    SNES::smp.opcode_cycle = 0;
    SNES::smp.rd = 0;
    SNES::smp.wr = 0;
    SNES::smp.dp = 0;
    SNES::smp.sp = 0;
    SNES::smp.ya = 0;
    SNES::smp.bit = 0;

    SNES::smp.regs.pc = static_cast<uint16>(spc.pc_low | (spc.pc_high << 8));
    SNES::smp.regs.sp = spc.sp;
    SNES::smp.regs.B.a = spc.a;
    SNES::smp.regs.x = spc.x;
    SNES::smp.regs.B.y = spc.y;
    SNES::smp.regs.p = spc.psw;

    SNES::smp.timer0.stage1_ticks = 0;
    SNES::smp.timer1.stage1_ticks = 0;
    SNES::smp.timer2.stage1_ticks = 0;
    SNES::smp.timer0.stage2_ticks = 0;
    SNES::smp.timer1.stage2_ticks = 0;
    SNES::smp.timer2.stage2_ticks = 0;

    SNES::dsp.spc_dsp.restore_spc_file(spc.dsp_registers);

    SNES::smp.mmio_write(0xf1, spc.apuram[0xf1]);
    SNES::smp.mmio_write(0xf2, spc.apuram[0xf2]);
    SNES::smp.mmio_write(0xf8, spc.apuram[0xf8]);
    SNES::smp.mmio_write(0xf9, spc.apuram[0xf9]);
    SNES::smp.mmio_write(0xfa, spc.apuram[0xfa]);
    SNES::smp.mmio_write(0xfb, spc.apuram[0xfb]);
    SNES::smp.mmio_write(0xfc, spc.apuram[0xfc]);
    SNES::smp.timer0.stage3_ticks = spc.apuram[0xfd] & 0x0f;
    SNES::smp.timer1.stage3_ticks = spc.apuram[0xfe] & 0x0f;
    SNES::smp.timer2.stage3_ticks = spc.apuram[0xff] & 0x0f;

    S9xClearSamples();
    S9xSetSoundMute(FALSE);
    apply_echo_preference();
    vspcplay_apply_voice_mute_mask();
}

} // namespace

extern "C" {

int SPC_init(SPC_Config *cfg)
{
    return SPC_set_state(cfg);
}

void SPC_close(void)
{
    S9xDeinitAPU();
}

int SPC_set_state(SPC_Config *cfg)
{
    if (cfg != nullptr)
        g_config = *cfg;

    g_cpu_cycle_fraction = 0;
    g_samples_per_mix = (g_config.sampling_rate / kMixRate) * g_config.channels;
    g_buffer_size_bytes = g_samples_per_mix;
    if (g_config.resolution == 16)
        g_buffer_size_bytes *= static_cast<int>(sizeof(int16));

    configure_settings(g_config);
    S9xInitAPU();
    if (!S9xInitSound(0))
        return 0;
    S9xResetAPU();
    return g_buffer_size_bytes;
}

void SPC_update(unsigned char *buf)
{
    if (buf == nullptr || g_samples_per_mix <= 0)
        return;

    while (S9xGetSampleCount() < g_samples_per_mix)
        run_apu_batch();

    if (!S9xMixSamples(buf, g_samples_per_mix))
        memset(buf, 0, static_cast<size_t>(g_buffer_size_bytes));
}

int SPC_load(const char *fname)
{
    FILE *fp = fopen(fname, "rb");
    if (fp == nullptr)
        return FALSE;

    const int loaded = SPC_loadFP(fp);
    fclose(fp);
    return loaded;
}

int SPC_loadFP(FILE *fp)
{
    if (fp == nullptr)
        return FALSE;

    SpcFile spc = {};
    rewind(fp);
    if (fread(&spc, 1, sizeof(spc), fp) != sizeof(spc))
        return FALSE;

    S9xResetAPU();
    restore_spc(spc);
    return TRUE;
}

SPC_ID666 *SPC_get_id666(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
        return NULL;

    SPC_ID666 *id666 = SPC_get_id666FP(fp);
    fclose(fp);
    return id666;
}

SPC_ID666 *SPC_get_id666FP(FILE *fp)
{
    SPC_ID666 *id = (SPC_ID666 *)malloc(sizeof(*id));
    unsigned char playtime_str[4] = { 0, 0, 0, 0 };
    if (id == NULL)
        return NULL;

    fseek(fp, 0x23, SEEK_SET);
    if (fgetc(fp) == 27) {
        free(id);
        return NULL;
    }

    fseek(fp, 0x2E, SEEK_SET);
    fread(id->songname, 1, 32, fp);
    id->songname[32] = '\0';

    fread(id->gametitle, 1, 32, fp);
    id->gametitle[32] = '\0';

    fread(id->dumper, 1, 16, fp);
    id->dumper[16] = '\0';

    fread(id->comments, 1, 32, fp);
    id->comments[32] = '\0';

    fseek(fp, 0xA9, SEEK_SET);
    fread(playtime_str, 1, 3, fp);
    id->playtime = atoi((char*)playtime_str);

    fseek(fp, 0xD1, SEEK_SET);
    switch (fgetc(fp)) {
        case 1:
            id->emulator = SPC_EMULATOR_ZSNES;
            break;
        case 2:
            id->emulator = SPC_EMULATOR_SNES9X;
            break;
        case 0:
        default:
            id->emulator = SPC_EMULATOR_UNKNOWN;
            break;
    }

    fseek(fp, 0xB0, SEEK_SET);
    fread(id->author, 1, 32, fp);
    id->author[32] = '\0';
    return id;
}

int SPC_write_id666(SPC_ID666 *id, const char *filename)
{
    if (id == NULL)
        return FALSE;

    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
        return FALSE;

    fseek(fp, 0, SEEK_END);
    int spc_size = ftell(fp);
    rewind(fp);

    unsigned char *spc_buf = (unsigned char *)malloc(spc_size);
    if (spc_buf == NULL) {
        fclose(fp);
        return FALSE;
    }

    fread(spc_buf, 1, spc_size, fp);
    fclose(fp);

    if (*(spc_buf + 0x23) == 27) {
        free(spc_buf);
        return FALSE;
    }

    memset(spc_buf + 0x2E, 0, 119);
    memset(spc_buf + 0xA9, 0, 38);
    memset(spc_buf + 0x2E, 0, 36);

    memcpy(spc_buf + 0x2E, id->songname, 32);
    memcpy(spc_buf + 0x4E, id->gametitle, 32);
    memcpy(spc_buf + 0x6E, id->dumper, 16);
    memcpy(spc_buf + 0x7E, id->comments, 32);
    memcpy(spc_buf + 0xB0, id->author, 32);

    spc_buf[0xD0] = 0;
    switch (id->emulator) {
        case SPC_EMULATOR_ZSNES:
            *(spc_buf + 0xD1) = 1;
            break;
        case SPC_EMULATOR_SNES9X:
            *(spc_buf + 0xD1) = 2;
            break;
        case SPC_EMULATOR_UNKNOWN:
        default:
            *(spc_buf + 0xD1) = 0;
            break;
    }

    fp = fopen(filename, "wb");
    if (fp == NULL) {
        free(spc_buf);
        return FALSE;
    }

    fwrite(spc_buf, 1, spc_size, fp);
    fclose(fp);
    free(spc_buf);
    return TRUE;
}

} // extern "C"
