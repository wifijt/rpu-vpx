/* rpu-vpx: the interface between PinMAME's RPU driver and rpu_avr, the plugin that runs an RPU
 * game's own .hex on an emulated ATmega2560 and the RPU board's bus.
 *
 * Copyright (C) 2026 John Turner (wifijt). The plugin is GPL-3.0 (it contains simavr); this header is
 * BSD-3-Clause so PinMAME's driver can include it. The driver loads rpu_avr at run time and calls
 * rpu_avr_get().
 */
#ifndef RPU_AVR_H
#define RPU_AVR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RPU_AVR_VERSION 1

/* The machine's bus, as the halted CPU's socket sees it: the driver passes these to PinMAME's memory map. */
typedef struct {
  uint8_t (*read)(uint16_t address);
  void (*write)(uint16_t address, uint8_t data);
} rpu_avr_bus;

typedef struct {
  int version;                                        /* RPU_AVR_VERSION */
  /* Loads the game. hex: the Arduino IDE's .hex; eeprom: the file that keeps the Mega's EEPROM (made if
     missing); sounds: the game's WAV folder (NNN_name.wav); rev: RPU_OS_HARDWARE_REV the hex was built
     for. 0 on success. */
  int (*open)(const char *hex, const char *eeprom, const char *sounds, int rev, const rpu_avr_bus *bus);
  void (*run_us)(unsigned us);                        /* advances the Mega by us microseconds */
  /* The WAV Trigger on Serial1: mixes `frames` stereo frames at `rate` into `out` (interleaved L R). */
  void (*render_audio)(int16_t *out, int frames, int rate);
  void (*close)(void);                                /* saves the EEPROM */
} rpu_avr_api;

typedef const rpu_avr_api *(*rpu_avr_get_fn)(void);
const rpu_avr_api *rpu_avr_get(void);

#ifdef __cplusplus
}
#endif
#endif
