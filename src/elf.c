#include "elf.h"

#include "devices.h"
#include "filesystem.h"
#include "memory.h"
#include "scheduler.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PACKED __attribute__((packed))

enum {
    ELF_PROGRAM_LOAD = 1,
    ELF_FLAG_EXECUTE = 1,
    ELF_FLAG_WRITE = 2,
    USER_MIN = 0x40000000U,
    USER_MAX = 0xC0000000U,
    USER_STACK_PAGE = 0xBFFFF000U,
    PAGE_SIZE = 4096,
    MAX_PROGRAM_HEADERS = 32,
    MAX_ARGUMENTS = 8,
    MAX_ARGUMENT_BYTES = 512,
};

struct PACKED elf_header {
    uint8_t identity[16];
    uint16_t type, machine;
    uint32_t version, entry, program_offset, section_offset, flags;
    uint16_t header_size, program_entry_size, program_count;
    uint16_t section_entry_size, section_count, section_names;
};

struct PACKED elf_program_header {
    uint32_t type, offset, virtual_address, physical_address;
    uint32_t file_size, memory_size, flags, alignment;
};

static void page_zero(void *page)
{
    uint8_t *bytes = page;
    for (size_t i = 0; i < PAGE_SIZE; ++i) bytes[i] = 0;
}

static bool read_exact(int fd, size_t offset, void *buffer, size_t size)
{
    return vfs_seek(fd, offset) == 0 && vfs_read(fd, buffer, size) == (int)size;
}

static bool valid_header(const struct elf_header *header)
{
    return header->identity[0] == 0x7F && header->identity[1] == 'E' &&
        header->identity[2] == 'L' && header->identity[3] == 'F' &&
        header->identity[4] == 1 && header->identity[5] == 1 &&
        header->identity[6] == 1 && header->type == 2 && header->machine == 3 &&
        header->version == 1 && header->header_size == sizeof(*header) &&
        header->program_entry_size == sizeof(struct elf_program_header) &&
        header->program_count != 0 && header->program_count <= MAX_PROGRAM_HEADERS &&
        header->program_offset <= UINT32_MAX -
            (uint32_t)header->program_count * sizeof(struct elf_program_header) &&
        header->entry >= USER_MIN && header->entry < USER_MAX;
}

static bool load_segment(int fd, uint32_t *space,
                         const struct elf_program_header *segment)
{
    if (segment->memory_size < segment->file_size || segment->memory_size == 0 ||
        segment->virtual_address < USER_MIN || segment->virtual_address >= USER_MAX ||
        segment->memory_size > USER_MAX - segment->virtual_address ||
        segment->offset > UINT32_MAX - segment->file_size) return false;
    uintptr_t first = segment->virtual_address & ~(uintptr_t)(PAGE_SIZE - 1);
    uintptr_t end = segment->virtual_address + segment->memory_size;
    uintptr_t last = (end + PAGE_SIZE - 1) & ~(uintptr_t)(PAGE_SIZE - 1);
    if (last > USER_MAX || last < end) return false;
    for (uintptr_t virtual_page = first; virtual_page < last; virtual_page += PAGE_SIZE) {
        void *physical_page = physical_page_alloc();
        if (physical_page == NULL) return false;
        page_zero(physical_page);
        if (!address_space_map_user(space, virtual_page, physical_page,
                                    (segment->flags & ELF_FLAG_WRITE) != 0)) {
            physical_page_free(physical_page);
            return false;
        }
        uintptr_t page_end = virtual_page + PAGE_SIZE;
        uintptr_t file_start = segment->virtual_address;
        uintptr_t file_end = file_start + segment->file_size;
        uintptr_t copy_start = virtual_page > file_start ? virtual_page : file_start;
        uintptr_t copy_end = page_end < file_end ? page_end : file_end;
        if (copy_start < copy_end) {
            size_t file_offset = segment->offset + (copy_start - file_start);
            size_t page_offset = copy_start - virtual_page;
            if (!read_exact(fd, file_offset, (uint8_t *)physical_page + page_offset,
                            copy_end - copy_start)) return false;
        }
    }
    return true;
}

