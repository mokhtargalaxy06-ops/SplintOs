#include "syscall.h"

#include "devices.h"
#include "error.h"
#include "interrupts.h"
#include "memory.h"
#include "scheduler.h"
#include "filesystem.h"
#include "elf.h"
#include "network.h"
#include "splint/abi.h"

#include <stddef.h>
#include <stdint.h>

enum {
    SYSCALL_WRITE = SPLINT_SYS_WRITE,
    SYSCALL_EXIT = SPLINT_SYS_EXIT,
    SYSCALL_OPEN = SPLINT_SYS_OPEN,
    SYSCALL_READ = SPLINT_SYS_READ,
    SYSCALL_CLOSE = SPLINT_SYS_CLOSE,
    SYSCALL_GETPID = SPLINT_SYS_GETPID,
    SYSCALL_SPAWN = SPLINT_SYS_SPAWN,
    SYSCALL_WAIT = SPLINT_SYS_WAIT,
    SYSCALL_DUP2 = SPLINT_SYS_DUP2,
    SYSCALL_PIPE = SPLINT_SYS_PIPE,
    SYSCALL_SPAWN_ACTIONS = SPLINT_SYS_SPAWN_ACTIONS,
    SYSCALL_LIST = SPLINT_SYS_LIST,
    SYSCALL_MEMORY_INFO = SPLINT_SYS_MEMORY_INFO,
    SYSCALL_UPTIME = SPLINT_SYS_UPTIME,
    SYSCALL_PROCESS_INFO = SPLINT_SYS_PROCESS_INFO,
    SYSCALL_BRK = SPLINT_SYS_BRK,
    SYSCALL_YIELD = SPLINT_SYS_YIELD,
    SYSCALL_SLEEP = SPLINT_SYS_SLEEP,
    SYSCALL_SEEK = SPLINT_SYS_SEEK,
    SYSCALL_FSYNC = SPLINT_SYS_FSYNC,
    SYSCALL_UNLINK = SPLINT_SYS_UNLINK,
    SYSCALL_RENAME = SPLINT_SYS_RENAME,
    SYSCALL_LOG_READ = SPLINT_SYS_LOG_READ,
    SYSCALL_STAT = SPLINT_SYS_STAT,
    SYSCALL_UNAME = SPLINT_SYS_UNAME,
    SYSCALL_TRUNCATE = SPLINT_SYS_TRUNCATE,
    SYSCALL_MKDIR = SPLINT_SYS_MKDIR,
    SYSCALL_RMDIR = SPLINT_SYS_RMDIR,
    SYSCALL_CHMOD = SPLINT_SYS_CHMOD,
    SYSCALL_GETUID = SPLINT_SYS_GETUID,
    SYSCALL_POLL = SPLINT_SYS_POLL,
    SYSCALL_CLOCK_GET = SPLINT_SYS_CLOCK_GET,
    SYSCALL_UDP_OPEN = SPLINT_SYS_UDP_OPEN,
    SYSCALL_UDP_SEND = SPLINT_SYS_UDP_SEND,
    SYSCALL_UDP_RECEIVE = SPLINT_SYS_UDP_RECEIVE,
    SYSCALL_NETWORK_CONFIG = SPLINT_SYS_NETWORK_CONFIG,
    SYSCALL_WALL_CLOCK = SPLINT_SYS_WALL_CLOCK,
    SYSCALL_STAT_TIMESTAMPS = SPLINT_SYS_STAT_TIMESTAMPS,
    SYSCALL_GETPGRP = SPLINT_SYS_GETPGRP,
    SYSCALL_SETPGID = SPLINT_SYS_SETPGID,
};

#define SYSCALL_MAX_WRITE SPLINT_ABI_MAX_IO
#define SYSCALL_MAX_PATH SPLINT_ABI_MAX_PATH
#define SYSCALL_MAX_ARGUMENTS SPLINT_ABI_MAX_ARGUMENTS
#define SYSCALL_MAX_ACTIONS SPLINT_ABI_MAX_ACTIONS

struct syscall_memory_info { uint32_t total_kib, free_kib; };
struct syscall_process_info { uint32_t process_count, current_pid; };
struct syscall_uname { char system[16], release[16], machine[16]; };
struct syscall_poll_entry { int descriptor; uint32_t events, returned_events; };
struct syscall_clock { uint32_t ticks, ticks_per_second; };
struct syscall_udp_endpoint { uint8_t address[4]; uint16_t port, reserved; };
struct syscall_network_config { uint8_t address[4], subnet[4], gateway[4], dns[4]; };
struct syscall_wall_clock { uint16_t year; uint8_t month, day, hour, minute, second; };

_Static_assert(sizeof(struct syscall_memory_info) == 8, "memory info ABI changed");
_Static_assert(sizeof(struct syscall_process_info) == 8, "process info ABI changed");
_Static_assert(sizeof(struct syscall_uname) == 48, "uname ABI changed");
_Static_assert(sizeof(struct syscall_poll_entry) == 12, "poll entry ABI changed");
_Static_assert(sizeof(struct syscall_clock) == 8, "clock ABI changed");
_Static_assert(sizeof(struct syscall_udp_endpoint) == 8, "UDP endpoint ABI changed");
_Static_assert(sizeof(struct syscall_network_config) == 16, "network config ABI changed");
_Static_assert(sizeof(struct syscall_wall_clock) == 8, "wall clock ABI changed");

