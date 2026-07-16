#include "scheduler.h"

#include "devices.h"
#include "interrupts.h"
#include "memory.h"
#include "filesystem.h"

#include <stddef.h>
#include <stdint.h>

enum {
    MAX_TASKS = 16,
    TASK_STACK_SIZE = 16 * 1024,
    TIME_SLICE_TICKS = 5,
    TASK_DESCRIPTOR_COUNT = 12,
    OPEN_FILE_COUNT = 48,
    PIPE_COUNT = 8,
    PIPE_CAPACITY = 256,
};

struct task_descriptor {
    bool used;
    int open_file;
};

enum open_file_type {
    OPEN_FILE_CONSOLE,
    OPEN_FILE_SERIAL,
    OPEN_FILE_VFS,
    OPEN_FILE_PIPE_READ,
    OPEN_FILE_PIPE_WRITE,
};

struct open_file {
    bool used;
    enum open_file_type type;
    uint32_t references;
    int vfs_descriptor;
};

struct pipe {
    bool used;
    uint8_t buffer[PIPE_CAPACITY];
    size_t read_position;
    size_t write_position;
    size_t count;
    uint32_t readers;
    uint32_t writers;
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
    uint32_t *address_space;
    struct task_descriptor descriptors[TASK_DESCRIPTOR_COUNT];
    uint32_t parent_id;
    int exit_status;
    uint32_t wait_child;
    uintptr_t wait_status_address;
    uintptr_t io_buffer;
    size_t io_count;
    int io_descriptor;
    uintptr_t user_break;
};

static struct task tasks[MAX_TASKS];
static uint32_t current_index;
static uint32_t next_id = 1;
static uint32_t active_tasks;
static bool scheduler_ready;
static volatile bool force_reschedule;
static struct open_file open_files[OPEN_FILE_COUNT];
static struct pipe pipes[PIPE_COUNT];

static void pipe_wake_waiters(void);

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

static int open_file_create(enum open_file_type type, int vfs_descriptor)
{
    for (int i = 0; i < OPEN_FILE_COUNT; ++i) {
        if (open_files[i].used) continue;
        open_files[i].used = true;
        open_files[i].type = type;
        open_files[i].references = 1;
        open_files[i].vfs_descriptor = vfs_descriptor;
        return i;
    }
    return -1;
}

static void open_file_retain(int index)
{
    if (index >= 0 && index < OPEN_FILE_COUNT && open_files[index].used)
        ++open_files[index].references;
}

static void open_file_release(int index)
{
    if (index < 0 || index >= OPEN_FILE_COUNT || !open_files[index].used ||
        open_files[index].references == 0) return;
    if (--open_files[index].references != 0) return;
    if (open_files[index].type == OPEN_FILE_VFS)
        (void)vfs_close(open_files[index].vfs_descriptor);
    else if (open_files[index].type == OPEN_FILE_PIPE_READ ||
             open_files[index].type == OPEN_FILE_PIPE_WRITE) {
        int pipe_index = open_files[index].vfs_descriptor;
        if (pipe_index >= 0 && pipe_index < PIPE_COUNT && pipes[pipe_index].used) {
            if (open_files[index].type == OPEN_FILE_PIPE_READ &&
                pipes[pipe_index].readers != 0) --pipes[pipe_index].readers;
            if (open_files[index].type == OPEN_FILE_PIPE_WRITE &&
                pipes[pipe_index].writers != 0) --pipes[pipe_index].writers;
            if (pipes[pipe_index].readers == 0 && pipes[pipe_index].writers == 0)
                pipes[pipe_index] = (struct pipe){0};
        }
        pipe_wake_waiters();
    }
    open_files[index] = (struct open_file){0};
}

static void descriptors_init(struct task *task, const struct task *parent)
{
    for (uint32_t i = 0; i < TASK_DESCRIPTOR_COUNT; ++i)
        task->descriptors[i] = (struct task_descriptor){0};
    if (parent == NULL) {
        int input = open_file_create(OPEN_FILE_CONSOLE, -1);
        int output = open_file_create(OPEN_FILE_SERIAL, -1);
        task->descriptors[0] = (struct task_descriptor){true, input};
        task->descriptors[1] = (struct task_descriptor){true, output};
        task->descriptors[2] = (struct task_descriptor){true, output};
        open_file_retain(output);
        return;
    }
    for (uint32_t i = 0; i < 3; ++i) {
        if (!parent->descriptors[i].used) continue;
        task->descriptors[i] = parent->descriptors[i];
        open_file_retain(task->descriptors[i].open_file);
    }
}

