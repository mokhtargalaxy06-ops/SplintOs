#include <splint/syscall.h>

int main(int argument_count, char **arguments)
{
    const char *path = argument_count > 1 ? arguments[1] : "/";
    struct splint_directory_entry entries[32];
    int count = sys_list(path, entries, 32);
    if (count < 0) return 1;
    for (int i = 0; i < count; ++i) {
        size_t length = 0;
        while (entries[i].name[length] != '\0') ++length;
        if (sys_write(1, entries[i].name, length) < 0 ||
            sys_write(1, "\r\n", 2) < 0) return 2;
    }
    return 0;
}
