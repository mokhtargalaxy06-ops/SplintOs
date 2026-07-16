#include <splint/stdlib.h>
#include <splint/syscall.h>

#include <stdbool.h>
#include <stdint.h>

static uintptr_t current_break;

struct heap_block {
    size_t size;
    struct heap_block *next;
    uint32_t free;
    uint32_t magic;
};

enum { BLOCK_MAGIC = 0x53484D42U };
static struct heap_block *first_block;

static size_t aligned_size(size_t size)
{
    if (size > (size_t)-1 - 7U) return 0;
    return (size + 7U) & ~(size_t)7U;
}

void *splint_malloc(size_t size)
{
    if (size == 0) return NULL;
    size = aligned_size(size);
    if (size == 0) return NULL;
    if (current_break == 0) current_break = (uintptr_t)sys_brk(NULL);
    struct heap_block *tail = NULL;
    for (struct heap_block *block = first_block; block != NULL; block = block->next) {
        if (!block->free || block->size < size) { tail = block; continue; }
        if (block->size >= size + sizeof(struct heap_block) + 8U) {
            struct heap_block *remainder = (struct heap_block *)
                ((uintptr_t)(block + 1) + size);
            *remainder = (struct heap_block){
                block->size - size - sizeof(struct heap_block), block->next, 1, BLOCK_MAGIC};
            block->size = size;
            block->next = remainder;
        }
        block->free = 0;
        return block + 1;
    }
    if (current_break == (uintptr_t)-1 ||
        size > 0x90000000U - current_break - sizeof(struct heap_block))
        return NULL;
    uintptr_t base = current_break;
    uintptr_t requested = current_break + sizeof(struct heap_block) + size;
    if ((uintptr_t)sys_brk((void *)requested) != requested) return NULL;
    current_break = requested;
    struct heap_block *block = (struct heap_block *)base;
    *block = (struct heap_block){size, NULL, 0, BLOCK_MAGIC};
    if (first_block == NULL) first_block = block;
    else tail->next = block;
    return block + 1;
}

void splint_free(void *pointer)
{
    if (pointer == NULL) return;
    struct heap_block *target = (struct heap_block *)pointer - 1;
    struct heap_block *previous = NULL;
    for (struct heap_block *block = first_block; block != NULL;
         previous = block, block = block->next) {
        if (block != target) continue;
        if (block->magic != BLOCK_MAGIC || block->free) return;
        block->free = 1;
        if (block->next != NULL && block->next->free &&
            block->next->magic == BLOCK_MAGIC) {
            block->size += sizeof(struct heap_block) + block->next->size;
            block->next = block->next->next;
        }
        if (previous != NULL && previous->free) {
            previous->size += sizeof(struct heap_block) + block->size;
            previous->next = block->next;
            block = previous;
        }
        if (block->next == NULL) {
            struct heap_block *predecessor = NULL;
            for (struct heap_block *candidate = first_block;
                 candidate != NULL && candidate != block; candidate = candidate->next)
                predecessor = candidate;
            uintptr_t requested = (uintptr_t)block;
            if ((uintptr_t)sys_brk((void *)requested) == requested) {
                current_break = requested;
                if (predecessor == NULL) first_block = NULL;
                else predecessor->next = NULL;
            }
        }
        return;
    }
}

void *splint_calloc(size_t count, size_t size)
{
    if (count != 0 && size > (size_t)-1 / count) return NULL;
    size_t total = count * size;
    uint8_t *memory = splint_malloc(total);
    if (memory == NULL) return NULL;
    for (size_t i = 0; i < total; ++i) memory[i] = 0;
    return memory;
}

void *splint_realloc(void *pointer, size_t size)
{
    if (pointer == NULL) return splint_malloc(size);
    if (size == 0) { splint_free(pointer); return NULL; }
    size = aligned_size(size);
    if (size == 0) return NULL;
    struct heap_block *target = (struct heap_block *)pointer - 1;
    bool known = false;
    for (struct heap_block *block = first_block; block != NULL; block = block->next)
        if (block == target && block->magic == BLOCK_MAGIC && !block->free) {
            known = true; break;
        }
    if (!known) return NULL;
    if (target->size >= size) return pointer;
    if (target->next != NULL && target->next->free &&
        target->next->magic == BLOCK_MAGIC &&
        target->size + sizeof(struct heap_block) + target->next->size >= size) {
        target->size += sizeof(struct heap_block) + target->next->size;
        target->next = target->next->next;
        return pointer;
    }
    uint8_t *replacement = splint_malloc(size);
    if (replacement == NULL) return NULL;
    for (size_t i = 0; i < target->size; ++i)
        replacement[i] = ((uint8_t *)pointer)[i];
    splint_free(pointer);
    return replacement;
}
