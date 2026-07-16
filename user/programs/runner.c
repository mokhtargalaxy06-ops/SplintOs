#include <splint/syscall.h>

int main(int argument_count, char **arguments)
{
    (void)argument_count;
    (void)arguments;
    static const char path[] = "/bin/cat";
    static const char argument[] = "/README";
    static const char success[] = "runner: child exited status=0\r\n";
    const char *child_arguments[] = {path, argument};
    int child = sys_spawn(path, child_arguments, 2);
    if (child < 0) return 1;
    int status = -1;
    if (sys_wait(child, &status) != child) return 2;
    if (status != 0) return 3;
    return sys_write(1, success, sizeof(success) - 1) < 0 ? 4 : 0;
}
