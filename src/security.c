#include "security.h"

#include "devices.h"
#include "scheduler.h"

#include <stdint.h>

uintptr_t __stack_chk_guard;

struct identity {
    uint32_t uid;
    const char *name;
    uint32_t capabilities;
};

static const struct identity identities[] = {
    {0, "root", 0xFFFFFFFFU},
    {1000, "guest", 0},
};

static const struct identity *current_identity(void)
{
    uint32_t uid = task_current_uid();
    for (uint32_t i = 0; i < sizeof(identities) / sizeof(identities[0]); ++i)
        if (identities[i].uid == uid) return &identities[i];
    return &identities[1];
}

void security_init(void)
{
    security_audit("security manager online");
}

bool security_has_capability(enum capability capability)
{
    return (current_identity()->capabilities & (uint32_t)capability) != 0;
}

const char *security_current_name(void) { return current_identity()->name; }

void security_audit(const char *event)
{
    serial_write("SECURITY: ");
    serial_write(event);
    serial_write("\r\n");
}

void __stack_chk_fail(void)
{
    __asm__ volatile ("cli");
    serial_write("\r\nKERNEL PANIC: stack corruption detected\r\n");
    for (;;) __asm__ volatile ("hlt");
}
