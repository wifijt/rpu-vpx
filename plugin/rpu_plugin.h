/* rpu-pinmame: the interface between PinMAME's RPU driver and an RPU game built as a plugin.
 *
 * Copyright (C) 2026 John Turner (wifijt). The game plugin is GPL-3.0, like RPU and rpu-sim; this
 * header is BSD-3-Clause so PinMAME's driver can include it. The driver loads the plugin at run time
 * (dlopen / LoadLibrary) and calls rpu_plugin_get().
 *
 * Numbers here are the game's own (RPU numbering, from 0). The driver adds PinMAME's offsets.
 */
#ifndef RPU_PLUGIN_H
#define RPU_PLUGIN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RPU_PLUGIN_VERSION 1

typedef struct {
  int version;                                        /* RPU_PLUGIN_VERSION */
  const char *game;                                   /* e.g. "Scorpion2022" */

  /* Runs the game's setup(). sound_dir: the game's WAV files (NNN_name.wav); eeprom: the saved
     settings (size bytes), or NULL for a fresh machine. */
  void (*start)(const char *sound_dir, const uint8_t *eeprom, int size);
  /* Advances the game by ms milliseconds: one pass of loop() each. */
  void (*run_ms)(unsigned ms);

  void (*set_switch)(int sw, int closed);
  int (*lamp)(int n);                                 /* 1 lit (flashing worked out at the current time) */
  uint32_t (*solenoids)(void);                        /* bit n: coil n is energised now */
  int (*flippers_enabled)(void);
  int (*display_digit)(int display, int pos);         /* display 0-3, pos 0 = leftmost: 0-9, or -1 blank */
  int (*display_digits)(void);                        /* digits per player display */
  int (*credits)(void);                               /* the credit display, -1 off */
  int (*ball_in_play)(void);                          /* the ball-in-play / match display, -1 off */

  /* The machine's own sound board: the next command (RPU's sound number), or -1. */
  int (*sound_command)(void);
  /* The WAV Trigger: mixes `frames` stereo frames at `rate` into `out` (interleaved L R). */
  void (*render_audio)(int16_t *out, int frames, int rate);

  /* The game's EEPROM, for the host to save; dirty is set when it changed since the last call. */
  const uint8_t *(*eeprom)(int *size, int *dirty);
} rpu_plugin_api;

typedef const rpu_plugin_api *(*rpu_plugin_get_fn)(void);
const rpu_plugin_api *rpu_plugin_get(void);

#ifdef __cplusplus
}
#endif
#endif
