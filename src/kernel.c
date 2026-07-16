#include "kernel.h"
#include "network.h"
#include "gui.h"
#include "devices.h"
#include "interrupts.h"
#include "memory.h"
#include "scheduler.h"
#include "filesystem.h"
#include "applications.h"
#include "hardware.h"
#include "security.h"

static void maintenance_task(void *context)
{
    (void)context;
    for (;;) task_sleep(100);
}

#include <stddef.h>
#include <stdint.h>

enum {
    VGA_WIDTH = 80,
    VGA_HEIGHT = 25,
    MULTIBOOT_BOOTLOADER_MAGIC = 0x2BADB002,
};

static volatile uint16_t *const vga = (uint16_t *)0xB8000;
static size_t row;
static size_t column;
static uint8_t color = 0x0F;

static void terminal_clear(void)
{
    const uint16_t blank = (uint16_t)' ' | (uint16_t)color << 8;

    for (size_t y = 0; y < VGA_HEIGHT; ++y) {
        for (size_t x = 0; x < VGA_WIDTH; ++x) {
            vga[y * VGA_WIDTH + x] = blank;
        }
    }
}

static void terminal_putchar(char character)
{
    if (character == '\n') {
        column = 0;
        if (++row == VGA_HEIGHT) {
            row = 0;
        }
        return;
    }

    vga[row * VGA_WIDTH + column] =
        (uint16_t)(uint8_t)character | (uint16_t)color << 8;

    if (++column == VGA_WIDTH) {
        column = 0;
        if (++row == VGA_HEIGHT) {
            row = 0;
        }
    }
}

void terminal_write(const char *text)
{
    while (*text != '\0') {
        terminal_putchar(*text++);
    }
}

void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info)
{
    terminal_clear();

    if (multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        color = 0x4F;
        terminal_write("SplintOS: invalid bootloader state.\n");
        return;
    }

    devices_init();
    bool memory_ready = memory_init(multiboot_info);
    hardware_init();
    bool graphics = gui_init(multiboot_info);
    bool networking = network_init();
    if (memory_ready) {
        scheduler_init();
        security_init();
        filesystem_init();
        (void)task_create("maintenance", maintenance_task, NULL);
        applications_init();
    }
    interrupts_init();
    if (graphics) {
        gui_set_network(networking);
    } else {
        terminal_write("SplintOS graphical mode unavailable.\n");
        terminal_write(networking ? "Network ready: 10.0.2.15/24.\n"
                                  : "No RTL8139 network adapter found.\n");
        terminal_write(memory_ready ? "Memory manager ready.\n"
                                    : "Memory manager unavailable.\n");
    }

    for (;;) {
        if (networking) {
            network_poll();
        }
        if (graphics) {
            gui_poll();
        }
        devices_poll();
        __asm__ volatile ("pause");
    }
}
