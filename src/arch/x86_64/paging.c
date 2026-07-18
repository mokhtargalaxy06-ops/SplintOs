#include <stddef.h>
#include <stdint.h>

#include "arch/x86_64/layout.h"
#include "arch/x86_64/paging.h"

#define ALIGNED(value) __attribute__((aligned(value)))

enum {
    ENTRY_PRESENT = 1U << 0,
    ENTRY_WRITABLE = 1U << 1,
    ENTRY_USER = 1U << 2,
    ENTRY_LARGE = 1U << 7,
    ENTRY_COUNT = 512,
};

#define ENTRY_ADDRESS_MASK UINT64_C(0x000ffffffffff000)

static uint64_t final_pml4[ENTRY_COUNT] ALIGNED(4096);
static uint64_t final_low_pdpt[ENTRY_COUNT] ALIGNED(4096);
static uint64_t final_high_pdpt[ENTRY_COUNT] ALIGNED(4096);
static uint64_t final_identity_pd[4][ENTRY_COUNT] ALIGNED(4096);

static void clear_table(uint64_t table[ENTRY_COUNT])
{
    for (size_t i = 0; i < ENTRY_COUNT; ++i) table[i] = 0;
}

static uint16_t pml4_index(uint64_t address)
{ return (uint16_t)((address >> 39) & 0x1ffU); }
static uint16_t pdpt_index(uint64_t address)
{ return (uint16_t)((address >> 30) & 0x1ffU); }
static uint16_t pd_index(uint64_t address)
{ return (uint16_t)((address >> 21) & 0x1ffU); }
static uint16_t pt_index(uint64_t address)
{ return (uint16_t)((address >> 12) & 0x1ffU); }

static int entry_allows(uint64_t entry, int writable)
{
    uint64_t required = ENTRY_PRESENT | ENTRY_USER;
    if (writable) required |= ENTRY_WRITABLE;
    return (entry & required) == required;
}

static int user_page_accessible(uint64_t address, int writable)
{
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    const uint64_t *pml4 = (const uint64_t *)(uintptr_t)(cr3 & ENTRY_ADDRESS_MASK);
    uint64_t pml4e = pml4[pml4_index(address)];
    if (!entry_allows(pml4e, writable)) return 0;
    const uint64_t *pdpt =
        (const uint64_t *)(uintptr_t)(pml4e & ENTRY_ADDRESS_MASK);
    uint64_t pdpte = pdpt[pdpt_index(address)];
    if (!entry_allows(pdpte, writable)) return 0;
    if ((pdpte & ENTRY_LARGE) != 0) return 1;
    const uint64_t *pd =
        (const uint64_t *)(uintptr_t)(pdpte & ENTRY_ADDRESS_MASK);
    uint64_t pde = pd[pd_index(address)];
    if (!entry_allows(pde, writable)) return 0;
    if ((pde & ENTRY_LARGE) != 0) return 1;
    const uint64_t *pt =
        (const uint64_t *)(uintptr_t)(pde & ENTRY_ADDRESS_MASK);
    return entry_allows(pt[pt_index(address)], writable);
}

int x86_64_paging_user_accessible(uint64_t address, uint64_t length,
                                  int writable)
{
    if (length == 0) return 1;
    if (!x86_64_user_range_valid(address, length)) return 0;
    uint64_t last = address + length - 1U;
    uint64_t page = address & ~(X86_64_PAGE_SIZE - 1U);
    for (;;) {
        if (!user_page_accessible(page, writable)) return 0;
        if (page >= (last & ~(X86_64_PAGE_SIZE - 1U))) return 1;
        page += X86_64_PAGE_SIZE;
    }
}

int x86_64_paging_init(void)
{
    clear_table(final_pml4);
    clear_table(final_low_pdpt);
    clear_table(final_high_pdpt);
    for (size_t table = 0; table < 4; ++table) {
        clear_table(final_identity_pd[table]);
        for (uint64_t i = 0; i < ENTRY_COUNT; ++i)
            final_identity_pd[table][i] =
                ((uint64_t)table * ENTRY_COUNT + i) * X86_64_LARGE_PAGE_SIZE |
                ENTRY_PRESENT | ENTRY_WRITABLE | ENTRY_LARGE;
    }

    uint64_t table_flags = ENTRY_PRESENT | ENTRY_WRITABLE;
    for (size_t table = 0; table < 4; ++table)
        final_low_pdpt[table] = (uintptr_t)final_identity_pd[table] | table_flags;
    final_pml4[0] = (uintptr_t)final_low_pdpt | table_flags;
    uint16_t high_root = pml4_index(X86_64_KERNEL_BASE);
    uint16_t high_pdpt = pdpt_index(X86_64_KERNEL_BASE);
    final_high_pdpt[high_pdpt] = (uintptr_t)final_identity_pd[0] | table_flags;
    final_pml4[high_root] = (uintptr_t)final_high_pdpt | table_flags;

    if (!x86_64_address_is_canonical(X86_64_KERNEL_BASE) ||
        !x86_64_address_is_canonical(X86_64_DIRECT_MAP_BASE) ||
        x86_64_address_is_canonical(UINT64_C(0x0000800000000000)) ||
        x86_64_user_range_valid(X86_64_USER_MAX - 1U, 4) ||
        (final_pml4[high_root] & ENTRY_PRESENT) == 0 ||
        (final_high_pdpt[high_pdpt] & ENTRY_PRESENT) == 0) return 0;

    __asm__ volatile ("mov %0, %%cr3" : : "r"((uintptr_t)final_pml4) : "memory");
    return 1;
}