static void descriptors_close_all(struct task *task)
{
    for (uint32_t i = 0; i < TASK_DESCRIPTOR_COUNT; ++i) {
        if (task->descriptors[i].used)
            open_file_release(task->descriptors[i].open_file);
        task->descriptors[i] = (struct task_descriptor){0};
    }
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
    tasks[0].address_space = address_space_kernel();
    descriptors_init(&tasks[0], NULL);
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

    if (tasks[slot].state == TASK_TERMINATED) {
        kfree(tasks[slot].stack);
        address_space_destroy(tasks[slot].address_space);
    }
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
    tasks[slot].address_space = address_space_kernel();
    tasks[slot].parent_id = tasks[current_index].id;
    descriptors_init(&tasks[slot], &tasks[current_index]);
    ++active_tasks;
    return (int)tasks[slot].id;
}

int task_create_process(const char *name, struct process_image *image)
{
    return task_create_process_actions(name, image, NULL, 0);
}

int task_create_process_actions(const char *name, struct process_image *image,
                                const struct process_descriptor_action *actions,
                                size_t action_count)
{
    if (!scheduler_ready || image == NULL || image->address_space == NULL ||
        image->entry < 0x40000000U || image->entry >= 0xC0000000U ||
        image->stack_pointer <= 0x40000000U || image->stack_pointer > 0xC0000000U)
        return -1;
    if (action_count > TASK_DESCRIPTOR_COUNT ||
        (action_count != 0 && actions == NULL)) return -1;
    for (size_t i = 0; i < action_count; ++i) {
        const struct process_descriptor_action *action = &actions[i];
        if (action->destination < 0 || action->destination >= TASK_DESCRIPTOR_COUNT ||
            (action->type != PROCESS_DESCRIPTOR_DUP2 &&
             action->type != PROCESS_DESCRIPTOR_CLOSE)) return -1;
        if (action->type == PROCESS_DESCRIPTOR_DUP2 &&
            (action->source < 0 || action->source >= TASK_DESCRIPTOR_COUNT ||
             !tasks[current_index].descriptors[action->source].used)) return -1;
    }
    uint32_t slot;
    for (slot = 1; slot < MAX_TASKS; ++slot)
        if (tasks[slot].state == TASK_UNUSED || tasks[slot].state == TASK_TERMINATED) break;
    if (slot == MAX_TASKS) return -1;
    if (tasks[slot].state == TASK_TERMINATED) {
        kfree(tasks[slot].stack);
        address_space_destroy(tasks[slot].address_space);
    }
    uint8_t *kernel_stack = kmalloc(TASK_STACK_SIZE);
    if (kernel_stack == NULL) return -1;
    struct interrupt_frame *frame =
        (struct interrupt_frame *)(kernel_stack + TASK_STACK_SIZE) - 1;
    *frame = (struct interrupt_frame){0};
    frame->gs = frame->fs = frame->es = frame->ds = 0x23;
    frame->eip = (uint32_t)image->entry;
    frame->cs = 0x1B;
    frame->eflags = 0x202;
    frame->useresp = (uint32_t)image->stack_pointer;
    frame->ss = 0x23;
    tasks[slot].id = next_id++;
    string_copy(tasks[slot].name, name, sizeof(tasks[slot].name));
    tasks[slot].state = TASK_READY;
    tasks[slot].frame = frame;
    tasks[slot].stack = kernel_stack;
    tasks[slot].entry = NULL;
    tasks[slot].context = NULL;
    tasks[slot].wake_tick = 0;
    tasks[slot].uid = tasks[current_index].uid;
    tasks[slot].address_space = image->address_space;
    tasks[slot].parent_id = tasks[current_index].id;
    tasks[slot].exit_status = 0;
    tasks[slot].wait_child = 0;
    tasks[slot].wait_status_address = 0;
    tasks[slot].user_break = 0x80000000U;
    descriptors_init(&tasks[slot], &tasks[current_index]);
    for (size_t i = 0; i < action_count; ++i) {
        const struct process_descriptor_action *action = &actions[i];
        if (action->type == PROCESS_DESCRIPTOR_CLOSE) {
            if (tasks[slot].descriptors[action->destination].used) {
                open_file_release(tasks[slot].descriptors[action->destination].open_file);
                tasks[slot].descriptors[action->destination] =
                    (struct task_descriptor){0};
            }
            continue;
        }
        int open_file = tasks[current_index].descriptors[action->source].open_file;
        open_file_retain(open_file);
        if (tasks[slot].descriptors[action->destination].used)
            open_file_release(tasks[slot].descriptors[action->destination].open_file);
        tasks[slot].descriptors[action->destination] =
            (struct task_descriptor){true, open_file};
    }
    image->address_space = NULL;
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
            address_space_activate(tasks[candidate].address_space);
            interrupts_set_kernel_stack((uintptr_t)tasks[candidate].stack + TASK_STACK_SIZE);
            return tasks[candidate].frame;
        }
    }
    tasks[current_index].state = TASK_RUNNING;
    return frame;
}

