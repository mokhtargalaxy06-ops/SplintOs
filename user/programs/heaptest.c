#include <splint/stdlib.h>
#include <splint/syscall.h>

int main(int argument_count, char **arguments)
{
    (void)argument_count; (void)arguments;
    char *memory = splint_malloc(128);
    if (memory == NULL) return 1;
    static const char message[] = "heap: userspace brk allocator online\r\n";
    for (size_t i = 0; i < sizeof(message); ++i) memory[i] = message[i];
    return sys_write(1, memory, sizeof(message) - 1) < 0 ? 2 : 0;
}
