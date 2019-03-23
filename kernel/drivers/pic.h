#ifndef DRIVERS_PIC_H
#define DRIVERS_PIC_H

////////////////////////////////////////////////////////////////////////////
// Programmable Interrupt Controller
////////////////////////////////////////////////////////////////////////////
//
// This is a simple driver for interacting with the pic
//
// TODO: Replace this with an APIC driver
//
////////////////////////////////////////////////////////////////////////////

#include <common/stdint.h>

void pic_remap(uint8_t master_offset, uint8_t slave_offset);
void pic_send_eoi(uint8_t irq);

extern void pic_disable();

#endif