#include <splint/syscall.h>

int main(int argument_count, char **arguments)
{
    (void)argument_count; (void)arguments;
    struct splint_uname identity;
    if (sys_uname(&identity) != 0 || identity.system[0] != 'S' ||
        identity.machine[0] != 'i') return 1;
    unsigned int value = sys_uptime();
    char digits[10]; size_t count = 0;
    do { digits[count++] = (char)('0' + value % 10); value /= 10; } while (value);
    char output[32] = "uptime: "; size_t position = 8;
    for (size_t i = 0; i < count; ++i) output[position++] = digits[count - i - 1];
    output[position++] = ' '; output[position++] = 's';
    output[position++] = '\r'; output[position++] = '\n';
    return sys_write(1, output, position) < 0 ? 1 : 0;
}
