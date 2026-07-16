#include <splint/syscall.h>

static int number(unsigned int value)
{
    char digits[10]; size_t count = 0;
    do { digits[count++] = (char)('0' + value % 10); value /= 10; } while (value);
    char output[10];
    for (size_t i = 0; i < count; ++i) output[i] = digits[count - i - 1];
    return sys_write(1, output, count);
}

int main(int argument_count, char **arguments)
{
    (void)argument_count; (void)arguments;
    struct splint_memory_info info;
    if (sys_memory_info(&info) < 0) return 1;
    if (sys_write(1, "memory: total=", 14) < 0 || number(info.total_kib) < 0 ||
        sys_write(1, " KiB free=", 10) < 0 || number(info.free_kib) < 0 ||
        sys_write(1, " KiB\r\n", 6) < 0) return 2;
    return 0;
}
