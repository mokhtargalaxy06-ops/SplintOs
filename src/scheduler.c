#include "scheduler.h"

#include "devices.h"
#include "interrupts.h"
#include "memory.h"

#include <stddef.h>
#include <stdint.h>

enum {
    MAX_TASKS = 16,
    TASK_STACK_SIZE = 16 * 1024,
    TIME_SLICE_TICKS = 5,
};

struct task {
    uint32_t id;
    char name[24];
    enum task_state state;
    struct interrupt_frame *frame;
    uint8_t *stack;
    task_entry entry;
    void *context;
    uint32_t wake_tick;
    uint32_t uid;
};

static struct task tasks[MAX_TASKS];
static uint32_t current_index;
static uint32_t next_id = 1;
static uint32_t active_tasks;
static bool scheduler_ready;
static volatile bool force_reschedule;

static void string_copy(char *destination, const char *source, size_t capacity)
{
    if (capacity == 0) return;
    size_t index = 0;
    while (index + 1 < capacity && source[index] != '\0') {
        destination[index] = source[index];
        ++index;
    }
    destination[index] = '\0';
}

static void task_trampoline(void)
{
    struct task *task = &tasks[current_index];
    __asm__ volatile ("sti");
    task->entry(task->context);
    task_exit();
}

void scheduler_init(void)
{
    for (uint32_t i = 0; i < MAX_TASKS; ++i) tasks[i].state = TASK_UNUSED;
    tasks[0].id = 0;
    string_copy(tasks[0].name, "kernel", sizeof(tasks[0].name));
    tasks[0].state = TASK_RUNNING;
    tasks[0].uid = 0;
    current_index = 0;
    active_tasks = 1;
    scheduler_ready = true;
    serial_write("SplintOS: preemptive scheduler online\r\n");
}

int task_create(const char *name, task_entry entry, void *context)
{
    if (!scheduler_ready || entry == NULL) return -1;
    uint32_t slot;
    for (slot = 1; slot < MAX_TASKS; ++slot)
        if (tasks[slot].state == TASK_UNUSED || tasks[slot].state == TASK_TERMINATED) break;
    if (slot == MAX_TASKS) return -1;

    uint8_t *stack = kmalloc(TASK_STACK_SIZE);
    if (stack == NULL) return -1;
    struct interrupt_frame *frame = (struct interrupt_frame *)(stack + TASK_STACK_SIZE) - 1;
    *frame = (struct interrupt_frame){0};
    frame->gs = frame->fs = frame->es = frame->ds = 0x10;
    frame->eip = (uint32_t)(uintptr_t)task_trampoline;
    frame->cs = 0x08;
    frame->eflags = 0x202;

    tasks[slot].id = next_id++;
    string_copy(tasks[slot].name, name, sizeof(tasks[slot].name));
    tasks[slot].state = TASK_READY;
    tasks[slot].frame = frame;
    tasks[slot].stack = stack;
    tasks[slot].entry = entry;
    tasks[slot].context = context;
    tasks[slot].wake_tick = 0;
    tasks[slot].uid = tasks[current_index].uid;
    ++active_tasks;
    return (int)tasks[slot].id;
}

static bool tick_reached(uint32_t now, uint32_t target)
{
    return (int32_t)(now - target) >= 0;
}

struct interrupt_frame *scheduler_tick(struct interrupt_frame *frame)
{
    if (!scheduler_ready) return frame;
    uint32_t now = timer_ticks();
    for (uint32_t i = 1; i < MAX_TASKS; ++i)
        if (tasks[i].state == TASK_SLEEPING && tick_reached(now, tasks[i].wake_tick))
            tasks[i].state = TASK_READY;

    if (!force_reschedule && now % TIME_SLICE_TICKS != 0 &&
        tasks[current_index].state == TASK_RUNNING)
        return frame;
    force_reschedule = false;

    tasks[current_index].frame = frame;
    if (tasks[current_index].state == TASK_RUNNING) tasks[current_index].state = TASK_READY;
    for (uint32_t offset = 1; offset <= MAX_TASKS; ++offset) {
        uint32_t candidate = (current_index + offset) % MAX_TASKS;
        if (tasks[candidate].state == TASK_READY) {
            current_index = candidate;
            tasks[candidate].state = TASK_RUNNING;
            return tasks[candidate].frame;
        }
    }
    tasks[current_index].state = TASK_RUNNING;
    return frame;
}

void task_yield(void)
{
    force_reschedule = true;
    __asm__ volatile ("int $32");
}

void task_sleep(uint32_t duration)
{
    if (current_index == 0 || duration == 0) { task_yield(); return; }
    __asm__ volatile ("cli");
    tasks[current_index].wake_tick = timer_ticks() + duration;
    tasks[current_index].state = TASK_SLEEPING;
    __asm__ volatile ("sti");
    task_yield();
}

void task_exit(void)
{
    __asm__ volatile ("cli");
    if (current_index != 0) {
        tasks[current_index].state = TASK_TERMINATED;
        if (active_tasks != 0) --active_tasks;
    }
    __asm__ volatile ("sti");
    task_yield();
    for (;;) __asm__ volatile ("hlt");
}

uint32_t task_current_id(void) { return tasks[current_index].id; }
uint32_t task_count(void) { return active_tasks; }
uint32_t task_current_uid(void) { return tasks[current_index].uid; }
void task_set_current_uid(uint32_t uid) { tasks[current_index].uid = uid; }
