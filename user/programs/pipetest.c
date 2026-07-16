#include <splint/syscall.h>

int main(int argument_count, char **arguments)
{
    (void)argument_count;
    (void)arguments;
    static const char echo_path[] = "/bin/echo";
    static const char payload[] = "child output crossed a pipe";
    static const char success[] = "pipe: blocking transfer and EOF online\r\n";
    const char *child_arguments[] = {echo_path, payload};
    if (sys_dup2(1, 5) != 5) return 1;
    int pipe_descriptors[2];
    if (sys_pipe(pipe_descriptors) != 0) return 2;
    if (sys_dup2(pipe_descriptors[1], 1) != 1) return 3;
    if (sys_close(pipe_descriptors[1]) != 0) return 4;
    int child = sys_spawn(echo_path, child_arguments, 2);
    if (child < 0) return 5;
    if (sys_dup2(5, 1) != 1 || sys_close(5) != 0) return 6;
    char buffer[64];
    int count = sys_read(pipe_descriptors[0], buffer, sizeof(buffer));
    if (count <= 0 || sys_write(1, buffer, (size_t)count) != count) return 7;
    int status;
    if (sys_wait(child, &status) != child || status != 0) return 8;
    for (;;) {
        count = sys_read(pipe_descriptors[0], buffer, sizeof(buffer));
        if (count < 0) return 9;
        if (count == 0) break;
        if (sys_write(1, buffer, (size_t)count) != count) return 10;
    }
    if (sys_close(pipe_descriptors[0]) != 0) return 10;
    return sys_write(1, success, sizeof(success) - 1) < 0 ? 11 : 0;
}
