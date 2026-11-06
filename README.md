# rpu-vpx

Play your RetroPinUpgrade (RPU) game in Visual Pinball, from the same `.hex` you flash to the Mega. An emulated
ATmega2560 runs your game. PinMAME runs the machine's real hardware (displays, lamps, coils, switches, the sound
board) with its original CPU halted, as an RPU board halts it. Your WAV Trigger sounds play on a channel of their own.
Any VPX table made for the machine works.

**Status: early test build, not released.**

## Machines

| Machine | RPU board rev | `rpu_<game>.txt` | ROMs |
| --- | --- | --- | --- |
| Williams System 6 | 101 or 102 | `rom scrpn_l1` (the machine) and `rev 102` (your build's `RPU_OS_HARDWARE_REV`, default 102) | only the sound ROM, optional |

Bally -35 and Williams System 4 come next. This kit has no ROMs and never will: without the machine's sound ROM your
game still plays, with WAV sounds only. Use your own ROMs.

## What you need

- **Windows:** VPX 10.8 (64-bit), the DirectX June 2010 runtime (Microsoft's `directx_Jun2010_redist.exe`) and the
  Visual C++ 2015-2022 x64 redistributable. VPX's full installer brings both; its Minimal zip doesn't, and without
  them VPX quits at once. Then this kit: `VPinMAME64.dll` (run `Setup64.exe` to register it) and its `rpu` folder.
- **Mac:** VPX Standalone, then `tools/install-vpx-macos.sh <PINMAME_SHA>` puts the RPU PinMAME into it, and
  `avr/build.sh` builds `rpu_avr.dylib` (copy it to `~/.pinmame/rpu/`).

## Your game

1. In the Arduino IDE: **Sketch → Export Compiled Binary**. Take `<sketch>.ino.hex` (not the `with_bootloader` one).
2. Put it in the `rpu` folder beside `VPinMAME64.dll` (Mac: `~/.pinmame/rpu/`) as `rpu_<game>.hex`, with
   `rpu_<game>.txt` (the two lines above) and your WAVs in `rpu\<game>\` (`001_name.wav`, …).
3. In the table's script, set `cGameName = "rpu_<game>"`. PinMAME lists every game in the folder when it starts.
   On Windows, the first run of each game shows VPinMAME's own questions (About, then its ROM licence notice): answer
   them once and they don't come back.

Your game's settings (its EEPROM) are kept in `rpu_<game>.eep`. Delete it for a factory-fresh Mega.

Numbering: PinMAME counts switches, coils and lamps from 1, RPU from 0, so RPU switch 5 is PinMAME switch 6.

## Debugging

| Variable | What it does |
| --- | --- |
| `RPU_PLUGIN_LOG=1` | your game's `Serial` output (build with `DEBUG_MESSAGES 1`) and the WAV Trigger's track count |
| `RPU_AUTOTEST=1` | drops a coin and presses Start by itself, and prints the lamps and coils every 2 s |
| `RPU_BUSLOG=n[@ms]` | prints n bus cycles (from ms into the run): every PIA read and write |
| `RPU_WATCH=0xADDR` | prints each change of one RAM byte, with the program counter (`avr-nm` on your build's `.elf` gives addresses) |
| `RPU_BREAK=0xADDR` | prints each time the Mega reaches that flash address, with r24 and the caller |
| `RPU_PLUGIN_DIR` | another `rpu` folder |

## The compiled plugin

The first version of this kit built your sketch for the PC instead (`plugin/`, with
[rpu-sim](https://github.com/wifijt/rpu-sim)): `rpu_<game>.dll` or `.dylib` in the same folder. It still works, and
it's handy for stepping through your game in a debugger, but it doesn't run RPU's real hardware code. If a folder has
both, the `.hex` wins.

## In this repo

- `pinmame-rpu.patch`: the RPU driver (`src/wpc/rpu.c`) against PinMAME at `PINMAME_BASE`.
- `avr/`: `rpu_avr`, the emulated Mega and the bus bridge (simavr at `avr/SIMAVR_BASE`), and its WAV Trigger.
- `plugin/`: the interfaces (`rpu_avr.h`, `rpu_plugin.h`) and the compiled plugin's rpu-sim glue.
- `tools/`: `install-vpx-macos.sh`, `pmtest.cpp` (a game in libpinmame with no table), `plugtest.c`.
- `docs/hex-design.md`: how the `.hex` version works.
- `.github/workflows/vpinmame.yml`: builds `VPinMAME64.dll` and `rpu_avr.dll`.

Copyright (C) 2026 John Turner (wifijt). `rpu_avr` and the plugin glue are GPL-3.0, like RPU and simavr. The PinMAME
driver is BSD-3-Clause, PinMAME's license for new code.
