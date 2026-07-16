#include "devices.h"
#include "scheduler.h"

#include <stdint.h>

enum {
    PS2_DATA = 0x60,
    PS2_STATUS = 0x64,
    PS2_COMMAND = 0x64,
    COM1 = 0x3F8,
};

static volatile enum input_key pending_key;
static volatile struct mouse_state mouse;
static uint8_t mouse_packet[3];
static uint8_t mouse_index;
static bool serial_ready;
static bool mouse_ready;
static char console_buffer[128];
static uint8_t console_read;
static uint8_t console_write;

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static bool wait_input_clear(void)
{
    for (uint32_t i = 0; i < 100000; ++i)
        if ((inb(PS2_STATUS) & 2U) == 0) return true;
    return false;
}

static bool wait_output_full(void)
{
    for (uint32_t i = 0; i < 100000; ++i)
        if ((inb(PS2_STATUS) & 1U) != 0) return true;
    return false;
}

static void controller_command(uint8_t command)
{
    if (wait_input_clear()) outb(PS2_COMMAND, command);
}

static bool mouse_command(uint8_t command)
{
    controller_command(0xD4);
    if (!wait_input_clear()) return false;
    outb(PS2_DATA, command);
    return wait_output_full() && inb(PS2_DATA) == 0xFA;
}

static void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
    outb(COM1 + 4, 0x1E);
    outb(COM1 + 0, 0xAE);
    /* A missing loopback echo is diagnostic only; do not silence COM1 logs. */
    serial_ready = true;
    (void)inb(COM1 + 0);
    outb(COM1 + 4, 0x0F);
    outb(COM1 + 1, 0x01);
}

void serial_write(const char *text)
{
    if (!serial_ready) return;
    while (*text != '\0') {
        for (uint32_t i = 0; i < 100000 && (inb(COM1 + 5) & 0x20U) == 0; ++i) {}
        outb(COM1, (uint8_t)*text++);
    }
}

void devices_init(void)
{
    serial_init();
    serial_write("SplintOS: device manager starting\r\n");

    /* Enable the PS/2 auxiliary port and IRQ-independent mouse streaming. */
    controller_command(0xA8);
    controller_command(0x20);
    if (wait_output_full()) {
        uint8_t config = inb(PS2_DATA);
        controller_command(0x60);
        if (wait_input_clear()) outb(PS2_DATA, (uint8_t)((config | 2U) & ~0x20U));
    }
    mouse_ready = mouse_command(0xF6) && mouse_command(0xF4);
    serial_write(mouse_ready ? "SplintOS: PS/2 mouse online\r\n"
                             : "SplintOS: PS/2 mouse unavailable\r\n");
}

static void handle_keyboard(uint8_t code)
{
    if ((code & 0x80U) != 0) return;
    if (code == 0x4B) pending_key = KEY_LEFT;
    else if (code == 0x4D) pending_key = KEY_RIGHT;
    else if (code == 0x0F) pending_key = KEY_TAB;
    else if (code == 0x1C) pending_key = KEY_ENTER;

    static const char keymap[58] = {
        [0x02]='1',[0x03]='2',[0x04]='3',[0x05]='4',[0x06]='5',[0x07]='6',
        [0x08]='7',[0x09]='8',[0x0A]='9',[0x0B]='0',[0x0C]='-',[0x0E]='\b',
        [0x0F]='\t',[0x10]='q',[0x11]='w',[0x12]='e',[0x13]='r',[0x14]='t',
        [0x15]='y',[0x16]='u',[0x17]='i',[0x18]='o',[0x19]='p',[0x1C]='\n',
        [0x1E]='a',[0x1F]='s',[0x20]='d',[0x21]='f',[0x22]='g',[0x23]='h',
        [0x24]='j',[0x25]='k',[0x26]='l',[0x2C]='z',[0x2D]='x',[0x2E]='c',
        [0x2F]='v',[0x30]='b',[0x31]='n',[0x32]='m',[0x39]=' ',
    };
    if (code < sizeof(keymap) && keymap[code] != 0) {
        uint8_t next = (uint8_t)((console_write + 1) % sizeof(console_buffer));
        if (next != console_read) {
            console_buffer[console_write] = keymap[code];
            console_write = next;
            scheduler_console_wake();
        }
    }
}

static void handle_mouse(uint8_t byte)
{
    if (mouse_index == 0 && (byte & 8U) == 0) return;
    mouse_packet[mouse_index++] = byte;
    if (mouse_index != 3) return;
    mouse_index = 0;
    if ((mouse_packet[0] & 0xC0U) != 0) return;
    mouse.dx += (int8_t)mouse_packet[1];
    mouse.dy -= (int8_t)mouse_packet[2];
    mouse.left = (mouse_packet[0] & 1U) != 0;
    mouse.changed = true;
}

void devices_poll(void)
{
    while ((inb(PS2_STATUS) & 1U) != 0) {
        uint8_t status = inb(PS2_STATUS);
        uint8_t data = inb(PS2_DATA);
        if ((status & 0x20U) != 0 && mouse_ready) handle_mouse(data);
        else handle_keyboard(data);
    }
    if (serial_ready) {
        while ((inb(COM1 + 5) & 1U) != 0) {
            uint8_t next = (uint8_t)((console_write + 1) % sizeof(console_buffer));
            char character = (char)inb(COM1);
            if (character == '\r') character = '\n';
            if (next != console_read) {
                console_buffer[console_write] = character;
                console_write = next;
                scheduler_console_wake();
            }
        }
    }
}

enum input_key keyboard_take_key(void)
{
    enum input_key result = pending_key;
    pending_key = KEY_NONE;
    return result;
}

struct mouse_state mouse_take_state(void)
{
    struct mouse_state result = mouse;
    mouse.dx = 0;
    mouse.dy = 0;
    mouse.changed = false;
    return result;
}

int console_take_character(void)
{
    if (console_read == console_write) return -1;
    char result = console_buffer[console_read];
    console_read = (uint8_t)((console_read + 1) % sizeof(console_buffer));
    return (uint8_t)result;
}
