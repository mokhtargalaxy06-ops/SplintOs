#include <stddef.h>
#include <stdint.h>

#include "arch/x86_64/layout.h"
#include "arch/x86_64/paging.h"
#include "arch/x86_64/usercopy.h"

static void copy_bytes(uint8_t *destination, const uint8_t *source,
                       size_t length)
{
    for (size_t i = 0; i < length; ++i) destination[i] = source[i];
}

int x86_64_copy_from_user(void *kernel_destination, uint64_t user_source,
                          size_t length)
{
    if (length == 0) return 1;
    if (kernel_destination == NULL ||
        !x86_64_paging_user_accessible(user_source, length, 0)) return 0;
    copy_bytes(kernel_destination,
               (const uint8_t *)(uintptr_t)user_source, length);
    return 1;
}

int x86_64_copy_to_user(uint64_t user_destination, const void *kernel_source,
                        size_t length)
{
    if (length == 0) return 1;
    if (kernel_source == NULL ||
        !x86_64_paging_user_accessible(user_destination, length, 1)) return 0;
    copy_bytes((uint8_t *)(uintptr_t)user_destination, kernel_source, length);
    return 1;
}

int x86_64_usercopy_conformance_test(void)
{
    uint8_t destination = 0x5a;
    const uint8_t source[4] = { 0xa5, 0x5a, 0xc3, 0x3c };
    if (!x86_64_copy_from_user(NULL, 0, 0) ||
        !x86_64_copy_to_user(0, NULL, 0)) return 0;
    if (x86_64_copy_from_user(&destination, 0, 1) ||
        x86_64_copy_from_user(&destination, X86_64_KERNEL_BASE, 1) ||
        x86_64_copy_from_user(&destination,
                              UINT64_C(0x0000800000000000), 1) ||
        x86_64_copy_from_user(&destination, X86_64_USER_MAX - 1U, 4) ||
        x86_64_copy_from_user(&destination, X86_64_USER_MIN, 1) ||
        destination != 0x5a) return 0;
    if (x86_64_copy_to_user(0, source, 1) ||
        x86_64_copy_to_user(X86_64_KERNEL_BASE, source, 1) ||
        x86_64_copy_to_user(UINT64_C(0x0000800000000000), source, 1) ||
        x86_64_copy_to_user(X86_64_USER_MAX, source, 2) ||
        x86_64_copy_to_user(X86_64_USER_MIN, source, 1)) return 0;
    return 1;
}
