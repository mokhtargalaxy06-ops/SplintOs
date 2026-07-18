#include <stddef.h>
#include <stdint.h>

#include "arch/x86_64/heap.h"

#define ALIGNED(value) __attribute__((aligned(value)))

enum { HEAP_SIZE = 1024 * 1024, ALIGNMENT = 16 };
static uint8_t storage[HEAP_SIZE] ALIGNED(ALIGNMENT);

struct block {
    size_t size;
    struct block *next;
    uint64_t magic;
    uint8_t free;
};

static const uint64_t block_magic = UINT64_C(0x53504C4845415031);
static struct block *head;

static size_t align_size(size_t size)
{
    if (size > SIZE_MAX - (ALIGNMENT - 1U)) return 0;
    return (size + ALIGNMENT - 1U) & ~(size_t)(ALIGNMENT - 1U);
}

void *x86_64_heap_alloc(size_t requested)
{
    size_t size = align_size(requested);
    if (size == 0) return NULL;
    for (struct block *block = head; block != NULL; block = block->next) {
        if (!block->free || block->magic != block_magic || block->size < size) continue;
        if (block->size - size >= sizeof(struct block) + ALIGNMENT) {
            struct block *tail = (struct block *)((uint8_t *)(block + 1) + size);
            *tail = (struct block){block->size - size - sizeof(*tail),
                                   block->next, block_magic, 1};
            block->next = tail;
            block->size = size;
        }
        block->free = 0;
        return block + 1;
    }
    return NULL;
}

void x86_64_heap_free(void *pointer)
{
    if (pointer == NULL) return;
    struct block *target = NULL;
    for (struct block *block = head; block != NULL; block = block->next)
        if (block + 1 == pointer && block->magic == block_magic && !block->free) {
            target = block; break;
        }
    if (target == NULL) return;
    target->free = 1;
    for (struct block *block = head; block != NULL && block->next != NULL;) {
        struct block *next = block->next;
        if (block->free && next->free && block->magic == block_magic &&
            next->magic == block_magic &&
            (uint8_t *)(block + 1) + block->size == (uint8_t *)next) {
            block->size += sizeof(*next) + next->size;
            block->next = next->next;
            next->magic = 0;
        } else block = next;
    }
}

int x86_64_heap_init(void)
{
    head = (struct block *)storage;
    *head = (struct block){HEAP_SIZE - sizeof(*head), NULL, block_magic, 1};
    void *first = x86_64_heap_alloc(128);
    void *second = x86_64_heap_alloc(256);
    if (first == NULL || second == NULL || first == second) return 0;
    x86_64_heap_free(first);
    void *reused = x86_64_heap_alloc(64);
    if (reused != first) return 0;
    x86_64_heap_free((uint8_t *)reused + 1);
    void *third = x86_64_heap_alloc(32);
    if (third == reused) return 0;
    x86_64_heap_free(third);
    x86_64_heap_free(reused);
    x86_64_heap_free(second);
    if (x86_64_heap_alloc(SIZE_MAX) != NULL) return 0;
    void *large = x86_64_heap_alloc(HEAP_SIZE / 2U);
    if (large == NULL) return 0;
    x86_64_heap_free(large);
    return 1;
}
