#include <splint/stdlib.h>
#include <splint/syscall.h>

#include <stdint.h>

static uintptr_t current_break;

void *splint_malloc(size_t size)
{
    if (size == 0) return NULL;
    size = (size + 7U) & ~7U;
    if (current_break == 0) current_break = (uintptr_t)sys_brk(NULL);
    if (current_break == (uintptr_t)-1 || size > 0x90000000U - current_break)
        return NULL;
    uintptr_t previous = current_break;
    uintptr_t requested = current_break + size;
    if ((uintptr_t)sys_brk((void *)requested) != requested) return NULL;
    current_break = requested;
    return (void *)previous;
}
