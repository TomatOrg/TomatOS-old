#include <lai/host.h>
#include <mem/vmm.h>
#include <sync/critical.h>
#include <arch/timing.h>
#include <util/string.h>
#include <proc/process.h>
#include <mem/mm.h>
#include <util/stb_ds.h>
#include "apic.h"
#include "intrin.h"
#include "idt.h"

#define SMP_FLAG                (0x510 + DIRECT_MAPPING_BASE)
#define SMP_KERNEL_ENTRY        (0x520 + DIRECT_MAPPING_BASE)
#define SMP_KERNEL_PAGE_TABLE   (0x540 + DIRECT_MAPPING_BASE)
#define SMP_STACK_POINTER       (0x550 + DIRECT_MAPPING_BASE)
#define SMP_KERNEL_GDT          (0x580 + DIRECT_MAPPING_BASE)
#define SMP_KERNEL_IDT          (0x590 + DIRECT_MAPPING_BASE)

/**
 * The mmio address used to talk with the lapic
 */
static uint8_t* g_lapic_base = NULL;

/**
 * The frequency of the local lapic
 */
static uint64_t CPU_LOCAL g_lapic_freq;

// represents a single ioapic
typedef struct ioapic {
    directptr_t base;
    size_t gsi_start;
    size_t gsi_end;
} ioapic_t;

/**
 * List of ioapics available to the system
 */
static ioapic_t* g_ioapics = NULL;

acpi_madt_t* g_madt = NULL;

static void lapic_write(size_t reg, uint32_t value) {
    *(volatile uint32_t*)(g_lapic_base + reg) = value;
}

static uint32_t lapic_read(size_t reg) {
    return *(volatile uint32_t*)(g_lapic_base + reg);
}

uint32_t get_lapic_id() {
    return  lapic_read(XAPIC_ID_OFFSET) >> 24u;
}

static uint32_t ioapic_read(ioapic_t* apic, size_t index) {
    POKE8(apic->base + IOAPIC_INDEX_OFFSET) = index;
    return POKE32(apic->base + IOAPIC_DATA_OFFSET);
}

static void ioapic_write(ioapic_t* apic, size_t index, uint32_t value) {
    POKE8(apic->base + IOAPIC_INDEX_OFFSET) = index;
    POKE32(apic->base + IOAPIC_DATA_OFFSET) = value;
}

err_t ioapic_redirect(uint8_t gsi, uint8_t vector, bool level_triggered, bool assertion_level) {
    err_t err = NO_ERROR;

    // TODO: redirect with isos

    // find the correct ioapic
    ioapic_t* ioapic = NULL;
    for (int i = 0; i < arrlen(g_ioapics); i++) {
        if (g_ioapics[i].gsi_start <= gsi && gsi <= g_ioapics[i].gsi_end) {
            ioapic = &g_ioapics[i];
        }
    }
    CHECK_ERROR(ioapic != NULL, ERROR_NOT_FOUND);
    gsi -= ioapic->gsi_start;

    // setup the entry
    ioapic_redir_entry_t entry = {
        .vector = vector,
        .delivery_mode = LAPIC_DELIVERY_MODE_FIXED,
        .polarity = !assertion_level,
        .trigger_mode = level_triggered,
        .destination_id = 0,
    };
    ioapic_write(ioapic, IOAPIC_REDIRECTION_TABLE_ENTRY_INDEX + gsi * 2 + 1, entry.raw_split.high);
    ioapic_write(ioapic, IOAPIC_REDIRECTION_TABLE_ENTRY_INDEX + gsi * 2, entry.raw_split.low);

cleanup:
    return err;
}

err_t init_apic() {
    err_t err = NO_ERROR;

    g_madt = laihost_scan("APIC", 0);
    CHECK_ERROR_TRACE(g_madt != NULL, ERROR_NOT_FOUND, "Could not find APIC ACPI table");

    TRACE("Enabling APIC globally");
    MSR_IA32_APIC_BASE_REGISTER base = { .raw = __rdmsr(MSR_IA32_APIC_BASE) };
    g_lapic_base = PHYSICAL_TO_DIRECT(((uint32_t)base.ApicBase << 12u) | ((uint64_t)base.ApicBaseHi << 32u));
    base.EN = 1;
    __wrmsr(MSR_IA32_APIC_BASE, base.raw);

    TRACE("Iterating IOAPICs");
    FOR_EACH_IN_MADT() {
        if (entry->type != MADT_IOAPIC) continue;
        ioapic_t ioapic = {
                .base = PHYSICAL_TO_DIRECT((physptr_t)entry->ioapic.ioapic_address),
                .gsi_start = entry->ioapic.gsi_base,
        };
        ioapic_version_t version = { .raw = ioapic_read(&ioapic, IOAPIC_VERSION_REGISTER_INDEX) };
        ioapic.gsi_end = ioapic.gsi_start + version.maximum_redirection_entry;
        TRACE("\t#%d: %d-%d", entry->ioapic.ioapic_id, ioapic.gsi_start, ioapic.gsi_end);
                arrpush(g_ioapics, ioapic);
    }

    TRACE("Iterating ISOs");
    FOR_EACH_IN_MADT() {
        if (entry->type != MADT_ISO) continue;
        if (entry->iso.source == entry->iso.gsi) {
            TRACE("\t%d: %s/%s",
                  entry->iso.gsi,
                  entry->iso.flags & BIT2 ? "active high" : "active low",
                  entry->iso.flags & BIT8 ? "edge triggered" : "level triggered");
        } else {
            TRACE("\t%d -> %d: %s/%s",
                    entry->iso.source,
                    entry->iso.gsi,
                    entry->iso.flags & BIT2 ? "active high" : "active low",
                    entry->iso.flags & BIT8 ? "edge triggered" : "level triggered");
        }
    }

cleanup:
    return err;
}

