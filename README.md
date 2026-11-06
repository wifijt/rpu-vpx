# rpu-vpx

Play your RetroPinUpgrade (RPU) game in Visual Pinball, from the same `.hex` you flash to the Mega. An emulated
ATmega2560 runs your game, and PinMAME runs the machine around it (displays, lamps, coils, switches, the sound board)
with its original CPU halted, as an RPU board halts it. Your WAV Trigger sounds play too. Any VPX table made for the
machine works.

**Status: test release.** Windows and Mac. It works with Williams System 6 (RPU board Rev 101/102) and Bally -35
(Rev 4).

## Quick start (Windows)

Once:

1. Install [Visual Pinball X 10.8](https://github.com/vpinball/vpinball/releases) (64-bit) with its full installer.
   It brings DirectX and the Visual C++ runtime; without them VPX quits at once.
2. Download `rpu-vpinmame64-<version>.zip` from this repo's Releases and unzip it.
3. Copy everything in it into your VPinMAME folder (for example `C:\vPinball\VPinMAME\`), replacing
   `VPinMAME64.dll`. Keep the `rpu` folder beside it.
4. Run `Setup64.exe` there and click **Install**.

For each game:

5. In the Arduino IDE: **Sketch → Export Compiled Binary**. The sketch folder gets `<sketch>.ino.hex`. Use that one,
   not the `with_bootloader` one.
6. Find a VPX table for the machine your game runs on (a Bally Star Trek table for a Star Trek game).
7. In your VPinMAME folder, run:
   ```
   powershell -ExecutionPolicy Bypass -File add-game.ps1 C:\MyGame\MyGame.ino.hex mygame startreb -Sounds C:\MyGame\sounds -Table "C:\Tables\Star Trek.vpx"
   ```
   - `mygame`: your game's name, letters, digits and `_`.
   - `startreb`: the machine, by its PinMAME name (see [Machines](#machines)).
   - `-Sounds`: your WAV Trigger files (`001_name.wav`, …), if your game uses them.
   - `-Rev 101`: only if your build's `RPU_OS_HARDWARE_REV` isn't the default for the machine.
8. Open the new table, `Star Trek (rpu_mygame).vpx`, in VPX and play. The first time, VPinMAME asks two questions
   (About, then its ROM notice): answer them once.

After you change your game, export again and run step 7 without `-Table`. Your game's settings are kept.

## Quick start (Mac)

There's no ready-made Mac download yet: you build it from this repo.

1. Install [VPX Standalone](https://github.com/vpinball/vpinball/releases) in `~/Applications`.
2. `tools/install-vpx-macos.sh <PINMAME_SHA>` builds the RPU PinMAME and puts it into VPX. `<PINMAME_SHA>` is the
   PinMAME commit your VPX release was built with (`PINMAME_BASE` here is VPX 10.8.1's).
3. Clone [simavr](https://github.com/buserror/simavr) beside this repo and check out the commit in
   `avr/SIMAVR_BASE`, then `avr/build.sh` builds `rpu_avr.dylib`.
   Copy it to `~/.pinmame/rpu/`.
4. For each game: `tools/add-game.sh MyGame.ino.hex mygame startreb --sounds ~/MyGame/sounds --table ~/Tables/"Star Trek.vpx"`
   (a board rev other than the default goes after the machine: `... startreb 101 ...`).

## Machines

| Machine | Its PinMAME name, for example | RPU board rev (default) |
| --- | --- | --- |
| Williams System 6 | `scrpn_l1` (Scorpion) | 101 or 102 (102) |
| Bally -35 | `startrek`, `startreb` (Star Trek, 6- or 7-digit displays) | 4 (4) |

The machine's name gives your game its playfield, displays and sound board. Pick the one whose displays your table
was made for. Williams System 4 comes next.

This kit has no ROMs and never will. Your game doesn't need them; the machine's sound board does. Put your own ROM set
in VPinMAME's `roms` folder for its sounds, or play with your WAVs only.

## Good to know

- **Settings:** your game's EEPROM is kept in `rpu\rpu_mygame.eep`. Delete it for a factory-fresh Mega.
- **Switch numbers:** PinMAME counts switches, coils and lamps from 1, RPU from 0, so RPU switch 5 is switch 6 in a
  table's script.
- **The selector switch:** the emulated RPU board has it set to RPU, so a game that boots the original ROM with it
  off still runs.
- **By hand, without the script:** put `rpu_mygame.hex` in the `rpu` folder (Mac: `~/.pinmame/rpu/`), with
  `rpu_mygame.txt` holding `rom startreb` (and `rev 101` if needed) and your WAVs in `rpu\mygame\`. Then set
  `cGameName = "rpu_mygame"` in the table's script.

## Known issues

- **Speed:** a slow PC runs the game slower than real time (about 0.7× on a Pentium Silver laptop).
- **Loud WAVs clip:** WAVs mastered near full volume distort when two play at once. Leave some headroom.
- **VPX pauses your game** while its window isn't in front.
- **Score window on its side:** some tables rotate VPinMAME's display window for cabinets. In the table's script,
  change `.Games(cGameName).Settings.Value("rol") = ...` to `= 0`.

## Debugging

Set these before starting VPX:

| Variable | What it does |
| --- | --- |
| `RPU_PLUGIN_LOG=1` | your game's `Serial` output (build with `DEBUG_MESSAGES 1`) and the WAV Trigger's track count |
| `RPU_AUTOTEST=1` | drops a coin and presses Start by itself, and prints the lamps and coils every 2 s |
| `RPU_BUSLOG=n[@ms]` | prints n bus cycles (from ms into the run): every PIA read and write |
| `RPU_WATCH=0xADDR` | prints each change of one RAM byte, with the program counter (`avr-nm` on your build's `.elf` gives addresses) |
| `RPU_BREAK=0xADDR` | prints each time the Mega reaches that flash address, with r24 and the caller |
| `RPU_PCLOG=1` | prints where the Mega is every 100 ms (`avr-addr2line -f -e <sketch>.ino.elf 0xADDR` names it) |
| `RPU_PLUGIN_DIR` | another `rpu` folder |

## The compiled plugin

You can also build your sketch for the PC with [rpu-sim](https://github.com/wifijt/rpu-sim) (`plugin/`):
`rpu_<game>.dll` or `.dylib` in the same folder. It's handy for stepping through your game in a debugger, but it
doesn't run RPU's real hardware code. If a folder has both, the `.hex` wins.

## In this repo

- `pinmame-rpu.patch`: the RPU driver (`src/wpc/rpu.c`) against PinMAME at `PINMAME_BASE`.
- `avr/`: `rpu_avr`, the emulated Mega and the bus bridge (simavr at `avr/SIMAVR_BASE`), and its WAV Trigger.
- `plugin/`: the interfaces (`rpu_avr.h`, `rpu_plugin.h`) and the compiled plugin's rpu-sim glue.
- `tools/`: `add-game.ps1` / `add-game.sh` (the setup script), `install-vpx-macos.sh`, `pmtest.cpp` (a game in
  libpinmame with no table), `plugtest.c`.
- `docs/hex-design.md`: how the `.hex` version works.
- `.github/workflows/vpinmame.yml`: builds `VPinMAME64.dll` and `rpu_avr.dll`; a `v*` tag makes a release.

## Credits and licence

[RetroPinUpgrade](https://github.com/RetroPinUpgrade) is Dick Hamill's. The Mega is emulated by
[simavr](https://github.com/buserror/simavr), and the machines by [PinMAME](https://github.com/vpinball/pinmame).

Copyright (C) 2026 John Turner (wifijt). `rpu_avr`, the setup scripts and the plugin glue are GPL-3.0, like RPU and
simavr ([COPYING](COPYING)). The PinMAME driver (`pinmame-rpu.patch`, `plugin/rpu_avr.h`, `plugin/rpu_plugin.h`) is
BSD-3-Clause, PinMAME's licence for new code ([LICENSE-BSD](LICENSE-BSD)).
