#include "syscall.h"

#include "devices.h"
#include "interrupts.h"
#include "memory.h"
#include "scheduler.h"
#include "filesystem.h"
#include "elf.h"

#include <stddef.h>
#include <stdint.h>

enum {
    SYSCALL_WRITE = 1,
    SYSCALL_EXIT = 2,
    SYSCALL_OPEN = 3,
    SYSCALL_READ = 4,
    SYSCALL_CLOSE = 5,
    SYSCALL_GETPID = 6,
    SYSCALL_SPAWN = 7,
    SYSCALL_WAIT = 8,
    SYSCALL_DUP2 = 9,
    SYSCALL_PIPE = 10,
    SYSCALL_SPAWN_ACTIONS = 11,
    SYSCALL_LIST = 12,
    SYSCALL_MEMORY_INFO = 13,
    SYSCALL_UPTIME = 14,
    SYSCALL_PROCESS_INFO = 15,
    SYSCALL_BRK = 16,
    SYSCALL_FSYNC = 20,
    SYSCALL_UNLINK = 21,
    SYSCALL_RENAME = 22,
    SYSCALL_MAX_WRITE = 4096,
    SYSCALL_MAX_PATH = 127,
    SYSCALL_MAX_ARGUMENTS = 8,
    SYSCALL_MAX_ACTIONS = 8,
};

struct syscall_memory_info { uint32_t total_kib, free_kib; };
struct syscall_process_info { uint32_t process_count, current_pid; };

struct user_spawn_request {
    uintptr_t path;
    uintptr_t arguments;
    uint32_t argument_count;
    uintptr_t actions;
    uint32_t action_count;
};

static bool copy_user_string(char *destination, size_t capacity, uintptr_t source)
{
    if (capacity == 0) return false;
    for (size_t i = 0; i + 1 < capacity; ++i) {
        if (!user_range_valid(task_current_address_space(), source + i, 1, false))
            return false;
        destination[i] = *(const char *)(source + i);
        if (destination[i] == '\0') return true;
    }
    destination[capacity - 1] = '\0';
    return false;
}

