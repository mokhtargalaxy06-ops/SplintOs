#include <splint/syscall.h>

int main(int argument_count, char **arguments)
{
    (void)argument_count;
    (void)arguments;
    static const char message[] = "dup2: shared descriptor online\r\n";
    if (sys_dup2(1, 5) != 5) return 1;
    if (sys_write(5, message, sizeof(message) - 1) < 0) return 2;
    return sys_close(5) == 0 ? 0 : 3;
}
