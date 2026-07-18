#include <splint/syscall.h>

int main(int argument_count, char **arguments)
{
    for (int i = 1; i < argument_count; ++i) {
        size_t length = 0;
        while (arguments[i][length] != '\0') ++length;
        if (sys_write(1, arguments[i], length) < 0) return 1;
        if (i + 1 < argument_count && sys_write(1, " ", 1) < 0) return 1;
    }
    return sys_write(1, "\r\n", 2) < 0 ? 1 : 0;
}
