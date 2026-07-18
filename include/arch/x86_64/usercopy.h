#ifndef SPLINTOS_ARCH_X86_64_USERCOPY_H
#define SPLINTOS_ARCH_X86_64_USERCOPY_H

#include <stddef.h>
#include <stdint.h>

int x86_64_copy_from_user(void *kernel_destination, uint64_t user_source,
                          size_t length);
int x86_64_copy_to_user(uint64_t user_destination, const void *kernel_source,
                        size_t length);
int x86_64_usercopy_conformance_test(void);

#endif
