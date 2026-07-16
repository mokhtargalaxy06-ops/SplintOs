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
#include "elf.h"
#include "block.h"
#include "partition.h"
#include "diskfs.h"

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

struct __attribute__((packed)) multiboot_command_info {
    uint32_t flags, mem_lower, mem_upper, boot_device, command_line;
};

static bool recovery_requested(uint32_t address)
{
    const struct multiboot_command_info *info =
        (const struct multiboot_command_info *)(uintptr_t)address;
    if ((info->flags & (1U << 2)) == 0 || info->command_line == 0) return false;
    const char *text = (const char *)(uintptr_t)info->command_line;
    static const char recovery[] = "recovery";
    for (size_t i = 0; text[i] != '\0'; ++i) {
        size_t j = 0;
        while (recovery[j] != '\0' && text[i + j] == recovery[j]) ++j;
        if (recovery[j] == '\0') return true;
    }
    return false;
}

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
    bool recovery = recovery_requested(multiboot_info);
    bool memory_ready = memory_init(multiboot_info);
    hardware_init();
    block_init();
    partition_init();
    diskfs_init();
    bool graphics = gui_init(multiboot_info);
    bool networking = network_init();
    if (memory_ready) {
        scheduler_init();
        security_init();
        filesystem_init();
        (void)task_create("maintenance", maintenance_task, NULL);
        applications_init(recovery);
        if (!recovery) {
            if (elf_load_process("/bin/hello", "hello") < 0)
                serial_write("SplintOS: failed to load /bin/hello\r\n");
            else
                serial_write("SplintOS: ELF loader online\r\n");
            if (elf_load_process("/bin/sh", "shell") < 0)
                serial_write("SplintOS: failed to load /bin/sh\r\n");
        }
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
