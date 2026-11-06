/* rpu-vpx: a WAV Trigger on the emulated Mega's Serial1. The game's commands in, its replies out, its
 * WAVs mixed. Copyright (C) 2026 John Turner (wifijt). GPL-3.0. */
#ifndef RPU_WAVTRIGGER_H
#define RPU_WAVTRIGGER_H

#include <stdint.h>

/* sounds: the folder of NNN_name.wav files; reply: called for each byte the WAV Trigger sends back */
void wt_init(const char *sounds, void (*reply)(uint8_t b));
void wt_rx(uint8_t b);                                   /* a byte from the game */
void wt_render(int16_t *out, int frames, int rate);      /* stereo, interleaved L R */

#endif
