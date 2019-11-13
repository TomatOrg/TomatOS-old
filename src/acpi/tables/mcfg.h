#include <libc/stddef.h>
#include <lai/include/acpispec/tables.h>

#ifndef TOMATKERNEL_MCFG_H
#define TOMATKERNEL_MCFG_H

typedef struct mcfg_entry {
    uint64_t base;
    uint16_t segment;
    uint8_t start_pci_bus;
    uint8_t end_pci_bus;
    uint32_t reserved;
} __attribute__((packed)) mcfg_entry_t;

typedef struct acpi_mcfg {
    acpi_header_t header;
    uint64_t reserved;
    mcfg_entry_t entries[0];
} __attribute__((packed)) acpi_mcfg_t;

extern acpi_mcfg_t* acpi_mcfg;

/**
 * Will try to find the mcfg table
 *
 * @remark
 * This will not throw an error because it is not a table we must have in
 * order for the os to work (only needed for PCIe)
 *
 */
void mcfg_init();

#endif //TOMATKERNEL_MCFG_H