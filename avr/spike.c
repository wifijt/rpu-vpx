/* rpu-vpx spike: run an RPU Rev 101/102 .hex in simavr with a fake bus (64 KB of RAM that keeps
 * what is written), log the bus cycles and Serial0, and measure the speed.
 * Usage: spike <game.hex> [seconds] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sim_avr.h"
#include "sim_hex.h"
#include "avr_ioport.h"
#include "avr_uart.h"

static avr_t *avr;
static uint8_t bus[65536];
static int vmaDone, logged;
static unsigned long reads, writes;
static avr_irq_t *pinA;

/* Rev 101/102 (RPU.cpp RPU_DataWrite/Read): data PORTA, address PORTF/PORTK, R/W PE5, VMA PG1, clock PG2 */
static uint8_t io(const char *port) {   /* the PORTx output latch */
  uint8_t v = 0;
  avr_ioctl(avr, AVR_IOCTL_IOPORT_GETSTATE(port[0]), &v);
  return v;
}
static uint8_t reg(int addr) { return avr->data[addr]; }

static void on_clock(struct avr_irq_t *irq, uint32_t value, void *param) {
  (void)irq; (void)param;
  if (!value) return;                                     /* rising edges only */
  uint8_t g = reg(0x34);                                  /* PORTG */
  if (!(g & 0x02)) { vmaDone = 0; return; }               /* VMA low: no cycle */
  if (vmaDone) return;                                    /* one transfer per VMA pulse */
  vmaDone = 1;
  uint16_t addr = reg(0x108) | (reg(0x31) << 8);          /* PORTK (0x108) high, PORTF (0x31) low */
  addr = (uint16_t)(reg(0x31) | (reg(0x108) << 8));
  int write = !(reg(0x2E) & 0x20);                        /* PORTE bit 5: R/W, low = write */
  if (write) { bus[addr] = reg(0x22); writes++; }        /* PORTA = 0x22 */
  else { avr_raise_irq(pinA, bus[addr]); reads++; }
  if (logged < 60) { logged++; fprintf(stderr, "  %c %04X %02X\n", write ? 'W' : 'R', addr, bus[addr]); }
}
static void on_vma(struct avr_irq_t *irq, uint32_t value, void *param) {
  (void)irq; (void)param; if (!value) vmaDone = 0;
}
static void on_uart(struct avr_irq_t *irq, uint32_t value, void *param) {
  (void)irq; (void)param; fputc((int)value, stdout);
}

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "usage: spike game.hex [seconds]\n"); return 1; }
  double secs = argc > 2 ? atof(argv[2]) : 3;
  uint32_t size = 0, start = 0;
  uint8_t *code = read_ihex_file(argv[1], &size, &start);
  if (!code) { fprintf(stderr, "can't read %s\n", argv[1]); return 1; }
  avr = avr_make_mcu_by_name("atmega2560");
  avr_init(avr);
  avr->frequency = 16000000;
  avr_loadcode(avr, code, size, start);
  avr->pc = 0;
  pinA = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('A'), IOPORT_IRQ_PIN_ALL);
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('G'), IOPORT_IRQ_PIN2), on_clock, NULL);
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('G'), IOPORT_IRQ_PIN1), on_vma, NULL);
  avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_OUTPUT), on_uart, NULL);
  uint32_t f = 0;                                          /* no "uart stdio" xon/xoff wait */
  avr_ioctl(avr, AVR_IOCTL_UART_GET_FLAGS('0'), &f);
  f &= ~AVR_UART_FLAG_STDIO;
  avr_ioctl(avr, AVR_IOCTL_UART_SET_FLAGS('0'), &f);
  fprintf(stderr, "loaded %u bytes at %u; running %.1f s of AVR time\n", size, start, secs);
  clock_t t0 = clock();
  avr_cycle_count_t end = (avr_cycle_count_t)(secs * avr->frequency);
  int state = cpu_Running;
  while (avr->cycle < end && state != cpu_Done && state != cpu_Crashed) state = avr_run(avr);
  double wall = (double)(clock() - t0) / CLOCKS_PER_SEC;
  fprintf(stderr, "\nstate %d, %.2f s AVR in %.2f s CPU (%.1fx real time); bus reads %lu, writes %lu, pc %05X\n",
          state, avr->cycle / 16e6, wall, (avr->cycle / 16e6) / wall, reads, writes, avr->pc);
  return 0;
}
