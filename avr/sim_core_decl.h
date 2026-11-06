/* rpu-vpx: simavr's core table, ATmega2560 only (simavr's Makefile generates this for every core) */
#ifndef __SIM_CORE_DECL_H__
#define __SIM_CORE_DECL_H__
extern avr_kind_t mega2560;
avr_kind_t * avr_kind[] = { &mega2560, NULL };
#endif
