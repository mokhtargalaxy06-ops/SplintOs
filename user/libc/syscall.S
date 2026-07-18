#include <splint/abi.h>

.section .text
.global sys_write
sys_write:
    push %ebx
    mov $SPLINT_SYS_WRITE, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    mov 16(%esp), %edx
    int $0x80
    pop %ebx
    ret

.global sys_exit
sys_exit:
    mov $SPLINT_SYS_EXIT, %eax
    mov 4(%esp), %ebx
    int $0x80
1:
    jmp 1b

.global sys_open
sys_open:
    push %ebx
    mov $SPLINT_SYS_OPEN, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.global sys_read
sys_read:
    push %ebx
    mov $SPLINT_SYS_READ, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    mov 16(%esp), %edx
    int $0x80
    pop %ebx
    ret

.global sys_close
sys_close:
    push %ebx
    mov $SPLINT_SYS_CLOSE, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_getpid
sys_getpid:
    mov $SPLINT_SYS_GETPID, %eax
    int $0x80
    ret

.global sys_spawn
sys_spawn:
    push %ebx
    mov $SPLINT_SYS_SPAWN, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    mov 16(%esp), %edx
    int $0x80
    pop %ebx
    ret

.global sys_wait
sys_wait:
    push %ebx
    mov $SPLINT_SYS_WAIT, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.global sys_dup2
sys_dup2:
    push %ebx
    mov $SPLINT_SYS_DUP2, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.global sys_pipe
sys_pipe:
    push %ebx
    mov $SPLINT_SYS_PIPE, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_spawn_actions
sys_spawn_actions:
    push %ebx
    mov $SPLINT_SYS_SPAWN_ACTIONS, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_list
sys_list:
    push %ebx
    mov $SPLINT_SYS_LIST, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    mov 16(%esp), %edx
    int $0x80
    pop %ebx
    ret

.global sys_memory_info
sys_memory_info:
    push %ebx
    mov $SPLINT_SYS_MEMORY_INFO, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_uptime
sys_uptime:
    mov $SPLINT_SYS_UPTIME, %eax
    int $0x80
    ret

.global sys_process_info
sys_process_info:
    push %ebx
    mov $SPLINT_SYS_PROCESS_INFO, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_brk
sys_brk:
    push %ebx
    mov $SPLINT_SYS_BRK, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_yield
sys_yield:
    mov $SPLINT_SYS_YIELD, %eax
    int $0x80
    ret

.global sys_sleep
sys_sleep:
    push %ebx
    mov $SPLINT_SYS_SLEEP, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_seek
sys_seek:
    push %ebx
    mov $SPLINT_SYS_SEEK, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.global sys_fsync
sys_fsync:
    push %ebx
    mov $SPLINT_SYS_FSYNC, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_unlink
sys_unlink:
    push %ebx
    mov $SPLINT_SYS_UNLINK, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_rename
sys_rename:
    push %ebx
    mov $SPLINT_SYS_RENAME, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.global sys_log_read
sys_log_read:
    push %ebx
    mov $SPLINT_SYS_LOG_READ, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.global sys_stat
sys_stat:
    push %ebx
    mov $SPLINT_SYS_STAT, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.global sys_uname
sys_uname:
    push %ebx
    mov $SPLINT_SYS_UNAME, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_truncate
sys_truncate:
    push %ebx
    mov $SPLINT_SYS_TRUNCATE, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.global sys_mkdir
sys_mkdir:
    push %ebx
    mov $SPLINT_SYS_MKDIR, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_rmdir
sys_rmdir:
    push %ebx
    mov $SPLINT_SYS_RMDIR, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_chmod
sys_chmod:
    push %ebx
    mov $SPLINT_SYS_CHMOD, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.global sys_getuid
sys_getuid:
    mov $SPLINT_SYS_GETUID, %eax
    int $0x80
    ret

.global sys_poll
sys_poll:
    push %ebx
    mov $SPLINT_SYS_POLL, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    mov 16(%esp), %edx
    int $0x80
    pop %ebx
    ret

.global sys_clock_get
sys_clock_get:
    push %ebx
    mov $SPLINT_SYS_CLOCK_GET, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_udp_open
sys_udp_open:
    push %ebx
    mov $SPLINT_SYS_UDP_OPEN, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_udp_send
sys_udp_send:
    push %ebx
    push %esi
    mov $SPLINT_SYS_UDP_SEND, %eax
    mov 12(%esp), %ebx
    mov 16(%esp), %ecx
    mov 20(%esp), %edx
    mov 24(%esp), %esi
    int $0x80
    pop %esi
    pop %ebx
    ret

.global sys_udp_receive
sys_udp_receive:
    push %ebx
    push %esi
    mov $SPLINT_SYS_UDP_RECEIVE, %eax
    mov 12(%esp), %ebx
    mov 16(%esp), %ecx
    mov 20(%esp), %edx
    mov 24(%esp), %esi
    int $0x80
    pop %esi
    pop %ebx
    ret

.global sys_network_config
sys_network_config:
    push %ebx
    mov $SPLINT_SYS_NETWORK_CONFIG, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_wall_clock
sys_wall_clock:
    push %ebx
    mov $SPLINT_SYS_WALL_CLOCK, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_stat_timestamps
sys_stat_timestamps:
    push %ebx
    mov $SPLINT_SYS_STAT_TIMESTAMPS, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.global sys_getpgrp
sys_getpgrp:
    mov $SPLINT_SYS_GETPGRP, %eax
    int $0x80
    ret

.global sys_setpgid
sys_setpgid:
    push %ebx
    mov $SPLINT_SYS_SETPGID, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.section .note.GNU-stack,"",@progbits
