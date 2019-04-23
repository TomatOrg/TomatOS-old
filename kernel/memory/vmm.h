#ifndef TOMATKERNEL_MEMORY_VMM
#define TOMATKERNEL_MEMORY_VMM

////////////////////////////////////////////////////////////////////////////
// Virtual Memory Manager
////////////////////////////////////////////////////////////////////////////
//
// The virtual memory manager manages the paging, it can allocate new pages
// as well as create and map physical address of different address spaces
//
////////////////////////////////////////////////////////////////////////////

#include <common/stdint.h>
#include <common/multiboot.h>
#include <common/stdbool.h>


///-------------------------------------------------------------------------
// Address spaces
///-------------------------------------------------------------------------

/**
 * This notates an address space
 *
 * internally this is the pml4 address
 */
typedef void* address_space_t;

/**
 * This is the boot address space, it should have 4GB identity mapping
 * of the first 4GB of memory. this is mostly used until the vmm is created
 * so the kernel can still do stuff in long mode.
 */
extern address_space_t boot_address_space;

/**
 * This is the kernel's address space, it is located here so it can be easily accessed
 */
extern address_space_t kernel_address_space;


///-------------------------------------------------------------------------
// Initialization functions
///-------------------------------------------------------------------------

/**
 * This will do early paging initialization
 *
 * it will basically set the boot_address_space and identity map the first 8GB
 */
void vmm_early_init();

/**
 * This will initialize the virtual memory manager
 *
 * it will create the allocated pages bitmap. it will also create the kernel address space.
 * by the end of this function we will be in the kernel address space and will be able to map and allocate
 * pages for the kernel address space.
 */
void vmm_init(multiboot_info_t* info);

/**
 * Will set the current address space
 * @param addrspace the address space
 */
extern void vmm_set(address_space_t addrspace);

/**
 * Will get the current address space
 * @return The current address space
 */
extern address_space_t vmm_get();

/**
 * Will create a new address space from the current address space.
 */
address_space_t vmm_create_address_space();

/**
 * Free the address space, clearing all the pages in it
 *
 * Note:    Make sure that the current address space is not the one being freed, otherwise bad stuff will happen!
 */
void vmm_free_address_space(address_space_t address_space);

///-------------------------------------------------------------------------
// Manipulation functions
///-------------------------------------------------------------------------

/**
 * Attributes that can be modified in the page attributes
 */
#define PAGE_ATTR_WRITE (int)(1u << 0u)
#define PAGE_ATTR_USER (int)(1u << 1u)
#define PAGE_ATTR_EXECUTE (int)(1u << 2u)

/**
 * Map a virtual address to the physical address with the given page attributes
 */
void vmm_map(address_space_t address_space, void* virtual_addr, void* physical_addr, int attributes);

/**
 * Will unmap the given virtual address, keeping the underlying physical page allocated
 */
void vmm_unmap(address_space_t address_space, void* virtual_addr);

/**
 * Will allocate a page for the virtual address with the given attributes
 */
void vmm_allocate(address_space_t address_space, void* virtual_addr, int attributes);

/**
 * Free the given virtual address, both unmapping it and freeing the underlying physical page
 */
void vmm_free(address_space_t address_space, void* virtual_addr);

#endif