#pragma once

#include <stdint.h>

auto vspcplay_read_apu_byte(uint16_t address) -> uint8_t;
auto vspcplay_read_dsp_reg(uint8_t address) -> uint8_t;
auto vspcplay_read_envx(int voice) -> int;
auto vspcplay_is_voice_muted(int voice) -> bool;
void vspcplay_set_voice_muted(int voice, bool muted);
void vspcplay_set_all_voices_muted(bool muted);
void vspcplay_apply_voice_mute_mask(void);
void vspcplay_write_input_port(int port, uint8_t value);
auto vspcplay_read_input_port(int port) -> uint8_t;
auto vspcplay_read_output_port(int port) -> uint8_t;
