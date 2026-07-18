#ifndef SPLINTOS_ARCH_X86_64_PAGING_H
#define SPLINTOS_ARCH_X86_64_PAGING_H

#include <stdint.h>

int x86_64_paging_init(void);
int x86_64_paging_user_accessible(uint64_t address, uint64_t length,
                                  int writable);

#endif
