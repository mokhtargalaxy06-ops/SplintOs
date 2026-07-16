#ifndef SPLINTOS_SCHEDULER_H
#define SPLINTOS_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

struct interrupt_frame;
typedef void (*task_entry)(void *context);

enum task_state {
    TASK_UNUSED,
    TASK_RUNNING,
    TASK_READY,
    TASK_SLEEPING,
    TASK_TERMINATED,
};

void scheduler_init(void);
int task_create(const char *name, task_entry entry, void *context);
struct interrupt_frame *scheduler_tick(struct interrupt_frame *frame);
void task_yield(void);
void task_sleep(uint32_t ticks);
void task_exit(void) __attribute__((noreturn));
uint32_t task_current_id(void);
uint32_t task_count(void);
uint32_t task_current_uid(void);
void task_set_current_uid(uint32_t uid);

#endif