static void serial_number(uint32_t value)
{
    char buffer[11];
    size_t position = sizeof(buffer);
    buffer[--position] = '\0';
    do {
        buffer[--position] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    serial_write(buffer + position);
}

struct interrupt_frame *syscall_dispatch(struct interrupt_frame *frame)
{
    if ((frame->cs & 3U) != 3U) {
        frame->eax = (uint32_t)-1;
        return frame;
    }
    if (frame->eax == SYSCALL_EXIT) {
        serial_write("SplintOS: ELF user process exited status=");
        serial_number(frame->ebx);
        serial_write("\r\n");
        return scheduler_process_exit(frame, (int)frame->ebx);
    }
    if (frame->eax == SYSCALL_WRITE) {
        const char *buffer = (const char *)(uintptr_t)frame->ecx;
        size_t length = frame->edx;
        if (length > SYSCALL_MAX_WRITE ||
            !user_range_valid(task_current_address_space(), (uintptr_t)buffer,
                              length, false)) {
            frame->eax = (uint32_t)-1;
            return frame;
        }
        if (task_descriptor_is_pipe_write((int)frame->ebx))
            return scheduler_pipe_write(frame, (int)frame->ebx,
                                        (uintptr_t)buffer, length);
        char chunk[65];
        size_t written = 0;
        while (written < length) {
            size_t count = length - written;
            if (count >= sizeof(chunk)) count = sizeof(chunk) - 1;
            for (size_t i = 0; i < count; ++i) chunk[i] = buffer[written + i];
            chunk[count] = '\0';
            int result = task_descriptor_write((int)frame->ebx, chunk, count);
            if (result < 0) {
                frame->eax = written == 0 ? (uint32_t)-1 : written;
                return frame;
            }
            written += count;
        }
        frame->eax = (uint32_t)length;
        return frame;
    }
    if (frame->eax == SYSCALL_OPEN) {
        char path[SYSCALL_MAX_PATH + 1];
        if ((frame->ecx & ~(VFS_READ | VFS_WRITE | VFS_CREATE | VFS_APPEND |
                            VFS_TRUNCATE)) != 0 ||
            !copy_user_string(path, sizeof(path), frame->ebx)) {
            frame->eax = (uint32_t)-1;
            return frame;
        }
        frame->eax = (uint32_t)task_descriptor_open(path, frame->ecx);
        return frame;
    }
    if (frame->eax == SYSCALL_READ) {
        size_t length = frame->edx;
        char *user_buffer = (char *)(uintptr_t)frame->ecx;
        if (length > SYSCALL_MAX_WRITE ||
            !user_range_valid(task_current_address_space(), (uintptr_t)user_buffer,
                              length, true)) {
            frame->eax = (uint32_t)-1;
            return frame;
        }
        if (task_descriptor_is_console((int)frame->ebx))
            return scheduler_console_read(frame, (uintptr_t)user_buffer, length);
        if (task_descriptor_is_pipe_read((int)frame->ebx))
            return scheduler_pipe_read(frame, (int)frame->ebx,
                                       (uintptr_t)user_buffer, length);
        char chunk[64];
        size_t total = 0;
        while (total < length) {
            size_t request = length - total;
            if (request > sizeof(chunk)) request = sizeof(chunk);
            int count = task_descriptor_read((int)frame->ebx, chunk, request);
            if (count < 0) { frame->eax = total == 0 ? (uint32_t)-1 : total; return frame; }
            if (count == 0) break;
            for (int i = 0; i < count; ++i) user_buffer[total + (size_t)i] = chunk[i];
            total += (size_t)count;
            if ((size_t)count < request) break;
        }
        frame->eax = total;
        return frame;
    }
    if (frame->eax == SYSCALL_CLOSE) {
        frame->eax = (uint32_t)task_descriptor_close((int)frame->ebx);
        return frame;
    }
    if (frame->eax == SYSCALL_GETPID) {
        frame->eax = task_current_id();
        return frame;
    }
    if (frame->eax == SYSCALL_SPAWN) {
        char path[SYSCALL_MAX_PATH + 1];
        char argument_storage[SYSCALL_MAX_ARGUMENTS][SYSCALL_MAX_PATH + 1];
        const char *arguments[SYSCALL_MAX_ARGUMENTS];
        size_t count = frame->edx;
        if (count > SYSCALL_MAX_ARGUMENTS ||
            !copy_user_string(path, sizeof(path), frame->ebx) ||
            (count != 0 && !user_range_valid(task_current_address_space(), frame->ecx,
                                             count * sizeof(uintptr_t), false))) {
            frame->eax = (uint32_t)-1;
            return frame;
        }
        const uintptr_t *user_arguments = (const uintptr_t *)(uintptr_t)frame->ecx;
        for (size_t i = 0; i < count; ++i) {
            if (!copy_user_string(argument_storage[i], sizeof(argument_storage[i]),
                                  user_arguments[i])) {
                frame->eax = (uint32_t)-1;
                return frame;
            }
            arguments[i] = argument_storage[i];
        }
        frame->eax = (uint32_t)elf_load_process_args(path, "spawned", count, arguments);
        return frame;
    }
    if (frame->eax == SYSCALL_WAIT) {
        if (!user_range_valid(task_current_address_space(), frame->ecx,
                              sizeof(int), true)) {
            frame->eax = (uint32_t)-1;
            return frame;
        }
        return scheduler_wait(frame, frame->ebx, frame->ecx);
    }
    if (frame->eax == SYSCALL_DUP2) {
        frame->eax = (uint32_t)task_descriptor_dup2((int)frame->ebx,
                                                    (int)frame->ecx);
        return frame;
    }
    if (frame->eax == SYSCALL_PIPE) {
        if (!user_range_valid(task_current_address_space(), frame->ebx,
                              2 * sizeof(int), true)) {
            frame->eax = (uint32_t)-1;
            return frame;
        }
        int descriptors[2];
        int result = task_descriptor_pipe(descriptors);
        if (result == 0) {
            int *user_descriptors = (int *)(uintptr_t)frame->ebx;
            user_descriptors[0] = descriptors[0];
            user_descriptors[1] = descriptors[1];
        }
        frame->eax = (uint32_t)result;
        return frame;
    }
    if (frame->eax == SYSCALL_SPAWN_ACTIONS) {
        if (!user_range_valid(task_current_address_space(), frame->ebx,
                              sizeof(struct user_spawn_request), false)) {
            frame->eax = (uint32_t)-1;
            return frame;
        }
        const struct user_spawn_request *request =
            (const struct user_spawn_request *)(uintptr_t)frame->ebx;
        if (request->argument_count > SYSCALL_MAX_ARGUMENTS ||
            request->action_count > SYSCALL_MAX_ACTIONS) {
            frame->eax = (uint32_t)-1;
            return frame;
        }
        char path[SYSCALL_MAX_PATH + 1];
        char argument_storage[SYSCALL_MAX_ARGUMENTS][SYSCALL_MAX_PATH + 1];
        const char *arguments[SYSCALL_MAX_ARGUMENTS];
        struct process_descriptor_action actions[SYSCALL_MAX_ACTIONS];
        if (!copy_user_string(path, sizeof(path), request->path) ||
            (request->argument_count != 0 &&
             !user_range_valid(task_current_address_space(), request->arguments,
                 request->argument_count * sizeof(uintptr_t), false)) ||
            (request->action_count != 0 &&
             !user_range_valid(task_current_address_space(), request->actions,
                 request->action_count * sizeof(struct process_descriptor_action),
                 false))) {
            frame->eax = (uint32_t)-1;
            return frame;
        }
        const uintptr_t *user_arguments =
            (const uintptr_t *)(uintptr_t)request->arguments;
        for (size_t i = 0; i < request->argument_count; ++i) {
            if (!copy_user_string(argument_storage[i], sizeof(argument_storage[i]),
                                  user_arguments[i])) {
                frame->eax = (uint32_t)-1;
                return frame;
            }
            arguments[i] = argument_storage[i];
        }
        const struct process_descriptor_action *user_actions =
            (const struct process_descriptor_action *)(uintptr_t)request->actions;
        for (size_t i = 0; i < request->action_count; ++i) actions[i] = user_actions[i];
        frame->eax = (uint32_t)elf_load_process_actions(
            path, "spawned", request->argument_count, arguments,
            actions, request->action_count);
        return frame;
    }
    if (frame->eax == SYSCALL_LIST) {
        char path[SYSCALL_MAX_PATH + 1];
        size_t capacity = frame->edx;
        if (capacity > 32 || !copy_user_string(path, sizeof(path), frame->ebx) ||
            (capacity != 0 &&
             !user_range_valid(task_current_address_space(), frame->ecx,
                 capacity * sizeof(struct vfs_directory_entry), true))) {
            frame->eax = (uint32_t)-1;
            return frame;
        }
        struct vfs_directory_entry entries[32];
        int count = vfs_list(path, entries, capacity);
        if (count > 0) {
            struct vfs_directory_entry *user_entries =
                (struct vfs_directory_entry *)(uintptr_t)frame->ecx;
            for (int i = 0; i < count; ++i) user_entries[i] = entries[i];
        }
        frame->eax = (uint32_t)count;
        return frame;
    }
    if (frame->eax == SYSCALL_MEMORY_INFO) {
        if (!user_range_valid(task_current_address_space(), frame->ebx,
                              sizeof(struct syscall_memory_info), true)) {
            frame->eax = (uint32_t)-1;
            return frame;
        }
        struct syscall_memory_info *info =
            (struct syscall_memory_info *)(uintptr_t)frame->ebx;
        info->total_kib = memory_total_kib();
        info->free_kib = memory_free_kib();
        frame->eax = 0;
        return frame;
    }
    if (frame->eax == SYSCALL_UPTIME) {
        frame->eax = timer_ticks() / 100;
        return frame;
    }
    if (frame->eax == SYSCALL_PROCESS_INFO) {
        if (!user_range_valid(task_current_address_space(), frame->ebx,
                              sizeof(struct syscall_process_info), true)) {
            frame->eax = (uint32_t)-1;
            return frame;
        }
        struct syscall_process_info *info =
            (struct syscall_process_info *)(uintptr_t)frame->ebx;
        info->process_count = task_count();
        info->current_pid = task_current_id();
        frame->eax = 0;
        return frame;
    }
    if (frame->eax == SYSCALL_BRK) {
        frame->eax = (uint32_t)task_user_brk(frame->ebx);
        return frame;
    }
    if (frame->eax == SYSCALL_FSYNC) {
        frame->eax = (uint32_t)task_descriptor_fsync((int)frame->ebx);
        return frame;
    }
    if (frame->eax == SYSCALL_UNLINK) {
        char path[SYSCALL_MAX_PATH + 1];
        if (!copy_user_string(path, sizeof(path), frame->ebx)) {
            frame->eax = (uint32_t)-1;
            return frame;
        }
        frame->eax = (uint32_t)task_unlink(path);
        return frame;
    }
    if (frame->eax == SYSCALL_RENAME) {
        char old_path[SYSCALL_MAX_PATH + 1], new_path[SYSCALL_MAX_PATH + 1];
        if (!copy_user_string(old_path, sizeof(old_path), frame->ebx) ||
            !copy_user_string(new_path, sizeof(new_path), frame->ecx)) {
            frame->eax = (uint32_t)-1;
            return frame;
        }
        frame->eax = (uint32_t)task_rename(old_path, new_path);
        return frame;
    }
    frame->eax = (uint32_t)-1;
    return frame;
}
