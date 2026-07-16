#include <splint/stdlib.h>
#include <splint/syscall.h>

#include <stdbool.h>
#include <stdint.h>

int main(int argument_count, char **arguments)
{
    (void)argument_count; (void)arguments;
    char *memory = splint_malloc(128);
    if (memory == NULL) return 1;
    char *neighbor = splint_malloc(64);
    if (neighbor == NULL) return 2;
    char *guard = splint_malloc(32);
    if (guard == NULL) return 2;
    splint_free(memory);
    char *reused = splint_malloc(48);
    if (reused != memory) return 3;
    splint_free(neighbor);
    splint_free(reused);
    char *coalesced = splint_malloc(160);
    if (coalesced != memory) return 4;
    char *tail = splint_malloc(5000);
    if (tail == NULL) return 5;
    void *before_release = sys_brk(NULL);
    splint_free(tail);
    if ((uintptr_t)sys_brk(NULL) >= (uintptr_t)before_release) return 6;
    guard[0] = 1;
    uint8_t *zeroed = splint_calloc(32, 4);
    if (zeroed == NULL) return 7;
    for (size_t i = 0; i < 128; ++i) if (zeroed[i] != 0) return 8;
    for (size_t i = 0; i < 128; ++i) zeroed[i] = (uint8_t)i;
    zeroed = splint_realloc(zeroed, 512);
    if (zeroed == NULL) return 9;
    for (size_t i = 0; i < 128; ++i) if (zeroed[i] != (uint8_t)i) return 10;
    char log[512];
    int log_size = sys_log_read(log, sizeof(log));
    if (log_size <= 0) return 11;
    bool found = false;
    static const char marker[] = "SplintOS";
    for (int i = 0; i + (int)sizeof(marker) - 1 <= log_size; ++i) {
        size_t j = 0;
        while (j + 1 < sizeof(marker) && log[i + (int)j] == marker[j]) ++j;
        if (j + 1 == sizeof(marker)) { found = true; break; }
    }
    if (!found) return 12;
    static const char message[] = "heap: allocation reuse and coalescing online\r\n";
    for (size_t i = 0; i < sizeof(message); ++i) coalesced[i] = message[i];
    return sys_write(1, coalesced, sizeof(message) - 1) < 0 ? 7 : 0;
}
