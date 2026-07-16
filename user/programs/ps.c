#include <splint/syscall.h>

static size_t append_number(char *output, size_t position, unsigned int value)
{
    char digits[10]; size_t count = 0;
    do { digits[count++] = (char)('0' + value % 10); value /= 10; } while (value);
    for (size_t i = 0; i < count; ++i) output[position++] = digits[count - i - 1];
    return position;
}

int main(int argument_count, char **arguments)
{
    (void)argument_count; (void)arguments;
    struct splint_process_info info;
    if (sys_process_info(&info) < 0 || sys_getuid() != 0) return 1;
    char output[64] = "processes: active="; size_t position = 18;
    position = append_number(output, position, info.process_count);
    output[position++] = ' '; output[position++] = 'p'; output[position++] = 'i';
    output[position++] = 'd'; output[position++] = '=';
    position = append_number(output, position, info.current_pid);
    output[position++] = '\r'; output[position++] = '\n';
    return sys_write(1, output, position) < 0 ? 2 : 0;
}
