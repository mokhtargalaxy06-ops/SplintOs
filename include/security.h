#ifndef SPLINTOS_SECURITY_H
#define SPLINTOS_SECURITY_H

#include <stdbool.h>
#include <stdint.h>

enum capability {
    CAP_CHANGE_IDENTITY = 1U << 0,
    CAP_CHANGE_PERMISSIONS = 1U << 1,
    CAP_DEVICE_ACCESS = 1U << 2,
    CAP_NETWORK_ADMIN = 1U << 3,
};

void security_init(void);
bool security_has_capability(enum capability capability);
const char *security_current_name(void);
void security_audit(const char *event);
void __stack_chk_fail(void) __attribute__((noreturn));

#endif
