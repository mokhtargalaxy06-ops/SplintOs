#include <splint/syscall.h>

enum { TEST_SIZE = 1300 };
static unsigned char written[TEST_SIZE];
static unsigned char read_back[TEST_SIZE];

int main(int argument_count, char **arguments)
{
    (void)argument_count; (void)arguments;
    static const char name[] = "/disk/user.txt";
    static const char success[] = "disk: multi-sector VFS persistence online\r\n";
    for (size_t i = 0; i < TEST_SIZE; ++i) written[i] = (unsigned char)(i * 37U + 11U);

    int descriptor = sys_open(name, 2 | 4 | 16);
    if (descriptor < 0) { sys_write(1, "disk: open failed\r\n", 19); return 1; }
    if (sys_write(descriptor, written, TEST_SIZE) != TEST_SIZE) {
        sys_write(1, "disk: write failed\r\n", 20); return 1;
    }
    if (sys_fsync(descriptor) != 0) { sys_write(1, "disk: fsync failed\r\n", 20); return 1; }
    if (sys_close(descriptor) != 0) return 1;
    descriptor = sys_open(name, 1);
    if (descriptor < 0 || sys_read(descriptor, read_back, TEST_SIZE) != TEST_SIZE ||
        sys_close(descriptor) != 0) return 2;
    for (size_t i = 0; i < TEST_SIZE; ++i) if (read_back[i] != written[i]) return 3;
    static const char temporary[] = "/disk/remove.txt";
    descriptor = sys_open(temporary, 1 | 2 | 4 | 16);
    if (descriptor < 0 || sys_write(descriptor, "remove", 6) != 6 ||
        sys_truncate(descriptor, 600) != 0 || sys_seek(descriptor, 6) != 0 ||
        sys_read(descriptor, read_back, 594) != 594 ||
        sys_close(descriptor) != 0 || sys_unlink(temporary) != 0 ||
        sys_open(temporary, 1) >= 0) return 4;
    for (size_t i = 0; i < 594; ++i) if (read_back[i] != 0) return 4;
    descriptor = sys_open(temporary, 2 | 4);
    if (descriptor < 0 || sys_write(descriptor, "reused", 6) != 6 ||
        sys_close(descriptor) != 0) return 5;
    static const char renamed[] = "/disk/renamed.txt";
    descriptor = sys_open(renamed, 2 | 4 | 16);
    if (descriptor < 0 || sys_write(descriptor, "oldold", 6) != 6 ||
        sys_close(descriptor) != 0) return 5;
    if (sys_rename(temporary, renamed) != 0 || sys_open(temporary, 1) >= 0) return 5;
    descriptor = sys_open(renamed, 1);
    char renamed_data[6];
    if (descriptor < 0 || sys_read(descriptor, renamed_data, sizeof(renamed_data)) != 6 ||
        sys_close(descriptor) != 0 || sys_unlink(renamed) != 0) return 5;
    for (size_t i = 0; i < 6; ++i) if (renamed_data[i] != "reused"[i]) return 5;
    static const char *const fill[] = {
        "/disk/f0", "/disk/f1", "/disk/f2", "/disk/f3",
        "/disk/f4", "/disk/f5", "/disk/overflow"
    };
    for (size_t i = 0; i < 6; ++i) {
        descriptor = sys_open(fill[i], 2 | 4);
        if (descriptor < 0 || sys_close(descriptor) != 0) return 6;
    }
    if (sys_open(fill[6], 2 | 4) >= 0) return 6;
    for (size_t i = 0; i < 6; ++i) if (sys_unlink(fill[i]) != 0) return 6;
    descriptor = sys_open(fill[6], 2 | 4);
    if (descriptor < 0 || sys_close(descriptor) != 0 || sys_unlink(fill[6]) != 0)
        return 6;
    return sys_write(1, success, sizeof(success) - 1) < 0 ? 4 : 0;
}
