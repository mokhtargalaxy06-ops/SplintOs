#include <splint/syscall.h>

int main(int argument_count, char **arguments)
{
    (void)argument_count;
    (void)arguments;
    static const char message[] = "hello from ELF user space\r\n";
    return sys_write(1, message, sizeof(message) - 1) < 0 ? 1 : 0;
}