void init_lapic() {
    // get the apic id
    uint32_t id = get_lapic_id();
    TRACE("Configuring LAPIC #%d", id);

    // enable apic
    lapic_svr_t svr = { .raw = lapic_read(XAPIC_SPURIOUS_VECTOR_OFFSET) };
    svr.spurious_vector = 0xff;
    svr.software_enable = 1;
    lapic_write(XAPIC_SPURIOUS_VECTOR_OFFSET, svr.raw);

    // setup the divier to biggest one
    lapic_write(XAPIC_TIMER_DIVIDE_CONFIGURATION_OFFSET, 0);

    // check the amount of ticks passed
    lapic_write(XAPIC_TIMER_INIT_COUNT_OFFSET, 0xFFFFFFFF);
    stall(1000);
    g_lapic_freq = 0xFFFFFFFF - lapic_read(XAPIC_TIMER_CURRENT_COUNT_OFFSET);
}

void send_lapic_eoi() {
    lapic_write(XAPIC_EOI_OFFSET, 0);
}

/**
 * Send an ipi to the given lapic
 */
static void send_ipi(lapic_icr_low_t low, uint8_t lapic_id) {
    critical_t critical;
    enter_critical(&critical);

    // send it
    lapic_write(XAPIC_ICR_HIGH_OFFSET, (uint32_t)lapic_id << 24);
    lapic_write(XAPIC_ICR_LOW_OFFSET, low.raw);

    // wait for finish
    do {
        low.raw = lapic_read(XAPIC_ICR_LOW_OFFSET);
    } while(low.delivery_status != 0);

    exit_critical(&critical);
}

void cpu_send_ipi(size_t cpu_id, uint8_t vector) {
    lapic_icr_low_t icr = {
        .vector = vector,
        .delivery_mode = LAPIC_DELIVERY_MODE_FIXED,
        .level = 1
    };
    send_ipi(icr, cpu_id);
}

/**
 * Send an init ipi to the given lapic
 */
static void send_init_ipi(uint32_t lapic_id) {
    lapic_icr_low_t icr = {
        .delivery_mode = LAPIC_DELIVERY_MODE_INIT,
        .level = 1,
    };
    send_ipi(icr, lapic_id);
}

/**
 * Send a startup ipi to the given lapic
 */
static void send_sipi_ipi(uint32_t lapic_id, uint32_t entry) {
    lapic_icr_low_t icr = {
        .delivery_mode = LAPIC_DELIVERY_MODE_STARTUP,
        .level = 1,
        .vector = entry >> 12
    };
    send_ipi(icr, lapic_id);
}

void per_cpu_entry();

extern char g_smp_trampoline[];
extern size_t g_smp_trampoline_size;

err_t startup_all_cores() {
    err_t err = NO_ERROR;

    // setup everything for starting the cores
    memcpy(PHYSICAL_TO_DIRECT((void*)0x1000), g_smp_trampoline, g_smp_trampoline_size);
    POKE64(SMP_KERNEL_ENTRY) = (uintptr_t)per_cpu_entry;
    POKE64(SMP_KERNEL_PAGE_TABLE) = DIRECT_TO_PHYSICAL(g_kernel.address_space.pml4);
    POKE(idt_t, SMP_KERNEL_IDT) = g_idt;
    POKE(gdt_t, SMP_KERNEL_GDT) = g_gdt;

    // map the data and code
    CHECK_AND_RETHROW(vmm_map(&g_kernel.address_space, 0, 0, MAP_WRITE));
    CHECK_AND_RETHROW(vmm_map(&g_kernel.address_space, (directptr_t)0x1000, 0x1000, MAP_WRITE | MAP_EXEC));

    FOR_EACH_IN_MADT() {
        if (entry->type != MADT_LAPIC) continue;
        if (entry->lapic.apic_id == get_lapic_id() || (!entry->lapic.enabled && !entry->lapic.online_capable)) continue;

        POKE64(SMP_FLAG) = 0;
        POKE64(SMP_STACK_POINTER) = (uintptr_t)alloc_stack();
        // TODO: we need to free this stack somehow

        send_init_ipi(entry->lapic.apic_id);
        stall(10000);
        send_sipi_ipi(entry->lapic.apic_id, 0x1000);
        stall(1000);
        if (POKE64(SMP_FLAG) == 0) {
            send_sipi_ipi(entry->lapic.apic_id, 0x1000);
            stall(1000000);
            CHECK_TRACE(POKE64(SMP_FLAG) != 0, "Failed to initialize core #%u", entry->lapic.apic_id);
        }
    }

    // unmap these
    CHECK_AND_RETHROW(vmm_unmap(&g_kernel.address_space, 0));
    CHECK_AND_RETHROW(vmm_unmap(&g_kernel.address_space, (directptr_t)0x1000));

cleanup:
    return err;
}

void set_next_scheduler_tick(uint64_t ms) {
    lapic_lvt_timer_t timer = {
        .vector = 0x20,
    };
    lapic_write(XAPIC_TIMER_INIT_COUNT_OFFSET, ms * g_lapic_freq);
    lapic_write(XAPIC_TIMER_DIVIDE_CONFIGURATION_OFFSET, 0);
    lapic_write(XAPIC_LVT_TIMER_OFFSET, timer.raw);
}