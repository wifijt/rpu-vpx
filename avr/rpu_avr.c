/* rpu-vpx: rpu_avr, the plugin that runs an RPU game's own .hex. An emulated ATmega2560 (simavr) runs the
 * hex; the RPU board's bus code (RPU.cpp RPU_DataWrite / RPU_DataRead) is turned back into the machine's
 * bus cycles, which go to PinMAME's memory map through rpu_avr_bus.
 *
 * Copyright (C) 2026 John Turner (wifijt). GPL-3.0, like RPU and simavr.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim_avr.h"
#include "sim_hex.h"
#include "avr_ioport.h"
#include "avr_uart.h"
#include "avr_eeprom.h"
#include "../plugin/rpu_avr.h"
#include "wavtrigger.h"

#ifdef _WIN32
#define RPU_AVR_EXPORT __declspec(dllexport)
#else
#define RPU_AVR_EXPORT __attribute__((visibility("default")))
#endif

#define F_MEGA 16000000

static avr_t *avr;
static const rpu_avr_bus *bus;
static int logOn;
static long busLog;                                     /* RPU_BUSLOG=n[@ms]: log n bus cycles (from ms) */
static avr_cycle_count_t busLogFrom;
static int watchAddr = -1;
static long breakPc = -1;                                /* RPU_BREAK=0xADDR (byte address): log each call there, r24 and the caller */                              /* RPU_WATCH=0xADDR: log each change of that RAM byte, with the PC */
static unsigned eeSaveIn;
static char eePath[1024];
static char line0[256];
static size_t line0Len;

/* the Mega's registers, by data-space address (ATmega2560) */
#define R_PINA  0x20
#define R_PORTA 0x22
#define R_PORTE 0x2E
#define R_PORTF 0x31
#define R_PORTG 0x34
#define R_PORTK 0x108

/*-- Rev 101 / 102 (Williams System 3-7): data PORTA, address PORTF (low) + PORTK (high), R/W PE5 (low = write),
     VMA PG1, clock PG2. On 6802/6808 boards the Mega makes the clock. Every clock with VMA high is a bus cycle,
     as on the real bus: RPU_DataRead/Write clock twice, and the timer interrupt can run its own cycle in the
     middle of one the main code started (VMA already high), so each rising edge transfers on the pins as they
     are then. --*/
static void rev102_clock(struct avr_irq_t *irq, uint32_t value, void *param) {
  (void)irq; (void)param;
  if (!value || !(avr->data[R_PORTG] & 0x02)) return;
  uint16_t a = (uint16_t)(avr->data[R_PORTF] | (avr->data[R_PORTK] << 8));
  if (avr->data[R_PORTE] & 0x20) {
    uint8_t d = bus->read(a);
    /* straight into PINA: simavr drops a pin input equal to the last one it was given, but writes to PORTA
       while DDRA was out have changed PINA since */
    avr->data[R_PINA] = d;
    if (busLog > 0 && avr->cycle >= busLogFrom) { busLog--; fprintf(stderr, "RPU bus %8.3f ms R %04X %02X\n", avr->cycle / 16000.0, a, d); }
  } else {
    bus->write(a, avr->data[R_PORTA]);
    if (busLog > 0 && avr->cycle >= busLogFrom) { busLog--; fprintf(stderr, "RPU bus %8.3f ms W %04X %02X\n", avr->cycle / 16000.0, a, avr->data[R_PORTA]); }
  }
}

/*-- Serial0: the game's debug log, a line at a time --*/
static void uart0_out(struct avr_irq_t *irq, uint32_t value, void *param) {
  (void)irq; (void)param;
  char c = (char)value;
  if (c == '\n' || line0Len == sizeof line0 - 1) {
    line0[line0Len] = 0;
    if (logOn && line0Len) fprintf(stderr, "RPU game: %s\n", line0);
    line0Len = 0;
  } else if (c != '\r') line0[line0Len++] = c;
}

/*-- Serial1: the WAV Trigger. The game's bytes go to it; its replies come back on the Mega's RX1 --*/
static avr_irq_t *rx1;
static void uart1_out(struct avr_irq_t *irq, uint32_t value, void *param) {
  (void)irq; (void)param;
  wt_rx((uint8_t)value);
}
static void uart1_reply(uint8_t b) { avr_raise_irq(rx1, b); }

/*-- the EEPROM: kept in a file beside the hex, written a second after the game last changed it --*/
static void ee_load(void) {
  FILE *f = fopen(eePath, "rb");
  uint8_t buf[4096];
  size_t n;
  if (!f) return;
  n = fread(buf, 1, sizeof buf, f);
  fclose(f);
  avr_eeprom_desc_t d = {buf, 0, (uint32_t)n};
  avr_ioctl(avr, AVR_IOCTL_EEPROM_SET, &d);
}
static void ee_save(void) {
  avr_eeprom_desc_t d = {NULL, 0, 4096};
  FILE *f;
  avr_ioctl(avr, AVR_IOCTL_EEPROM_GET, &d);
  if (!d.ee || !(f = fopen(eePath, "wb"))) return;
  fwrite(d.ee, 1, d.size, f);
  fclose(f);
}
static void no_sleep(avr_t *a, avr_cycle_count_t howLong) { (void)a; (void)howLong; }   /* never wait for real time */

