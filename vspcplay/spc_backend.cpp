#include "spc_backend.h"

#include "../snes9x-src/apu/bapu/snes/snes.hpp"

namespace {

uint8_t g_voice_mute_mask = 0;

auto clamp_voice(int voice) -> int
{
    if (voice < 0)
        return 0;
    if (voice > 7)
        return 7;
    return voice;
}

void apply_voice_mute_mask(uint8_t mask)
{
    g_voice_mute_mask = mask;
    SNES::dsp.spc_dsp.mute_voices(mask);
}

} // namespace

auto vspcplay_read_apu_byte(uint16_t address) -> uint8_t
{
    switch (address) {
        case 0x00f2:
            return static_cast<uint8_t>(SNES::smp.status.dsp_addr);
        case 0x00f3:
            return static_cast<uint8_t>(SNES::dsp.spc_dsp.read(SNES::smp.status.dsp_addr & 0x7f));
        case 0x00f4:
        case 0x00f5:
        case 0x00f6:
        case 0x00f7:
            return SNES::cpu.port_read(static_cast<uint8_t>(address - 0x00f4));
        case 0x00f8:
            return static_cast<uint8_t>(SNES::smp.status.ram00f8);
        case 0x00f9:
            return static_cast<uint8_t>(SNES::smp.status.ram00f9);
        case 0x00fd:
            return static_cast<uint8_t>(SNES::smp.timer0.stage3_ticks & 0x0f);
        case 0x00fe:
            return static_cast<uint8_t>(SNES::smp.timer1.stage3_ticks & 0x0f);
        case 0x00ff:
            return static_cast<uint8_t>(SNES::smp.timer2.stage3_ticks & 0x0f);
        default:
            return SNES::smp.apuram[address];
    }
}

auto vspcplay_read_dsp_reg(uint8_t address) -> uint8_t
{
    return static_cast<uint8_t>(SNES::dsp.spc_dsp.read(address & 0x7f));
}

auto vspcplay_read_envx(int voice) -> int
{
    return SNES::dsp.spc_dsp.envx_value(clamp_voice(voice));
}

auto vspcplay_is_voice_muted(int voice) -> bool
{
    return (g_voice_mute_mask & (1u << clamp_voice(voice))) != 0;
}

void vspcplay_set_voice_muted(int voice, bool muted)
{
    const uint8_t bit = static_cast<uint8_t>(1u << clamp_voice(voice));
    const uint8_t mask = muted ? static_cast<uint8_t>(g_voice_mute_mask | bit)
                               : static_cast<uint8_t>(g_voice_mute_mask & ~bit);
    apply_voice_mute_mask(mask);
}

void vspcplay_set_all_voices_muted(bool muted)
{
    apply_voice_mute_mask(muted ? 0xffu : 0x00u);
}

void vspcplay_apply_voice_mute_mask(void)
{
    apply_voice_mute_mask(g_voice_mute_mask);
}

void vspcplay_write_input_port(int port, uint8_t value)
{
    SNES::cpu.port_write(static_cast<uint8_t>(port & 3), value);
}

auto vspcplay_read_input_port(int port) -> uint8_t
{
    return SNES::cpu.port_read(static_cast<uint8_t>(port & 3));
}

auto vspcplay_read_output_port(int port) -> uint8_t
{
    return static_cast<uint8_t>(SNES::smp.port_read(static_cast<unsigned>(port & 3)));
}