struct interrupt_frame *scheduler_fault(struct interrupt_frame *frame)
{
    return scheduler_process_exit(frame, -1);
}

static struct task *task_by_id(uint32_t id)
{
    for (uint32_t i = 1; i < MAX_TASKS; ++i)
        if (tasks[i].state != TASK_UNUSED && tasks[i].state != TASK_TERMINATED &&
            tasks[i].id == id) return &tasks[i];
    return NULL;
}

static void write_wait_status(struct task *parent, uintptr_t address, int status)
{
    uint32_t *current_space = tasks[current_index].address_space;
    address_space_activate(parent->address_space);
    *(int *)address = status;
    address_space_activate(current_space);
}

struct interrupt_frame *scheduler_process_exit(struct interrupt_frame *frame,
                                               int status)
{
    if (current_index == 0) return frame;
    struct task *child = &tasks[current_index];
    descriptors_close_all(child);
    child->exit_status = status;
    child->state = TASK_ZOMBIE;
    if (active_tasks != 0) --active_tasks;
    struct task *parent = task_by_id(child->parent_id);
    if (parent != NULL && parent->state == TASK_WAITING &&
        (parent->wait_child == 0 || parent->wait_child == child->id)) {
        write_wait_status(parent, parent->wait_status_address, status);
        parent->frame->eax = child->id;
        parent->state = TASK_READY;
        child->state = TASK_TERMINATED;
    }
    if (child->parent_id == 0) child->state = TASK_TERMINATED;
    for (uint32_t i = 1; i < MAX_TASKS; ++i)
        if (tasks[i].parent_id == child->id && tasks[i].state != TASK_UNUSED &&
            tasks[i].state != TASK_TERMINATED) tasks[i].parent_id = 0;
    force_reschedule = true;
    return scheduler_tick(frame);
}

struct interrupt_frame *scheduler_wait(struct interrupt_frame *frame,
                                       uint32_t child_id,
                                       uintptr_t status_address)
{
    struct task *parent = &tasks[current_index];
    struct task *child = NULL;
    for (uint32_t i = 1; i < MAX_TASKS; ++i) {
        if (tasks[i].parent_id == parent->id && tasks[i].id == child_id &&
            tasks[i].state != TASK_UNUSED && tasks[i].state != TASK_TERMINATED) {
            child = &tasks[i];
            break;
        }
    }
    if (child == NULL) { frame->eax = (uint32_t)-1; return frame; }
    if (child->state == TASK_ZOMBIE) {
        *(int *)status_address = child->exit_status;
        child->state = TASK_TERMINATED;
        frame->eax = child_id;
        return frame;
    }
    parent->frame = frame;
    parent->wait_child = child_id;
    parent->wait_status_address = status_address;
    parent->state = TASK_WAITING;
    force_reschedule = true;
    return scheduler_tick(frame);
}

struct interrupt_frame *scheduler_console_read(struct interrupt_frame *frame,
                                               uintptr_t buffer, size_t count)
{
    if (count == 0) { frame->eax = 0; return frame; }
    int character = console_take_character();
    if (character >= 0) {
        *(char *)buffer = (char)character;
        frame->eax = 1;
        return frame;
    }
    tasks[current_index].frame = frame;
    tasks[current_index].io_buffer = buffer;
    tasks[current_index].io_count = count;
    tasks[current_index].state = TASK_IO_WAIT;
    force_reschedule = true;
    return scheduler_tick(frame);
}

