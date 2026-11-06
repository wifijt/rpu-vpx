# RPU .hex in VPX: design

Goal: a developer drops the `.hex` the Arduino IDE already makes (Sketch → Export Compiled Binary) into the `rpu`
folder, and the table plays it. No compiler, no rebuild per game, and the real firmware runs, timing included.

## How

The plugin emulates the RPU board, not the game:

- **An ATmega2560 emulator** ([simavr](https://github.com/buserror/simavr), GPL-3.0) runs the `.hex` at 16 MHz.
- **PinMAME runs the real machine** (Williams System 6, Bally -35, …) exactly as it does for the stock ROM, but with
  its 6800/6808 CPU halted, which is what RPU does to the real CPU.
- **A bus bridge** turns the Mega's pin changes into that machine's bus cycles. Each read or write goes through
  PinMAME's own memory map, so the PIAs, displays, lamps, solenoids and the sound board behave as in the stock
  game. Nothing per game, and nothing per machine beyond what PinMAME already emulates.

So one plugin (`rpu_avr`) serves every game. `rpu_<game>.hex` plus `rpu_<game>.txt` (the machine, e.g.
`rom scrpn_l1`, and the board rev) is the whole install.

## The bus, per RPU board rev

From RPU.cpp (`RPU_DataWrite` / `RPU_DataRead`):

| Rev | Machines | Data | Address | R/W | VMA | Clock |
| --- | --- | --- | --- | --- | --- | --- |
| 101/102 | Williams System 3–7 | PORTA | PORTF (low), PORTK (high) | PE5 | PG1 | PG2: the Mega makes it on 6802/6808 boards; on 6800 boards it waits on the board's |
| 4 | Bally -17/-35, Stern | PORTA | PORTF (low), PORTK (high) | PE5 | PG1 | PG2: the board's φ2 (the Mega waits on it) |

A cycle: every clock rising edge with VMA high does a transfer, on the pins as they are then, as on the real bus
(RPU's reads and writes hold VMA for two clocks, and its timer interrupt can run its own cycles inside one the main
code started). On a read, the bridge calls PinMAME's read at that address and puts the byte on PINA before the
sketch samples it. On a write, it passes PORTA to PinMAME's write.

Where the Mega waits on a clock it doesn't make (6800 boards, Bally), the bridge answers reads of that pin from
the cycle count (the machine's CPU clock, 500 kHz on the -35) instead of toggling it, so it costs nothing per
cycle. A rising edge the Mega sees with VMA high is a cycle.

## Everything else on the Mega

| Mega | In the plugin |
| --- | --- |
| Timer 1 (RPU's interrupt) | simavr's own timers: the sketch's ISRs run as on the board |
| IRQ (pin 2, INT4) | Bally: the PIAs' IRQ line, low while asserted, updated after every bus cycle and each slice |
| Selector switch (pin 38) | closed: run the RPU game |
| Serial0 (debug) | the game log |
| Serial1 (WAV Trigger) | parsed as the WAV Trigger protocol (measured on a real one, see the RPU-SP notes): the WAV channel |
| Serial2 (PinNode link) | ignored for now; later it could reach a real node |
| EEPROM | saved beside the hex, like the plugin build's |
| Board-ID / select pins | strapped as the rev's board would be |

## Risks

- **Speed:** a 16 MHz AVR in real time is easy on any recent PC, but slow laptops (the Pentium Silver test laptop)
  may not keep up. The bridge runs the AVR in slices tied to PinMAME's timeslices, so it slows the game, not the
  sound.
- **Exactness of the pins:** each rev's bus code has to match RPU.cpp. Rev 102 first (the Scorpion test table),
  then Rev 4 (Bally).
- **Licence:** simavr is GPL-3.0, so it lives only in the plugin (GPL, like RPU). The PinMAME driver stays
  BSD-3-Clause and only exposes the bus, IRQs and time to the plugin.

## Order of work

1. Spike on the Mac: simavr runs the Scorpion's Rev 102 hex with a fake bus, and logs its PIA writes during
   RPU_InitializeMPU.
2. Bridge into PinMAME's System 6 driver with the CPU halted: the attract mode lights.
3. Serial1 → WAV channel; EEPROM; the debug log.
4. Windows (CI) and the laptop test; then Rev 4 / Bally.
5. The compiled-plugin path stays for developers who want it (faster, debuggable), but the hex is the default.
