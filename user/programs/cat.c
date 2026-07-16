#include <splint/syscall.h>

int main(int argument_count, char **arguments)
{
    static const char default_path[] = "/README";
    const char *path = argument_count > 1 ? arguments[1] : default_path;
    char buffer[64];
    int descriptor = sys_open(path, SPLINT_READ);
    if (descriptor < 0) return 1;
    for (;;) {
        int count = sys_read(descriptor, buffer, sizeof(buffer));
        if (count < 0) { (void)sys_close(descriptor); return 2; }
        if (count == 0) break;
        if (sys_write(1, buffer, (size_t)count) != count) {
            (void)sys_close(descriptor);
            return 3;
        }
    }
    return sys_close(descriptor) == 0 ? 0 : 4;
}
