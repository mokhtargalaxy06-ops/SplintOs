#include <splint/syscall.h>

int main(int argument_count, char **arguments)
{
    (void)argument_count;
    (void)arguments;
    char buffer[64];
    unsigned int total = 0;
    for (;;) {
        int count = sys_read(0, buffer, sizeof(buffer));
        if (count < 0) return 1;
        if (count == 0) break;
        total += (unsigned int)count;
    }
    char output[32] = "wc: bytes=";
    size_t prefix = 10;
    char digits[10];
    size_t digit_count = 0;
    do {
        digits[digit_count++] = (char)('0' + total % 10);
        total /= 10;
    } while (total != 0);
    for (size_t i = 0; i < digit_count; ++i)
        output[prefix + i] = digits[digit_count - i - 1];
    output[prefix + digit_count] = '\r';
    output[prefix + digit_count + 1] = '\n';
    return sys_write(1, output, prefix + digit_count + 2) < 0 ? 2 : 0;
}