void scheduler_console_wake(void)
{
    uint32_t *current_space = tasks[current_index].address_space;
    for (uint32_t i = 1; i < MAX_TASKS; ++i) {
        if (tasks[i].state != TASK_IO_WAIT || tasks[i].io_count == 0) continue;
        int character = console_take_character();
        if (character < 0) break;
        address_space_activate(tasks[i].address_space);
        *(char *)tasks[i].io_buffer = (char)character;
        address_space_activate(current_space);
        tasks[i].frame->eax = 1;
        tasks[i].state = TASK_READY;
        tasks[i].io_buffer = 0;
        tasks[i].io_count = 0;
    }
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
        descriptors_close_all(&tasks[current_index]);
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
uint32_t *task_current_address_space(void) { return tasks[current_index].address_space; }

int task_descriptor_open(const char *path, uint32_t flags)
{
    uint32_t descriptor;
    for (descriptor = 3; descriptor < TASK_DESCRIPTOR_COUNT; ++descriptor)
        if (!tasks[current_index].descriptors[descriptor].used) break;
    if (descriptor == TASK_DESCRIPTOR_COUNT) return -1;
    int vfs_descriptor = vfs_open(path, flags);
    if (vfs_descriptor < 0) return -1;
    int open_file = open_file_create(OPEN_FILE_VFS, vfs_descriptor);
    if (open_file < 0) { (void)vfs_close(vfs_descriptor); return -1; }
    tasks[current_index].descriptors[descriptor].used = true;
    tasks[current_index].descriptors[descriptor].open_file = open_file;
    return (int)descriptor;
}

int task_descriptor_read(int descriptor, void *buffer, size_t count)
{
    if (descriptor < 0 || descriptor >= TASK_DESCRIPTOR_COUNT ||
        !tasks[current_index].descriptors[descriptor].used) return -1;
    struct open_file *file =
        &open_files[tasks[current_index].descriptors[descriptor].open_file];
    if (!file->used || file->type != OPEN_FILE_VFS) return -1;
    return vfs_read(file->vfs_descriptor, buffer, count);
}

int task_descriptor_write(int descriptor, const void *buffer, size_t count)
{
    if (descriptor < 0 || descriptor >= TASK_DESCRIPTOR_COUNT ||
        !tasks[current_index].descriptors[descriptor].used) return -1;
    struct open_file *file =
        &open_files[tasks[current_index].descriptors[descriptor].open_file];
    if (!file->used) return -1;
    if (file->type == OPEN_FILE_SERIAL) {
        const char *characters = buffer;
        for (size_t i = 0; i < count; ++i) {
            char text[2] = {characters[i], '\0'};
            serial_write(text);
        }
        return (int)count;
    }
    if (file->type != OPEN_FILE_VFS) return -1;
    return vfs_write(file->vfs_descriptor, buffer, count);
}

int task_descriptor_fsync(int descriptor)
{
    if (descriptor < 0 || descriptor >= TASK_DESCRIPTOR_COUNT ||
        !tasks[current_index].descriptors[descriptor].used) return -1;
    struct open_file *file =
        &open_files[tasks[current_index].descriptors[descriptor].open_file];
    if (!file->used || file->type != OPEN_FILE_VFS) return -1;
    return vfs_fsync(file->vfs_descriptor);
}

int task_unlink(const char *path) { return vfs_unlink(path); }
int task_rename(const char *old_path, const char *new_path)
{ return vfs_rename(old_path, new_path); }

int task_descriptor_close(int descriptor)
{
    if (descriptor < 0 || descriptor >= TASK_DESCRIPTOR_COUNT ||
        !tasks[current_index].descriptors[descriptor].used) return -1;
    open_file_release(tasks[current_index].descriptors[descriptor].open_file);
    tasks[current_index].descriptors[descriptor] = (struct task_descriptor){0};
    return 0;
}

int task_descriptor_dup2(int source, int destination)
{
    if (source < 0 || source >= TASK_DESCRIPTOR_COUNT || destination < 0 ||
        destination >= TASK_DESCRIPTOR_COUNT ||
        !tasks[current_index].descriptors[source].used) return -1;
    if (source == destination) return destination;
    int open_file = tasks[current_index].descriptors[source].open_file;
    open_file_retain(open_file);
    if (tasks[current_index].descriptors[destination].used)
        open_file_release(tasks[current_index].descriptors[destination].open_file);
    tasks[current_index].descriptors[destination] =
        (struct task_descriptor){true, open_file};
    return destination;
}

bool task_descriptor_is_console(int descriptor)
{
    if (descriptor < 0 || descriptor >= TASK_DESCRIPTOR_COUNT ||
        !tasks[current_index].descriptors[descriptor].used) return false;
    int index = tasks[current_index].descriptors[descriptor].open_file;
    return index >= 0 && index < OPEN_FILE_COUNT && open_files[index].used &&
        open_files[index].type == OPEN_FILE_CONSOLE;
}

static struct open_file *descriptor_file(struct task *task, int descriptor)
{
    if (descriptor < 0 || descriptor >= TASK_DESCRIPTOR_COUNT ||
        !task->descriptors[descriptor].used) return NULL;
    int index = task->descriptors[descriptor].open_file;
    if (index < 0 || index >= OPEN_FILE_COUNT || !open_files[index].used) return NULL;
    return &open_files[index];
}

bool task_descriptor_is_pipe_read(int descriptor)
{
    struct open_file *file = descriptor_file(&tasks[current_index], descriptor);
    return file != NULL && file->type == OPEN_FILE_PIPE_READ;
}

bool task_descriptor_is_pipe_write(int descriptor)
{
    struct open_file *file = descriptor_file(&tasks[current_index], descriptor);
    return file != NULL && file->type == OPEN_FILE_PIPE_WRITE;
}

int task_descriptor_pipe(int descriptors[2])
{
    int first = -1, second = -1;
    for (int i = 3; i < TASK_DESCRIPTOR_COUNT; ++i) {
        if (tasks[current_index].descriptors[i].used) continue;
        if (first < 0) first = i;
        else { second = i; break; }
    }
    if (second < 0) return -1;
    int pipe_index;
    for (pipe_index = 0; pipe_index < PIPE_COUNT; ++pipe_index)
        if (!pipes[pipe_index].used) break;
    if (pipe_index == PIPE_COUNT) return -1;
    pipes[pipe_index] = (struct pipe){0};
    pipes[pipe_index].used = true;
    pipes[pipe_index].readers = 1;
    pipes[pipe_index].writers = 1;
    int read_file = open_file_create(OPEN_FILE_PIPE_READ, pipe_index);
    if (read_file < 0) { pipes[pipe_index] = (struct pipe){0}; return -1; }
    int write_file = open_file_create(OPEN_FILE_PIPE_WRITE, pipe_index);
    if (write_file < 0) {
        pipes[pipe_index].writers = 0;
        open_file_release(read_file);
        return -1;
    }
    tasks[current_index].descriptors[first] =
        (struct task_descriptor){true, read_file};
    tasks[current_index].descriptors[second] =
        (struct task_descriptor){true, write_file};
    descriptors[0] = first;
    descriptors[1] = second;
    return 0;
}

static int pipe_read_now(struct task *task, int descriptor, uintptr_t buffer,
                         size_t length, bool *blocked)
{
    struct open_file *file = descriptor_file(task, descriptor);
    if (file == NULL || file->type != OPEN_FILE_PIPE_READ) return -1;
    struct pipe *pipe = &pipes[file->vfs_descriptor];
    if (pipe->count == 0) {
        if (pipe->writers == 0) return 0;
        *blocked = true;
        return 0;
    }
    size_t count = length < pipe->count ? length : pipe->count;
    char *destination = (char *)buffer;
    for (size_t i = 0; i < count; ++i) {
        destination[i] = (char)pipe->buffer[pipe->read_position];
        pipe->read_position = (pipe->read_position + 1) % PIPE_CAPACITY;
    }
    pipe->count -= count;
    return (int)count;
}

static int pipe_write_now(struct task *task, int descriptor, uintptr_t buffer,
                          size_t length, bool *blocked)
{
    struct open_file *file = descriptor_file(task, descriptor);
    if (file == NULL || file->type != OPEN_FILE_PIPE_WRITE) return -1;
    struct pipe *pipe = &pipes[file->vfs_descriptor];
    if (pipe->readers == 0) return -1;
    size_t available = PIPE_CAPACITY - pipe->count;
    if (available == 0) { *blocked = true; return 0; }
    size_t count = length < available ? length : available;
    const char *source = (const char *)buffer;
    for (size_t i = 0; i < count; ++i) {
        pipe->buffer[pipe->write_position] = (uint8_t)source[i];
        pipe->write_position = (pipe->write_position + 1) % PIPE_CAPACITY;
    }
    pipe->count += count;
    return (int)count;
}

struct interrupt_frame *scheduler_pipe_read(struct interrupt_frame *frame,
                                            int descriptor, uintptr_t buffer,
                                            size_t count)
{
    bool blocked = false;
    int result = pipe_read_now(&tasks[current_index], descriptor, buffer, count, &blocked);
    if (!blocked) { frame->eax = (uint32_t)result; pipe_wake_waiters(); return frame; }
    tasks[current_index].frame = frame;
    tasks[current_index].io_descriptor = descriptor;
    tasks[current_index].io_buffer = buffer;
    tasks[current_index].io_count = count;
    tasks[current_index].state = TASK_PIPE_READ;
    force_reschedule = true;
    return scheduler_tick(frame);
}

struct interrupt_frame *scheduler_pipe_write(struct interrupt_frame *frame,
                                             int descriptor, uintptr_t buffer,
                                             size_t count)
{
    bool blocked = false;
    int result = pipe_write_now(&tasks[current_index], descriptor, buffer, count, &blocked);
    if (!blocked) { frame->eax = (uint32_t)result; pipe_wake_waiters(); return frame; }
    tasks[current_index].frame = frame;
    tasks[current_index].io_descriptor = descriptor;
    tasks[current_index].io_buffer = buffer;
    tasks[current_index].io_count = count;
    tasks[current_index].state = TASK_PIPE_WRITE;
    force_reschedule = true;
    return scheduler_tick(frame);
}

static void pipe_wake_waiters(void)
{
    uint32_t *current_space = tasks[current_index].address_space;
    bool progress;
    do {
        progress = false;
        for (uint32_t i = 1; i < MAX_TASKS; ++i) {
            if (tasks[i].state != TASK_PIPE_READ && tasks[i].state != TASK_PIPE_WRITE)
                continue;
            address_space_activate(tasks[i].address_space);
            bool blocked = false;
            int result = tasks[i].state == TASK_PIPE_READ
                ? pipe_read_now(&tasks[i], tasks[i].io_descriptor,
                                tasks[i].io_buffer, tasks[i].io_count, &blocked)
                : pipe_write_now(&tasks[i], tasks[i].io_descriptor,
                                 tasks[i].io_buffer, tasks[i].io_count, &blocked);
            address_space_activate(current_space);
            if (blocked) continue;
            tasks[i].frame->eax = (uint32_t)result;
            tasks[i].state = TASK_READY;
            tasks[i].io_buffer = 0;
            tasks[i].io_count = 0;
            progress = true;
        }
    } while (progress);
}

uintptr_t task_user_brk(uintptr_t requested)
{
    enum { HEAP_START = 0x80000000U, HEAP_LIMIT = 0x90000000U, PAGE_SIZE = 4096 };
    struct task *task = &tasks[current_index];
    if (requested == 0) return task->user_break;
    if (requested < task->user_break || requested < HEAP_START || requested > HEAP_LIMIT)
        return (uintptr_t)-1;
    uintptr_t mapped = (task->user_break + PAGE_SIZE - 1) & ~(uintptr_t)(PAGE_SIZE - 1);
    uintptr_t end = (requested + PAGE_SIZE - 1) & ~(uintptr_t)(PAGE_SIZE - 1);
    while (mapped < end) {
        void *page = physical_page_alloc();
        if (page == NULL) { task->user_break = mapped; return (uintptr_t)-1; }
        for (size_t i = 0; i < PAGE_SIZE; ++i) ((uint8_t *)page)[i] = 0;
        if (!address_space_map_user(task->address_space, mapped, page, true)) {
            physical_page_free(page);
            task->user_break = mapped;
            return (uintptr_t)-1;
        }
        mapped += PAGE_SIZE;
    }
    task->user_break = requested;
    return requested;
}