struct user_spawn_request {
    uintptr_t path;
    uintptr_t arguments;
    uint32_t argument_count;
    uintptr_t actions;
    uint32_t action_count;
};

_Static_assert(sizeof(struct user_spawn_request) == 20, "spawn request ABI changed");

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
    if (frame->eax == SYSCALL_YIELD)
        return scheduler_user_yield(frame);
    if (frame->eax == SYSCALL_SLEEP)
        return scheduler_user_sleep(frame, frame->ebx);
    if (frame->eax == SYSCALL_SEEK) {
        frame->eax = (uint32_t)task_descriptor_seek((int)frame->ebx, frame->ecx);
        return frame;
    }
    if (frame->eax == SYSCALL_FSYNC) {
        int result = task_descriptor_fsync((int)frame->ebx);
        frame->eax = result == KERNEL_OK ? 0U : (uint32_t)-1;
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
    if (frame->eax == SYSCALL_LOG_READ) {
        size_t capacity = frame->ecx;
        if (capacity > 512 || capacity == 0 ||
            !user_range_valid(task_current_address_space(), frame->ebx,
                              capacity, true)) {
            frame->eax = (uint32_t)-1; return frame;
        }
        char snapshot[512];
        size_t count = boot_log_read(snapshot, capacity);
        char *user_buffer = (char *)(uintptr_t)frame->ebx;
        for (size_t i = 0; i < count; ++i) user_buffer[i] = snapshot[i];
        frame->eax = count;
        return frame;
    }
    if (frame->eax == SYSCALL_STAT) {
        char path[SYSCALL_MAX_PATH + 1];
        if (!copy_user_string(path, sizeof(path), frame->ebx) ||
            !user_range_valid(task_current_address_space(), frame->ecx,
                              sizeof(struct vfs_directory_entry), true)) {
            frame->eax = (uint32_t)-1; return frame;
        }
        struct vfs_directory_entry entry;
        int result = vfs_stat(path, &entry);
        if (result == 0)
            *(struct vfs_directory_entry *)(uintptr_t)frame->ecx = entry;
        frame->eax = (uint32_t)result;
        return frame;
    }
    if (frame->eax == SYSCALL_STAT_TIMESTAMPS) {
        char path[SYSCALL_MAX_PATH + 1];
        if (!copy_user_string(path, sizeof(path), frame->ebx) ||
            !user_range_valid(task_current_address_space(), frame->ecx,
                              sizeof(struct vfs_timestamp_entry), true)) {
            frame->eax = (uint32_t)-1; return frame;
        }
        struct vfs_timestamp_entry entry;
        int result = vfs_stat_timestamps(path, &entry);
        if (result == 0)
            *(struct vfs_timestamp_entry *)(uintptr_t)frame->ecx = entry;
        frame->eax = (uint32_t)result;
        return frame;
    }
    if (frame->eax == SYSCALL_GETPGRP) {
        frame->eax = task_current_process_group();
        return frame;
    }
    if (frame->eax == SYSCALL_SETPGID) {
        frame->eax = (uint32_t)task_set_process_group(frame->ebx, frame->ecx);
        return frame;
    }
    if (frame->eax == SYSCALL_UNAME) {
        if (!user_range_valid(task_current_address_space(), frame->ebx,
                              sizeof(struct syscall_uname), true)) {
            frame->eax = (uint32_t)-1; return frame;
        }
        static const struct syscall_uname identity = {
            "SplintOS", "0.4-educational", "i686"
        };
        *(struct syscall_uname *)(uintptr_t)frame->ebx = identity;
        frame->eax = 0;
        return frame;
    }
    if (frame->eax == SYSCALL_TRUNCATE) {
        frame->eax = (uint32_t)task_descriptor_truncate((int)frame->ebx, frame->ecx);
        return frame;
    }
    if (frame->eax == SYSCALL_MKDIR || frame->eax == SYSCALL_RMDIR) {
        char path[SYSCALL_MAX_PATH + 1];
        if (!copy_user_string(path, sizeof(path), frame->ebx)) {
            frame->eax = (uint32_t)-1; return frame;
        }
        frame->eax = frame->eax == SYSCALL_MKDIR
            ? (uint32_t)task_mkdir(path) : (uint32_t)task_rmdir(path);
        return frame;
    }
    if (frame->eax == SYSCALL_CHMOD) {
        char path[SYSCALL_MAX_PATH + 1];
        if (frame->ecx > 0777U || !copy_user_string(path, sizeof(path), frame->ebx)) {
            frame->eax = (uint32_t)-1; return frame;
        }
        frame->eax = (uint32_t)task_chmod(path, (uint16_t)frame->ecx);
        return frame;
    }
    if (frame->eax == SYSCALL_GETUID) {
        frame->eax = task_current_uid();
        return frame;
    }
    if (frame->eax == SYSCALL_POLL) {
        size_t count = frame->ecx;
        if (count > 8 || frame->edx > 0x7FFFFFFFU ||
            (count != 0 && !user_range_valid(task_current_address_space(), frame->ebx,
                count * sizeof(struct syscall_poll_entry), true))) {
            frame->eax = (uint32_t)-1; return frame;
        }
        struct syscall_poll_entry *entries =
            (struct syscall_poll_entry *)(uintptr_t)frame->ebx;
        for (size_t i = 0; i < count; ++i) {
            if ((entries[i].events & ~3U) != 0) {
                frame->eax = (uint32_t)-1; return frame;
            }
            entries[i].returned_events = 0;
        }
        return scheduler_poll(frame, frame->ebx, count, frame->edx);
    }
    if (frame->eax == SYSCALL_CLOCK_GET) {
        if (!user_range_valid(task_current_address_space(), frame->ebx,
                              sizeof(struct syscall_clock), true)) {
            frame->eax = (uint32_t)-1; return frame;
        }
        struct syscall_clock *clock = (struct syscall_clock *)(uintptr_t)frame->ebx;
        clock->ticks = timer_ticks();
        clock->ticks_per_second = 100;
        frame->eax = 0;
        return frame;
    }
    if (frame->eax == SYSCALL_UDP_OPEN) {
        if (frame->ebx > 65535U) frame->eax = (uint32_t)-1;
        else frame->eax = (uint32_t)task_udp_open((uint16_t)frame->ebx);
        return frame;
    }
    if (frame->eax == SYSCALL_UDP_SEND) {
        size_t length = frame->esi;
        if (length > 512 ||
            !user_range_valid(task_current_address_space(), frame->ecx,
                              sizeof(struct syscall_udp_endpoint), false) ||
            (length != 0 && !user_range_valid(task_current_address_space(), frame->edx,
                                               length, false))) {
            frame->eax = (uint32_t)-1; return frame;
        }
        struct syscall_udp_endpoint endpoint =
            *(const struct syscall_udp_endpoint *)(uintptr_t)frame->ecx;
        uint8_t payload[512];
        const uint8_t *source = (const uint8_t *)(uintptr_t)frame->edx;
        for (size_t i = 0; i < length; ++i) payload[i] = source[i];
        frame->eax = (uint32_t)task_udp_send((int)frame->ebx, endpoint.address,
                                             endpoint.port, payload, length);
        return frame;
    }
    if (frame->eax == SYSCALL_UDP_RECEIVE) {
        size_t capacity = frame->esi;
        if (capacity > 512 ||
            !user_range_valid(task_current_address_space(), frame->ecx,
                              sizeof(struct syscall_udp_endpoint), true) ||
            (capacity != 0 && !user_range_valid(task_current_address_space(), frame->edx,
                                                 capacity, true))) {
            frame->eax = (uint32_t)-1; return frame;
        }
        uint8_t payload[512], address[4]; uint16_t port;
        int count = task_udp_receive((int)frame->ebx, payload, capacity, address, &port);
        if (count >= 0) {
            struct syscall_udp_endpoint *endpoint =
                (struct syscall_udp_endpoint *)(uintptr_t)frame->ecx;
            for (size_t i = 0; i < 4; ++i) endpoint->address[i] = address[i];
            endpoint->port = port; endpoint->reserved = 0;
            uint8_t *destination = (uint8_t *)(uintptr_t)frame->edx;
            for (int i = 0; i < count; ++i) destination[i] = payload[i];
        }
        frame->eax = (uint32_t)count;
        return frame;
    }
    if (frame->eax == SYSCALL_NETWORK_CONFIG) {
        if (!user_range_valid(task_current_address_space(), frame->ebx,
                              sizeof(struct syscall_network_config), true)) {
            frame->eax = (uint32_t)-1; return frame;
        }
        struct syscall_network_config *configuration =
            (struct syscall_network_config *)(uintptr_t)frame->ebx;
        network_configuration(configuration->address, configuration->subnet,
                              configuration->gateway, configuration->dns);
        frame->eax = 0;
        return frame;
    }
    if (frame->eax == SYSCALL_WALL_CLOCK) {
        if (!user_range_valid(task_current_address_space(), frame->ebx,
                              sizeof(struct syscall_wall_clock), true)) {
            frame->eax = (uint32_t)-1; return frame;
        }
        struct wall_clock clock;
        if (!devices_wall_clock(&clock)) { frame->eax = (uint32_t)-1; return frame; }
        struct syscall_wall_clock *output =
            (struct syscall_wall_clock *)(uintptr_t)frame->ebx;
        output->year = clock.year; output->month = clock.month; output->day = clock.day;
        output->hour = clock.hour; output->minute = clock.minute; output->second = clock.second;
        frame->eax = 0;
        return frame;
    }
    frame->eax = (uint32_t)-1;
    return frame;
}
