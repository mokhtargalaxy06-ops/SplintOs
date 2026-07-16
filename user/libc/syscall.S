.section .text
.global sys_write
sys_write:
    push %ebx
    mov $1, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    mov 16(%esp), %edx
    int $0x80
    pop %ebx
    ret

.global sys_exit
sys_exit:
    mov $2, %eax
    mov 4(%esp), %ebx
    int $0x80
1:
    jmp 1b

.global sys_open
sys_open:
    push %ebx
    mov $3, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.global sys_read
sys_read:
    push %ebx
    mov $4, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    mov 16(%esp), %edx
    int $0x80
    pop %ebx
    ret

.global sys_close
sys_close:
    push %ebx
    mov $5, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_getpid
sys_getpid:
    mov $6, %eax
    int $0x80
    ret

.global sys_spawn
sys_spawn:
    push %ebx
    mov $7, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    mov 16(%esp), %edx
    int $0x80
    pop %ebx
    ret

.global sys_wait
sys_wait:
    push %ebx
    mov $8, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.global sys_dup2
sys_dup2:
    push %ebx
    mov $9, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.global sys_pipe
sys_pipe:
    push %ebx
    mov $10, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_spawn_actions
sys_spawn_actions:
    push %ebx
    mov $11, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_list
sys_list:
    push %ebx
    mov $12, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    mov 16(%esp), %edx
    int $0x80
    pop %ebx
    ret

.global sys_memory_info
sys_memory_info:
    push %ebx
    mov $13, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_uptime
sys_uptime:
    mov $14, %eax
    int $0x80
    ret

.global sys_process_info
sys_process_info:
    push %ebx
    mov $15, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_brk
sys_brk:
    push %ebx
    mov $16, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_yield
sys_yield:
    mov $17, %eax
    int $0x80
    ret

.global sys_sleep
sys_sleep:
    push %ebx
    mov $18, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_seek
sys_seek:
    push %ebx
    mov $19, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.global sys_fsync
sys_fsync:
    push %ebx
    mov $20, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_unlink
sys_unlink:
    push %ebx
    mov $21, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_rename
sys_rename:
    push %ebx
    mov $22, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.global sys_log_read
sys_log_read:
    push %ebx
    mov $23, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.global sys_stat
sys_stat:
    push %ebx
    mov $24, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.global sys_uname
sys_uname:
    push %ebx
    mov $25, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_truncate
sys_truncate:
    push %ebx
    mov $26, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.global sys_mkdir
sys_mkdir:
    push %ebx
    mov $27, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_rmdir
sys_rmdir:
    push %ebx
    mov $28, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_chmod
sys_chmod:
    push %ebx
    mov $29, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    int $0x80
    pop %ebx
    ret

.global sys_getuid
sys_getuid:
    mov $30, %eax
    int $0x80
    ret

.global sys_poll
sys_poll:
    push %ebx
    mov $31, %eax
    mov 8(%esp), %ebx
    mov 12(%esp), %ecx
    mov 16(%esp), %edx
    int $0x80
    pop %ebx
    ret

.global sys_clock_get
sys_clock_get:
    push %ebx
    mov $32, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_udp_open
sys_udp_open:
    push %ebx
    mov $33, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_udp_send
sys_udp_send:
    push %ebx
    push %esi
    mov $34, %eax
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
    mov $35, %eax
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
    mov $36, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.global sys_wall_clock
sys_wall_clock:
    push %ebx
    mov $37, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    ret

.section .note.GNU-stack,"",@progbits
