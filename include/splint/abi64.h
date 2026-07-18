#ifndef SPLINT_ABI64_H
#define SPLINT_ABI64_H

#include <stdint.h>

#define SPLINT64_ABI_MAGIC UINT32_C(0x364C5053)
#define SPLINT64_ABI_VERSION 1U

/* RAX selects the call and returns a nonnegative value or a negative error.
 * Arguments 1..6 are RDI, RSI, RDX, R10, R8, and R9. SYSCALL destroys RCX
 * and R11; all other System V callee-saved registers remain preserved. */
#define SPLINT64_SYSCALL_ARGUMENTS 6U

#define SPLINT64_SYS_WRITE 1U
#define SPLINT64_SYS_EXIT 2U
#define SPLINT64_SYS_OPEN 3U
#define SPLINT64_SYS_READ 4U
#define SPLINT64_SYS_CLOSE 5U
#define SPLINT64_SYS_GETPID 6U
#define SPLINT64_SYS_SPAWN 7U
#define SPLINT64_SYS_WAIT 8U
#define SPLINT64_SYS_DUP2 9U
#define SPLINT64_SYS_PIPE 10U
#define SPLINT64_SYS_SPAWN_ACTIONS 11U
#define SPLINT64_SYS_LIST 12U
#define SPLINT64_SYS_MEMORY_INFO 13U
#define SPLINT64_SYS_UPTIME 14U
#define SPLINT64_SYS_PROCESS_INFO 15U
#define SPLINT64_SYS_BRK 16U
#define SPLINT64_SYS_YIELD 17U
#define SPLINT64_SYS_SLEEP 18U
#define SPLINT64_SYS_SEEK 19U
#define SPLINT64_SYS_FSYNC 20U
#define SPLINT64_SYS_UNLINK 21U
#define SPLINT64_SYS_RENAME 22U
#define SPLINT64_SYS_LOG_READ 23U
#define SPLINT64_SYS_STAT 24U
#define SPLINT64_SYS_UNAME 25U
#define SPLINT64_SYS_TRUNCATE 26U
#define SPLINT64_SYS_MKDIR 27U
#define SPLINT64_SYS_RMDIR 28U
#define SPLINT64_SYS_CHMOD 29U
#define SPLINT64_SYS_GETUID 30U
#define SPLINT64_SYS_POLL 31U
#define SPLINT64_SYS_CLOCK_GET 32U
#define SPLINT64_SYS_UDP_OPEN 33U
#define SPLINT64_SYS_UDP_SEND 34U
#define SPLINT64_SYS_UDP_RECEIVE 35U
#define SPLINT64_SYS_NETWORK_CONFIG 36U
#define SPLINT64_SYS_WALL_CLOCK 37U
#define SPLINT64_SYS_STAT_TIMESTAMPS 38U
#define SPLINT64_SYS_GETPGRP 39U
#define SPLINT64_SYS_SETPGID 40U
#define SPLINT64_SYS_ABI_QUERY 41U
#define SPLINT64_SYS_COUNT 41U

#define SPLINT64_MAX_IO UINT32_C(4096)
#define SPLINT64_MAX_PATH UINT32_C(4096)
#define SPLINT64_MAX_ARGUMENTS UINT32_C(64)
#define SPLINT64_MAX_ACTIONS UINT32_C(64)

#define SPLINT64_OK INT64_C(0)
#define SPLINT64_EINVAL (-INT64_C(1))
#define SPLINT64_EIO (-INT64_C(2))
#define SPLINT64_EBUSY (-INT64_C(3))
#define SPLINT64_ENOSPC (-INT64_C(4))
#define SPLINT64_ETIMEDOUT (-INT64_C(5))
#define SPLINT64_ENOTSUP (-INT64_C(6))
#define SPLINT64_ENOENT (-INT64_C(7))
#define SPLINT64_EFAULT (-INT64_C(8))
#define SPLINT64_EPERM (-INT64_C(9))
#define SPLINT64_ENOMEM (-INT64_C(10))
#define SPLINT64_EEXIST (-INT64_C(11))
#define SPLINT64_ERANGE (-INT64_C(12))

struct splint64_abi_info {
    uint32_t magic;
    uint16_t version, size;
    uint32_t syscall_count, flags;
};
struct splint64_descriptor_action {
    uint32_t type;
    int32_t source, destination;
    uint32_t reserved;
};
struct splint64_spawn_request {
    uint16_t version, size;
    uint32_t flags;
    uint64_t path, arguments;
    uint32_t argument_count, action_count;
    uint64_t actions;
};
struct splint64_directory_entry {
    char name[32];
    uint32_t type;
    uint16_t mode, reserved0;
    uint32_t owner, reserved1;
    uint64_t size;
};
struct splint64_timestamp_entry {
    struct splint64_directory_entry entry;
    uint64_t birth_time, modification_time, change_time;
};
struct splint64_poll_entry {
    int32_t descriptor;
    uint32_t events, returned_events, reserved;
};
struct splint64_memory_info { uint64_t total_bytes, free_bytes; };
struct splint64_process_info { uint32_t process_count, current_pid; uint64_t reserved; };
struct splint64_uname { char system[16], release[16], machine[16], architecture[16]; };
struct splint64_clock { uint64_t ticks; uint32_t ticks_per_second, reserved; };
struct splint64_udp_endpoint { uint8_t address[4]; uint16_t port, reserved; };
struct splint64_network_config { uint8_t address[4], subnet[4], gateway[4], dns[4]; };
struct splint64_wall_clock {
    uint16_t year;
    uint8_t month, day, hour, minute, second, reserved0;
    uint32_t nanosecond, reserved1;
};

_Static_assert(sizeof(struct splint64_abi_info) == 16, "x86_64 ABI info layout");
_Static_assert(sizeof(struct splint64_descriptor_action) == 16, "x86_64 action layout");
_Static_assert(sizeof(struct splint64_spawn_request) == 40, "x86_64 spawn layout");
_Static_assert(sizeof(struct splint64_directory_entry) == 56, "x86_64 directory layout");
_Static_assert(sizeof(struct splint64_timestamp_entry) == 80, "x86_64 timestamp layout");
_Static_assert(sizeof(struct splint64_poll_entry) == 16, "x86_64 poll layout");
_Static_assert(sizeof(struct splint64_memory_info) == 16, "x86_64 memory layout");
_Static_assert(sizeof(struct splint64_process_info) == 16, "x86_64 process layout");
_Static_assert(sizeof(struct splint64_uname) == 64, "x86_64 uname layout");
_Static_assert(sizeof(struct splint64_clock) == 16, "x86_64 clock layout");
_Static_assert(sizeof(struct splint64_udp_endpoint) == 8, "x86_64 UDP layout");
_Static_assert(sizeof(struct splint64_network_config) == 16, "x86_64 network layout");
_Static_assert(sizeof(struct splint64_wall_clock) == 16, "x86_64 wall-clock layout");

#endif
