/* rpu-vpx: simavr without libelf. The plugin loads Intel hex only, so the ELF loader is a stub. */
#include "sim_elf.h"
int elf_read_firmware(const char *file, elf_firmware_t *firmware) { (void)file; (void)firmware; return -1; }
void sim_default_mcu(elf_firmware_t *fp, const char *filename) { (void)fp; (void)filename; }
