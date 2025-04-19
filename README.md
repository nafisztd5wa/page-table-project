# page-table-project


Multi-level Page Table Implementation

Configuration Guide (config.h)

I've designed this system to be configured through two key parameters in config.h:

```c
#define LEVELS  3    // Number of page table levels
#define POBITS  12   // Number of page offset bits
```

Choosing POBITS:
POBITS determines the page size (2^POBITS bytes). Here are the common values and what I've found works best:

When setting POBITS to 12, you'll get 4KB pages, which is the standard in most systems. This configuration offers the highest compatibility with real hardware and strikes an excellent balance between granularity and overhead. With this setting, each page table can hold 512 entries, which I've found to be optimal for caching performance.

For larger memory requirements, setting POBITS to 16 gives you 64KB pages. This configuration significantly reduces page table overhead and performs exceptionally well for large sequential memory access patterns. However, be aware that it may lead to internal fragmentation when dealing with small allocations.

Valid range: 
The valid range for POBITS spans from 4 to 18. The minimum value of 4 ensures that pages are at least 16 bytes and each table can hold a minimum of 2 entries. I've capped the maximum at 18 to prevent page sizes from exceeding 256KB, which could lead to impractical memory usage patterns.

System impacts:
* Page size = 2^POBITS bytes (affects memory granularity)
* Entries per table = 2^(POBITS-3) (due to 8-byte entries)
* Table size = 2^POBITS bytes (each table fills one page)
* Memory alignment requirements = 2^POBITS

Choosing LEVELS:
LEVELS determines page table hierarchy depth:

Common values and use cases:
* A single-level configuration (LEVELS=1) provides the simplest possible implementation. This setup is ideal for small address spaces, offering direct mapping and the fastest possible translation times. However, it comes with a limitation of 2^(POBITS-3) total pages, making it suitable only for specific use cases.
* The two-level configuration (LEVELS=2) mirrors early x86 architectures and provides a good middle ground. It is particularly well-suited for systems with address spaces up to a few gigabytes. This configuration has proven especially effective in embedded systems where resource constraints are a primary concern.
* For modern systems, the four-level configuration (LEVELS=4) follows the x86_64 approach. This setup excels at handling huge address spaces and manages sparse allocation patterns with notable efficiency. It has become the standard in contemporary systems due to its versatility and scalability.

I've set the valid range from 1 to 6, based on practical constraints:
* Minimum 1: Simplest possible page table
* Maximum 6: Limited by address bit count constraint

Performance implications:
* Through testing, I have observed that increasing the number of levels expands the address space capacity but adds one memory access per level. However, higher levels also enable more sophisticated memory sharing capabilities.

Configuration Constraints
* Each page table level handles 9 bits of the virtual address (POBITS - 3, since we need 9 bits to index 512 entries in a 4KB page). The extract_level_index() function handles pulling out these 9-bit chunks for each level. For example, with POBITS=12 and LEVELS=4, each level extracts its own 9 bits from the virtual address to find the right entry.

```c
#define BITS_PER_LEVEL (POBITS - 3)
#define ENTRIES_PER_TABLE (PAGE_SIZE / sizeof(size_t))
```

Library Usage Guide

Basic Usage Examples:
```c
// Initialize and allocate a page
#include "mlpt.h"

// Example 1: Basic page allocation and access
size_t virtual_addr = 0x1000;
page_allocate(virtual_addr);
size_t physical_addr = translate(virtual_addr);

// Example 2: Working with page offsets
size_t addr_with_offset = virtual_addr | 0xFF;
size_t phys_with_offset = translate(addr_with_offset);

// Example 3: Deallocation
if (page_deallocate(virtual_addr)) {
    // the deallocation was successful
} else {
    // the page wasn't allocated
}
```

Implementation Details

Memory Management Structure:
* Each page table entry occupies 8 bytes (size_t) and follows a specific bit allocation: The least significant bit serves as a valid flag, with 1 indicating a valid entry and 0 indicating invalid. Bits 1 through 11 are currently reserved for future extensions and functionality. The physical page number occupies bits 12 through 63, providing extensive addressing capabilities. This structure is preserved through PAGE_MASK and ENTRY_TO_PHYS_ADDR operations, which carefully handle the valid flag and physical address bits while maintaining the reserved bits for future use. To maintain proper alignment and efficient memory access, all the pages have to align to the PAGE_SIZE boundary.

Performance Analysis

Time Complexity:
1. translate():
   * O(LEVELS) - must traverse each level
   * Consistent performance for all valid addresses

2. page_allocate():
   * The performance depends on the existing state of the page tables
   * Best case: O(LEVELS) when all tables exist
   * Worst case: O(LEVELS) with additional allocation time
   * Memory zeroing: O(PAGE_SIZE) per allocation

Space Complexity:
- Each page table requires 2^POBITS bytes of memory, with a minimum requirement of one root table. In the worst case, memory usage scales as O(LEVELS * N) for N pages.
- In a typcial case, it will be lower due to table sharing

Known Limitations:

The current implementation has several areas for potential enhancement. In terms of memory features, the system currently lacks support for huge pages, page protection flags, and shared page capabilities. Performance considerations include the lack of implementation relying on sequential table traversal. The error handling system is also relatively basic, with limited error reporting and validation capabilities. The error handling system mainly uses return values to indicate success/failure.

Future Improvements that could be made:

1. Memory Management:
   * Adding page protection flags
   * Implement support for huge pages and shared page capabilities

2. Performance:
   * Adding TLB simulation
   * Implement caching
   * Optimize table traversal

Memory Deallocation Interface:

* I've implemented a deallocation interface that allows for the removal of pages and cleanup of page tables. The interface is designed to work seamlessly with the existing page_allocate and translate functions without requiring any modifications to their implementation.

```c
int page_deallocate(size_t va);
```
* This function takes a virtual address as input and returns 1 if the deallocation was successful, and 0 if the page wasn't mapped (indicating nothing was deallocated).

This implementation handles several key aspects of memory management:
* The function performs page table traversal by walking through the page table hierarchy to locate and deallocate the specified page. During this process, it maintains a careful record of the path taken through the tables to ensure accurate cleanup later.
* Memory cleanup goes beyond removing page mappings. The system performs a cleanup process starting by deallocating the physical page at the target virtual address. It then recursively checks and removes any empty intermediate page tables, while maintaining the integrity of the overall page table structure throughout the cleanup process.
* The system implements empty table detection. When a page is deallocated, the system checks the remaining structure to identify and remove any page tables that have become empty. This approach prevents memory leaks that could happen from abandoned table structures.

Example usage:

```c
// Allocate a page
size_t virtual_addr = 0x1000;
page_allocate(virtual_addr);

// Use the page...

// Deallocate when no longer needed
if (page_deallocate(virtual_addr)) {
    printf("Page successfully deallocated\n");
} else {
    printf("Page was not allocated\n");
}
```
Deallocation Implementation Notes:

* The deallocation system works with the existing page table structure without needing to change page_allocate or translate
* It maintains the same page size and alignment requirements as the allocation system
* The implementation includes safeguards against invalid deallocations and multiple frees
* Empty page table cleanup is performed automatically to prevent memory leaks

