#ifndef SPLINTOS_SCHEDULER_H
#define SPLINTOS_SCHEDULER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct interrupt_frame;
typedef void (*task_entry)(void *context);

struct process_image {
    uint32_t *address_space;
    uintptr_t entry;
    uintptr_t stack_pointer;
};

enum process_descriptor_action_type {
    PROCESS_DESCRIPTOR_DUP2 = 1,
    PROCESS_DESCRIPTOR_CLOSE = 2,
};

struct process_descriptor_action {
    uint32_t type;
    int source;
    int destination;
};

enum task_state {
    TASK_UNUSED,
    TASK_RUNNING,
    TASK_READY,
    TASK_SLEEPING,
    TASK_WAITING,
    TASK_IO_WAIT,
    TASK_PIPE_READ,
    TASK_PIPE_WRITE,
    TASK_POLL,
    TASK_ZOMBIE,
    TASK_TERMINATED,
};

void scheduler_init(void);
int task_create(const char *name, task_entry entry, void *context);
int task_create_process(const char *name, struct process_image *image);
int task_create_process_actions(const char *name, struct process_image *image,
                                const struct process_descriptor_action *actions,
                                size_t action_count);
struct interrupt_frame *scheduler_tick(struct interrupt_frame *frame);
struct interrupt_frame *scheduler_fault(struct interrupt_frame *frame);
struct interrupt_frame *scheduler_process_exit(struct interrupt_frame *frame,
                                               int status);
struct interrupt_frame *scheduler_wait(struct interrupt_frame *frame,
                                       uint32_t child_id,
                                       uintptr_t status_address);
struct interrupt_frame *scheduler_user_yield(struct interrupt_frame *frame);
struct interrupt_frame *scheduler_user_sleep(struct interrupt_frame *frame,
                                             uint32_t ticks);
struct interrupt_frame *scheduler_poll(struct interrupt_frame *frame,
                                       uintptr_t entries, size_t count,
                                       uint32_t timeout_ticks);
struct interrupt_frame *scheduler_console_read(struct interrupt_frame *frame,
                                               uintptr_t buffer, size_t count);
void scheduler_console_wake(void);
void task_yield(void);
void task_sleep(uint32_t ticks);
void task_exit(void) __attribute__((noreturn));
uint32_t task_current_id(void);
uint32_t task_count(void);
uint32_t task_current_uid(void);
void task_set_current_uid(uint32_t uid);
uint32_t *task_current_address_space(void);
int task_descriptor_open(const char *path, uint32_t flags);
int task_descriptor_read(int descriptor, void *buffer, size_t count);
int task_descriptor_write(int descriptor, const void *buffer, size_t count);
int task_descriptor_fsync(int descriptor);
int task_descriptor_seek(int descriptor, size_t offset);
int task_descriptor_truncate(int descriptor, size_t size);
int task_udp_open(uint16_t local_port);
int task_udp_send(int descriptor, const uint8_t address[4], uint16_t port,
                  const void *data, size_t length);
int task_udp_receive(int descriptor, void *data, size_t capacity,
                     uint8_t address[4], uint16_t *port);
int task_unlink(const char *path);
int task_rename(const char *old_path, const char *new_path);
int task_mkdir(const char *path);
int task_rmdir(const char *path);
int task_chmod(const char *path, uint16_t mode);
int task_descriptor_close(int descriptor);
int task_descriptor_dup2(int source, int destination);
bool task_descriptor_is_console(int descriptor);
int task_descriptor_pipe(int descriptors[2]);
struct interrupt_frame *scheduler_pipe_read(struct interrupt_frame *frame,
                                            int descriptor, uintptr_t buffer,
                                            size_t count);
struct interrupt_frame *scheduler_pipe_write(struct interrupt_frame *frame,
                                             int descriptor, uintptr_t buffer,
                                             size_t count);
bool task_descriptor_is_pipe_read(int descriptor);
bool task_descriptor_is_pipe_write(int descriptor);
uintptr_t task_user_brk(uintptr_t requested);

#endif
