#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mlpt.h"
#include "config.h"

/* System-wide constants for page table configuration */
#define PAGE_SIZE (1UL << POBITS)
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define ENTRIES_PER_TABLE (PAGE_SIZE / sizeof(size_t))
#define VALID_FLAG 1UL
#define ENTRY_TO_PHYS_ADDR(pte) ((pte) & PAGE_MASK)

/* Virtual address parsing constants */
#define BITS_PER_LEVEL (POBITS - 3)
#define LEVEL_INDEX_MASK ((1UL << BITS_PER_LEVEL) - 1)

/* Global page table base register */
size_t ptbr = 0;

/**
 * Extract the index for a specific page table level from a virtual address
 */
static size_t extract_level_index(size_t virtual_addr, int level) {
    size_t shift_amount = POBITS + (level * BITS_PER_LEVEL);
    return (virtual_addr >> shift_amount) & LEVEL_INDEX_MASK;
}

/**
 * Allocate a new page-aligned table or page
 */
static size_t* allocate_aligned_page(void) {
    void* new_page = NULL;
    if (posix_memalign(&new_page, PAGE_SIZE, PAGE_SIZE) != 0) {
        return NULL;
    }
    memset(new_page, 0, PAGE_SIZE);
    return new_page;
}

/**
 * Check if the virtual address is within valid bounds
 */
static int is_address_valid(size_t virtual_addr) {
    for (int level = LEVELS - 1; level >= 0; level -= 1) {
        if (extract_level_index(virtual_addr, level) >= ENTRIES_PER_TABLE) {
            return 0;
        }
    }
    return 1;
}

size_t translate(size_t virtual_addr) {
    if (ptbr == 0) {
        return ~0UL;
    }

    size_t* current_table = (size_t*)ptbr;
    size_t page_offset = virtual_addr & (PAGE_SIZE - 1);
    size_t table_index;
    size_t table_entry;

    /* Walk through page table levels */
    for (int level = LEVELS - 1; level >= 0; level -= 1) {
        table_index = extract_level_index(virtual_addr, level);
        if (table_index >= ENTRIES_PER_TABLE) {
            return ~0UL;
        }

        table_entry = current_table[table_index];
        if (!(table_entry & VALID_FLAG)) {
            return ~0UL;
        }

        if (level == 0) {
            return ENTRY_TO_PHYS_ADDR(table_entry) | page_offset;
        }
        
        current_table = (size_t*)ENTRY_TO_PHYS_ADDR(table_entry);
    }

    return ~0UL;
}

void page_allocate(size_t virtual_addr) {
    /* Initial validation */
    if (!is_address_valid(virtual_addr)) {
        return;
    }

    /* Initialize root page table if needed */
    if (ptbr == 0) {
        size_t* root_table = allocate_aligned_page();
        if (root_table == NULL) {
            return;
        }
        ptbr = (size_t)root_table;
    }

    size_t* current_table = (size_t*)ptbr;
    size_t table_index;

    /* Traverse and allocate intermediate page tables */
    for (int level = LEVELS - 1; level > 0; level -= 1) {
        table_index = extract_level_index(virtual_addr, level);
        
        if (!(current_table[table_index] & VALID_FLAG)) {
            size_t* new_table = allocate_aligned_page();
            if (new_table == NULL) {
                return;
            }
            current_table[table_index] = ((size_t)new_table & PAGE_MASK) | VALID_FLAG;
        }
        
        current_table = (size_t*)ENTRY_TO_PHYS_ADDR(current_table[table_index]);
    }

    /* Allocate final page if needed */
    table_index = extract_level_index(virtual_addr, 0);
    if (!(current_table[table_index] & VALID_FLAG)) {
        size_t* new_page = allocate_aligned_page();
        if (new_page == NULL) {
            return;
        }
        current_table[table_index] = ((size_t)new_page & PAGE_MASK) | VALID_FLAG;
    }
}

/**
 * Deallocate a page and clean up empty page tables
 */
int page_deallocate(size_t virtual_addr) {
    if (ptbr == 0) {
        return 0;
    }

    /* Store the path through the page tables */
    size_t* tables[LEVELS];
    size_t indices[LEVELS];
    size_t* current_table = (size_t*)ptbr;
    
    /* Validate the path and store it */
    for (int level = LEVELS - 1; level >= 0; level -= 1) {
        tables[level] = current_table;
        indices[level] = extract_level_index(virtual_addr, level);
        
        if (!(current_table[indices[level]] & VALID_FLAG)) {
            return 0;
        }
        
        if (level > 0) {
            current_table = (size_t*)ENTRY_TO_PHYS_ADDR(
                current_table[indices[level]]
            );
        }
    }
    
    /* Clear the leaf page table entry */
    tables[0][indices[0]] = 0;
    
    /* Clean up empty intermediate tables */
    for (int level = 1; level < LEVELS; level += 1) {
        int table_empty = 1;
        
        for (size_t i = 0; i < ENTRIES_PER_TABLE; i += 1) {
            if (tables[level - 1][i] & VALID_FLAG) {
                table_empty = 0;
                break;
            }
        }
        
        if (table_empty) {
            free(tables[level - 1]);
            tables[level][indices[level]] = 0;
        } else {
            break;
        }
    }
    
    return 1;
}