static bool stack_build(void *physical_stack, size_t argument_count,
                        const char *const arguments[], uintptr_t *stack_pointer)
{
    if (argument_count > MAX_ARGUMENTS) return false;
    uint8_t *stack = physical_stack;
    size_t offset = PAGE_SIZE;
    uintptr_t pointers[MAX_ARGUMENTS];
    size_t total = 0;
    for (size_t reverse = argument_count; reverse != 0; --reverse) {
        size_t index = reverse - 1;
        size_t length = 0;
        while (arguments[index][length] != '\0') {
            if (++length > MAX_ARGUMENT_BYTES) return false;
        }
        ++length;
        if (total + length > MAX_ARGUMENT_BYTES || length > offset) return false;
        total += length;
        offset -= length;
        for (size_t i = 0; i < length; ++i) stack[offset + i] = arguments[index][i];
        pointers[index] = USER_STACK_PAGE + offset;
    }
    offset &= ~(size_t)3;
    size_t words = argument_count + 2;
    if (offset < words * sizeof(uint32_t)) return false;
    offset -= sizeof(uint32_t);
    *(uint32_t *)(stack + offset) = 0;
    for (size_t reverse = argument_count; reverse != 0; --reverse) {
        offset -= sizeof(uint32_t);
        *(uint32_t *)(stack + offset) = (uint32_t)pointers[reverse - 1];
    }
    offset -= sizeof(uint32_t);
    *(uint32_t *)(stack + offset) = (uint32_t)argument_count;
    *stack_pointer = USER_STACK_PAGE + offset;
    return true;
}

int elf_load_process_args(const char *path, const char *name,
                          size_t argument_count, const char *const arguments[])
{
    return elf_load_process_actions(path, name, argument_count, arguments, NULL, 0);
}

int elf_load_process_actions(const char *path, const char *name,
                             size_t argument_count, const char *const arguments[],
                             const struct process_descriptor_action *actions,
                             size_t action_count)
{
    if (argument_count != 0 && arguments == NULL) return -1;
    int fd = vfs_open(path, VFS_READ);
    if (fd < 0) return -1;
    struct elf_header header;
    if (!read_exact(fd, 0, &header, sizeof(header)) || !valid_header(&header)) {
        (void)vfs_close(fd);
        return -1;
    }
    uint32_t *space = address_space_create();
    if (space == NULL) { (void)vfs_close(fd); return -1; }
    bool entry_executable = false;
    for (uint16_t i = 0; i < header.program_count; ++i) {
        struct elf_program_header segment;
        size_t offset = header.program_offset + (size_t)i * sizeof(segment);
        if (!read_exact(fd, offset, &segment, sizeof(segment))) goto fail;
        if (segment.type != ELF_PROGRAM_LOAD) continue;
        if (!load_segment(fd, space, &segment)) goto fail;
        if ((segment.flags & ELF_FLAG_EXECUTE) != 0 &&
            header.entry >= segment.virtual_address &&
            header.entry - segment.virtual_address < segment.memory_size)
            entry_executable = true;
    }
    if (!entry_executable) goto fail;
    void *stack = physical_page_alloc();
    if (stack == NULL) goto fail;
    page_zero(stack);
    uintptr_t stack_pointer;
    if (!stack_build(stack, argument_count, arguments, &stack_pointer)) {
        physical_page_free(stack);
        goto fail;
    }
    if (!address_space_map_user(space, USER_STACK_PAGE, stack, true)) {
        physical_page_free(stack);
        goto fail;
    }
    (void)vfs_close(fd);
    struct process_image image = {space, header.entry, stack_pointer};
    int process = task_create_process_actions(name, &image, actions, action_count);
    if (process < 0) { address_space_destroy(space); return -1; }
    serial_write("SplintOS: loaded ");
    serial_write(path);
    serial_write(" ELF32 process\r\n");
    return process;
fail:
    (void)vfs_close(fd);
    address_space_destroy(space);
    return -1;
}

int elf_load_process(const char *path, const char *name)
{
    const char *arguments[] = {path};
    return elf_load_process_args(path, name, 1, arguments);
}
