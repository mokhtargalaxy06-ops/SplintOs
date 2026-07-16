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
    if (sys_yield() != 0) return 4;
    struct splint_clock before_sleep, after_sleep;
    if (sys_clock_get(&before_sleep) != 0 || before_sleep.ticks_per_second != 100 ||
        sys_sleep(100) != 0 || sys_clock_get(&after_sleep) != 0 ||
        (unsigned int)(after_sleep.ticks - before_sleep.ticks) < 100) return 4;
    static const char hello[] = "/bin/hello";
    for (int i = 0; i < 2; ++i)
        if (sys_spawn(hello, NULL, 0) < 0) return 4;
    for (int i = 0; i < 2; ++i) {
        status = -1;
        if (sys_wait(0, &status) <= 0 || status != 0) return 4;
    }
    for (int i = 0; i < 3; ++i) {
        if (sys_spawn(hello, NULL, 0) < 0) return 4;
        for (volatile unsigned int spin = 0; spin < 5000000U; ++spin) {}
    }
    return sys_write(1, success, sizeof(success) - 1) < 0 ? 4 : 0;
}
