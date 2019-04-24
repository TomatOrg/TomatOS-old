#ifndef MEMORY_GDT_H
#define MEMORY_GDT_H

////////////////////////////////////////////////////////////////////////////
// Global Descriptor Table
////////////////////////////////////////////////////////////////////////////
//
// This has some stuff related to the GDT
//
// The GDT itself is already setup on boot so we don't need to touch
// it at all in runtime
//
////////////////////////////////////////////////////////////////////////////

#define GDT_KERNEL_CODE 8
#define GDT_KERNEL_DATA 16
#define GDT_USER_DATA 24
#define GDT_USER_CODE 32

#endif //TOMATKERNEL_GDT_H