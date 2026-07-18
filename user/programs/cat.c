#include <splint/syscall.h>

static void write_text(const char *text)
{
    size_t length = 0;
    while (text[length] != '\0') ++length;
    (void)sys_write(2, text, length);
}

int main(int argument_count, char **arguments)
{
    static const char default_path[] = "/README";
    const char *path = argument_count > 1 ? arguments[1] : default_path;
    char buffer[64];
    int descriptor = sys_open(path, SPLINT_READ);
    if (descriptor < 0) {
        write_text("cat: cannot open file\r\n");
        return 1;
    }
    for (;;) {
        int count = sys_read(descriptor, buffer, sizeof(buffer));
        if (count < 0) {
            write_text("cat: file read failed\r\n");
            (void)sys_close(descriptor);
            return 2;
        }
        if (count == 0) break;
        if (sys_write(1, buffer, (size_t)count) != count) {
            write_text("cat: output failed\r\n");
            (void)sys_close(descriptor);
            return 3;
        }
    }
    return sys_close(descriptor) == 0 ? 0 : 4;
}