static void quiet_uart(char n) {
  uint32_t f = 0;
  avr_ioctl(avr, AVR_IOCTL_UART_GET_FLAGS(n), &f);
  f &= ~(AVR_UART_FLAG_STDIO | AVR_UART_FLAG_POLL_SLEEP);
  avr_ioctl(avr, AVR_IOCTL_UART_SET_FLAGS(n), &f);
}

static int avr_open(const char *hex, const char *eeprom, const char *sounds, int rev, const rpu_avr_bus *b) {
  uint32_t size = 0, start = 0;
  uint8_t *code;
  logOn = getenv("RPU_PLUGIN_LOG") != NULL;
  if (getenv("RPU_BREAK")) breakPc = strtol(getenv("RPU_BREAK"), NULL, 0);
  if (getenv("RPU_WATCH")) watchAddr = (int)strtol(getenv("RPU_WATCH"), NULL, 0) & 0xffff;
  if (getenv("RPU_BUSLOG")) {
    const char *at = strchr(getenv("RPU_BUSLOG"), '@');
    busLog = atol(getenv("RPU_BUSLOG"));
    busLogFrom = at ? (avr_cycle_count_t)atol(at + 1) * 16000 : 0;
  }
  if (rev != 101 && rev != 102) { fprintf(stderr, "RPU: board rev %d isn't emulated yet (101 and 102 are)\n", rev); return -1; }
  if (!(code = read_ihex_file(hex, &size, &start))) { fprintf(stderr, "RPU: can't read %s\n", hex); return -1; }
  bus = b;
  avr = avr_make_mcu_by_name("atmega2560");
  avr_init(avr);
  avr->frequency = F_MEGA;
  avr->sleep = no_sleep;
  avr_loadcode(avr, code, size, start);
  free(code);
  avr->pc = 0;
  snprintf(eePath, sizeof eePath, "%s", eeprom);
  ee_load();
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('G'), IOPORT_IRQ_PIN2), rev102_clock, NULL);
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_OUTPUT), uart0_out, NULL);
  quiet_uart('0'); quiet_uart('1'); quiet_uart('2'); quiet_uart('3');
  rx1 = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('1'), UART_IRQ_INPUT);
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('1'), UART_IRQ_OUTPUT), uart1_out, NULL);
  wt_init(sounds, uart1_reply);
  fprintf(stderr, "RPU: %s: %u bytes, board rev %d\n", hex, size, rev);
  return 0;
}

static void avr_run_us(unsigned us) {
  avr_cycle_count_t end;
  if (!avr) return;
  end = avr->cycle + (avr_cycle_count_t)us * (F_MEGA / 1000000);
  while (avr->cycle < end) {
    int s = avr_run(avr);
    if (breakPc >= 0 && avr->pc == (avr_flashaddr_t)breakPc) {
      uint16_t sp = (uint16_t)(avr->data[0x5D] | (avr->data[0x5E] << 8));
      uint32_t ret = ((uint32_t)avr->data[sp + 1] << 16 | avr->data[sp + 2] << 8 | avr->data[sp + 3]) * 2;
      fprintf(stderr, "RPU break %8.3f ms: r24 %02X, from %05X, PINA %02X DDRA %02X\n", avr->cycle / 16000.0, avr->data[24], ret, avr->data[0x20], avr->data[0x21]);
    }
    if (watchAddr >= 0) {
      static int last = -1;
      if (avr->data[watchAddr] != last) {
        fprintf(stderr, "RPU watch %8.3f ms: [%04X] %02X -> %02X at pc %05X\n", avr->cycle / 16000.0, watchAddr, last & 0xff, avr->data[watchAddr], avr->pc);
        last = avr->data[watchAddr];
      }
    }
    if (s == cpu_Done || s == cpu_Crashed) { fprintf(stderr, "RPU: the Mega stopped (state %d, pc %05X)\n", s, avr->pc); avr = NULL; return; }
  }
  /* the EEPROM: a cheap check once a second of Mega time */
  if ((eeSaveIn += us) >= 1000000) { eeSaveIn = 0; ee_save(); }
}

static void avr_render(int16_t *out, int frames, int rate) {
  wt_render(out, frames, rate);
}

static void avr_close(void) {
  if (avr) ee_save();
}

static const rpu_avr_api api = {RPU_AVR_VERSION, avr_open, avr_run_us, avr_render, avr_close};
RPU_AVR_EXPORT const rpu_avr_api *rpu_avr_get(void) { return &api; }
