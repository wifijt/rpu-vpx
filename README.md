# rpu-vpx

Play your RetroPinUpgrade (RPU) game in Visual Pinball. Your sketch, unchanged, is built for your computer as a
plugin. A PinMAME with an RPU driver runs it in place of the machine's original CPU, on any VPX table made for that
machine. The machine's own sound board still plays (if you have its sound ROM), and your WAV Trigger sounds play on
a channel of their own.

**Status: early test build, not released.**

## Machines

| Machine | `rpu_<game>.txt` | ROMs |
| --- | --- | --- |
| Bally -35 (and Stern MPU-100/200) | `digits 6` or `digits 7`: the display the table was made for | none |
| Williams System 4 | `rom <game>`, e.g. `rom phnix_l1` | only the sound ROM, optional |
| Williams System 6 | `rom <game>`, e.g. `rom scrpn_l1` | only the sound ROM, optional |

Without the sound ROM, a System 4 or 6 game still plays, with WAV sounds only. This kit has no ROMs and never will.
Use your own.

## What you need

- **Windows:** VPX 10.8 (64-bit), and this kit's `VPinMAME64.dll` (run `Setup64.exe` to register it). To build your
  game: Python 3, MinGW-w64 g++ (WinLibs) and [rpu-sim](https://github.com/wifijt/rpu-sim).
- **Mac:** VPX Standalone, then `tools/install-vpx-macos.sh <PINMAME_SHA>` puts the RPU PinMAME into it. To build:
  Python 3, clang (Xcode command-line tools) and rpu-sim.

## Your game

1. Build the plugin from your sketch folder:
   `SIM_PLUGIN=<rpu-vpx>/plugin/sim_plugin.cpp SIM_PLUGIN_OUT=rpu_<game>.dll python3 <rpu-sim>/build.py <sketch folder>`
   (`.dylib` on a Mac).
2. Put `rpu_<game>.dll` in the `rpu` folder beside `VPinMAME64.dll` (Mac: `~/.pinmame/rpu/`), with your WAVs in
   `rpu\<game>\` and, if needed, `rpu_<game>.txt`.
3. In the table's script, set `cGameName = "rpu_<game>"`. PinMAME lists every plugin in the folder when it starts.

Numbering: PinMAME counts switches, coils and lamps from 1, RPU from 0, so RPU switch 5 is PinMAME switch 6.

Debugging: `RPU_PLUGIN_LOG=1` prints your game's log, `RPU_SWLOG=1` each switch passed to it, `RPU_AUTOTEST=1` drops a
coin and presses Start by itself. `RPU_PLUGIN_DIR` overrides the plugin folder.

## In this repo

- `pinmame-rpu.patch`: the RPU driver (`src/wpc/rpu.c`) against PinMAME at `PINMAME_BASE`.
- `plugin/`: the plugin interface and its rpu-sim glue.
- `tools/`: `install-vpx-macos.sh`, `plugtest.c` (a plugin on its own), `pmtest.cpp` (a game in libpinmame, no table).
- `.github/workflows/vpinmame.yml`: builds `VPinMAME64.dll` with the driver.

Copyright (C) 2026 John Turner (wifijt). The plugin glue is GPL-3.0, like RPU and rpu-sim. The PinMAME driver is
BSD-3-Clause, PinMAME's license for new code.
