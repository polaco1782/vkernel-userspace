/*
 * midi_vk.c - Minimal Duke music shim for vkernel
 *
 * Digital sound effects are backed by Duke's audiolib on vkernel. Music is
 * currently stubbed until a MIDI/OPL bridge is wired in for Duke's asset
 * format.
 */

#include <stdio.h>
#include <string.h>

#include "../duke3d-src/Game/src/audiolib/music.h"

int MUSIC_ErrorCode = MUSIC_Ok;

char *MUSIC_ErrorString(int error_number)
{
    (void)error_number;
    return "Music playback is unavailable on vkernel.";
}

int MUSIC_Init(int sound_card, int address)
{
    (void)sound_card;
    (void)address;
    MUSIC_ErrorCode = MUSIC_Ok;
    return MUSIC_Ok;
}

int MUSIC_Shutdown(void)
{
    return MUSIC_Ok;
}

void MUSIC_SetMaxFMMidiChannel(int channel)
{
    (void)channel;
}

void MUSIC_SetVolume(int volume)
{
    (void)volume;
}

void MUSIC_SetMidiChannelVolume(int channel, int volume)
{
    (void)channel;
    (void)volume;
}

void MUSIC_ResetMidiChannelVolumes(void)
{
}

int MUSIC_GetVolume(void)
{
    return 0;
}

void MUSIC_SetLoopFlag(int loopflag)
{
    (void)loopflag;
}

int MUSIC_SongPlaying(void)
{
    return 0;
}

void MUSIC_Continue(void)
{
}

void MUSIC_Pause(void)
{
}

int MUSIC_StopSong(void)
{
    return MUSIC_Ok;
}

int MUSIC_PlaySong(char *song_filename, int loopflag)
{
    (void)song_filename;
    (void)loopflag;
    return 0;
}

void MUSIC_SetContext(int context)
{
    (void)context;
}

int MUSIC_GetContext(void)
{
    return 0;
}

void MUSIC_SetSongTick(uint32_t position_in_ticks)
{
    (void)position_in_ticks;
}

void MUSIC_SetSongTime(uint32_t milliseconds)
{
    (void)milliseconds;
}

void MUSIC_SetSongPosition(int measure, int beat, int tick)
{
    (void)measure;
    (void)beat;
    (void)tick;
}

void MUSIC_GetSongPosition(songposition *pos)
{
    if (pos != NULL) {
        memset(pos, 0, sizeof(*pos));
    }
}

void MUSIC_GetSongLength(songposition *pos)
{
    if (pos != NULL) {
        memset(pos, 0, sizeof(*pos));
    }
}

int MUSIC_FadeVolume(int tovolume, int milliseconds)
{
    (void)tovolume;
    (void)milliseconds;
    return MUSIC_Ok;
}

int MUSIC_FadeActive(void)
{
    return 0;
}

void MUSIC_StopFade(void)
{
}

void MUSIC_RerouteMidiChannel(int channel, int cdecl (*function)(int event, int c1, int c2))
{
    (void)channel;
    (void)function;
}

void MUSIC_RegisterTimbreBank(unsigned char *timbres)
{
    (void)timbres;
}

void PlayMusic(char *filename)
{
    (void)filename;
}
