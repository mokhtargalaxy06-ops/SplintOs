#include <stddef.h>
#include <stdint.h>
#include "arch/x86_64/input.h"
#include "arch/x86_64/timer.h"

enum { INPUT_CAPACITY = 32 };
static struct x86_64_input_event events[INPUT_CAPACITY];
static uint8_t read_index, write_index, event_count;
static uint8_t extended;
static uint8_t mouse_packet[3], mouse_index;

static inline uint8_t inb(uint16_t port)
{ uint8_t value; __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port)); return value; }
static inline void outb(uint16_t port, uint8_t value)
{ __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port)); }

static uint64_t lock(void)
{ uint64_t flags; __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory"); return flags; }
static void unlock(uint64_t flags)
{ if ((flags & (1U << 9)) != 0) __asm__ volatile ("sti" : : : "memory"); }

void x86_64_input_init(void)
{ uint64_t flags = lock(); read_index = 0; write_index = 0; event_count = 0; unlock(flags); }

int x86_64_input_push(const struct x86_64_input_event *event)
{
    if (event == NULL || event->type > X86_64_INPUT_POINTER) return 0;
    uint64_t flags = lock();
    if (event_count == INPUT_CAPACITY) { unlock(flags); return 0; }
    events[write_index] = *event;
    write_index = (uint8_t)((write_index + 1U) % INPUT_CAPACITY);
    ++event_count; unlock(flags); return 1;
}

int x86_64_input_pop(struct x86_64_input_event *event)
{
    if (event == NULL) return 0;
    uint64_t flags = lock();
    if (event_count == 0) { unlock(flags); return 0; }
    *event = events[read_index];
    read_index = (uint8_t)((read_index + 1U) % INPUT_CAPACITY);
    --event_count; unlock(flags); return 1;
}

int x86_64_input_conformance_test(void)
{
    x86_64_input_init();
    for (uint16_t i = 0; i < INPUT_CAPACITY; ++i) {
        struct x86_64_input_event event = {i, (int32_t)i, -(int32_t)i,
            i, (uint8_t)(i & 1U), 1};
        if (!x86_64_input_push(&event)) return 0;
    }
    struct x86_64_input_event event = {0};
    if (x86_64_input_push(&event)) return 0;
    for (uint16_t i = 0; i < INPUT_CAPACITY; ++i)
        if (!x86_64_input_pop(&event) || event.timestamp != i ||
            event.code != i || event.value_x != i) return 0;
    return !x86_64_input_pop(&event);
}

static int keyboard_scancode(uint8_t scancode)
{
    enum { KEY_ESCAPE = 1, KEY_ENTER = 28, KEY_LEFT = 75, KEY_RIGHT = 77,
           KEY_UP = 72, KEY_DOWN = 80 };
    if (scancode == 0xe0) { extended = 1; return 1; }
    uint8_t released = (uint8_t)(scancode & 0x80U);
    uint8_t code = (uint8_t)(scancode & 0x7fU);
    if (code == 0 || code > 83) { extended = 0; return 0; }
    uint16_t key = code;
    if (extended) {
        if (code == 0x4b) key = KEY_LEFT;
        else if (code == 0x4d) key = KEY_RIGHT;
        else if (code == 0x48) key = KEY_UP;
        else if (code == 0x50) key = KEY_DOWN;
        else { extended = 0; return 0; }
    } else if (code == 0x1c) key = KEY_ENTER;
    else if (code == 0x01) key = KEY_ESCAPE;
    extended = 0;
    struct x86_64_input_event event = {
        x86_64_timer_ticks(), 0, 0, key, X86_64_INPUT_KEY, released == 0
    };
    return x86_64_input_push(&event);
}

void x86_64_ps2_keyboard_irq(void)
{
    if ((inb(0x64) & 1U) != 0) (void)keyboard_scancode(inb(0x60));
}

int x86_64_ps2_keyboard_init(void)
{
    x86_64_input_init(); extended = 0;
    if (!keyboard_scancode(0xe0) || !keyboard_scancode(0x4b) ||
        !keyboard_scancode(0xe0) || !keyboard_scancode(0xcb)) return 0;
    struct x86_64_input_event press, release;
    if (!x86_64_input_pop(&press) || !x86_64_input_pop(&release) ||
        press.code != 75 || !press.pressed || release.code != 75 || release.pressed)
        return 0;
    x86_64_input_init();
    while ((inb(0x64) & 1U) != 0) (void)inb(0x60);
    x86_64_pic_unmask(1);
    return 1;
}

static int mouse_byte(uint8_t value)
{
    if (mouse_index == 0 && (value & 8U) == 0) return 0;
    mouse_packet[mouse_index++] = value;
    if (mouse_index != 3) return 1;
    mouse_index = 0;
    if ((mouse_packet[0] & 0xc0U) != 0) return 0;
    struct x86_64_input_event event = {
        x86_64_timer_ticks(), (int8_t)mouse_packet[1], -(int8_t)mouse_packet[2],
        (uint16_t)(mouse_packet[0] & 7U), X86_64_INPUT_POINTER,
        (uint8_t)((mouse_packet[0] & 1U) != 0)
    };
    return x86_64_input_push(&event);
}

void x86_64_ps2_mouse_irq(void)
{
    if ((inb(0x64) & 0x21U) == 0x21U) (void)mouse_byte(inb(0x60));
}

static int controller_write(uint16_t port, uint8_t value)
{
    for (uint32_t spin = 0; spin < 100000; ++spin)
        if ((inb(0x64) & 2U) == 0) { outb(port, value); return 1; }
    return 0;
}

static int controller_read(uint8_t *value)
{
    for (uint32_t spin = 0; spin < 100000; ++spin)
        if ((inb(0x64) & 1U) != 0) { *value = inb(0x60); return 1; }
    return 0;
}

static int mouse_command(uint8_t command)
{
    uint8_t response;
    return controller_write(0x64, 0xd4) && controller_write(0x60, command) &&
           controller_read(&response) && response == 0xfa;
}

int x86_64_ps2_mouse_init(void)
{
    x86_64_input_init(); mouse_index = 0;
    if (!mouse_byte(0x09) || !mouse_byte(4) || !mouse_byte(0xfe)) return 0;
    struct x86_64_input_event event;
    if (!x86_64_input_pop(&event) || event.type != X86_64_INPUT_POINTER ||
        event.value_x != 4 || event.value_y != 2 || !event.pressed) return 0;
    x86_64_input_init(); mouse_index = 0;
    uint64_t flags = lock();
    int ready = controller_write(0x64, 0xa8) && controller_write(0x64, 0x20);
    uint8_t configuration = 0;
    ready = ready && controller_read(&configuration);
    if (ready) {
        configuration = (uint8_t)((configuration | 2U) & (uint8_t)~0x20U);
        ready = controller_write(0x64, 0x60) && controller_write(0x60, configuration) &&
                mouse_command(0xf6) && mouse_command(0xf4);
    }
    unlock(flags);
    if (!ready) return 0;
    x86_64_pic_unmask(12);
    return 1;
}
