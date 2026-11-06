/* rpu-vpx: simavr without libelf or gdb. The plugin loads Intel hex only and never opens a gdb port. */
#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_gdb.h"
int elf_read_firmware(const char *file, elf_firmware_t *firmware) { (void)file; (void)firmware; return -1; }
void sim_default_mcu(elf_firmware_t *fp, const char *filename) { (void)fp; (void)filename; }
int avr_gdb_init(avr_t *avr) { (void)avr; return -1; }
void avr_deinit_gdb(avr_t *avr) { (void)avr; }
int avr_gdb_processor(avr_t *avr, int sleep) { (void)avr; (void)sleep; return 0; }
void avr_gdb_handle_watchpoints(avr_t *g, uint16_t addr, enum avr_gdb_watch_type type) { (void)g; (void)addr; (void)type; }
void avr_gdb_handle_break(avr_t *avr) { (void)avr; }
