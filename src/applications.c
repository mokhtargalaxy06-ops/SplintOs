#include "applications.h"

#include "devices.h"
#include "diskfs.h"
#include "filesystem.h"
#include "interrupts.h"
#include "memory.h"
#include "scheduler.h"
#include "network.h"
#include "hardware.h"
#include "security.h"

#include <stddef.h>
#include <stdint.h>

struct command {
    const char *name;
    const char *description;
    void (*run)(const char *arguments);
};

static size_t text_length(const char *text)
{
    size_t result = 0;
    while (text[result] != '\0') ++result;
    return result;
}

static bool text_equal(const char *left, const char *right)
{
    while (*left != '\0' && *left == *right) { ++left; ++right; }
    return *left == *right;
}

static void print(const char *text) { serial_write(text); }

static void print_number(uint32_t value)
{
    char buffer[11];
    size_t position = sizeof(buffer);
    buffer[--position] = '\0';
    do {
        buffer[--position] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    print(buffer + position);
}

static void command_help(const char *arguments);

static void command_echo(const char *arguments)
{
    print(arguments);
    print("\r\n");
}

static void command_ls(const char *arguments)
{
    const char *path = *arguments == '\0' ? "/" : arguments;
    struct vfs_directory_entry entries[32];
    int count = vfs_list(path, entries, 32);
    if (count < 0) { print("ls: directory not found\r\n"); return; }
    for (int i = 0; i < count; ++i) {
        print(entries[i].type == VFS_DIRECTORY ? "[dir]  " : "[file] ");
        print(entries[i].name);
        print("\r\n");
    }
}

static void command_cat(const char *arguments)
{
    if (*arguments == '\0') { print("usage: cat /path\r\n"); return; }
    int fd = vfs_open(arguments, VFS_READ);
    if (fd < 0) { print("cat: file not found\r\n"); return; }
    char buffer[65];
    int count;
    while ((count = vfs_read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[count] = '\0';
        print(buffer);
    }
    (void)vfs_close(fd);
}

static void command_write(const char *arguments)
{
    const char *space = arguments;
    while (*space != '\0' && *space != ' ') ++space;
    if (*space == '\0') { print("usage: write /path text\r\n"); return; }
    char path[64];
    size_t length = (size_t)(space - arguments);
    if (length == 0 || length >= sizeof(path)) { print("write: invalid path\r\n"); return; }
    for (size_t i = 0; i < length; ++i) path[i] = arguments[i];
    path[length] = '\0';
    while (*space == ' ') ++space;
    int fd = vfs_open(path, VFS_WRITE | VFS_CREATE | VFS_TRUNCATE);
    if (fd < 0 || vfs_write(fd, space, text_length(space)) < 0) {
        print("write: failed\r\n");
    } else print("file written\r\n");
    if (fd >= 0) (void)vfs_close(fd);
}

static void command_mem(const char *arguments)
{
    (void)arguments;
    print("total usable memory: "); print_number(memory_total_kib()); print(" KiB\r\n");
    print("free physical memory: "); print_number(memory_free_kib()); print(" KiB\r\n");
}

static void command_tasks(const char *arguments)
{
    (void)arguments;
    print("active tasks: "); print_number(task_count()); print("\r\ncurrent task: ");
    print_number(task_current_id()); print("\r\n");
}

static void command_uptime(const char *arguments)
{
    (void)arguments;
    print("uptime: "); print_number(timer_ticks() / 100); print(" seconds\r\n");
}

static void command_net(const char *arguments)
{
    (void)arguments;
    uint8_t address[4];
    network_address(address);
    print("IPv4 address: ");
    for (uint8_t i = 0; i < 4; ++i) {
        print_number(address[i]);
        if (i != 3) print(".");
    }
    print(network_dhcp_configured() ? " (DHCP)\r\n" : " (static fallback)\r\n");
    print("Protocols: Ethernet ARP IPv4 ICMP UDP DHCP\r\n");
}

static void print_hex16(uint16_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    char output[5];
    for (int i = 3; i >= 0; --i) { output[i] = digits[value & 15U]; value >>= 4; }
    output[4] = '\0';
    print(output);
}

static void command_devices(const char *arguments)
{
    (void)arguments;
    print("ACPI: ");
    if (acpi_available()) {
        print("RSDT found, tables="); print_number(acpi_table_count()); print("\r\n");
    } else print("not found\r\n");
    print("PCI devices:\r\n");
    for (size_t i = 0; i < pci_device_count(); ++i) {
        const struct pci_device *device = pci_device_get(i);
        print_number(device->bus); print(":"); print_number(device->slot); print(".");
        print_number(device->function); print(" ");
        print_hex16(device->vendor_id); print(":"); print_hex16(device->device_id);
        print(" "); print(pci_class_name(device->class_code)); print(" IRQ ");
        print_number(device->interrupt_line); print("\r\n");
    }
}

static void command_whoami(const char *arguments)
{
    (void)arguments;
    print(security_current_name()); print(" uid=");
    print_number(task_current_uid()); print("\r\n");
}

static void command_migrate_disk(const char *arguments)
{
    if (*arguments != '\0') {
        print("usage: migrate-disk\r\n"); return;
    }
    if (diskfs_migrate_legacy() == 0)
        print("SPLFS4 migrated to SPLFS5\r\n");
    else
        print("migrate-disk: no validated read-only SPLFS4 mount\r\n");
}

static const struct command commands[] = {
    {"help", "show available commands", command_help},
    {"echo", "print text", command_echo},
    {"ls", "list a directory", command_ls},
    {"cat", "read a file", command_cat},
    {"write", "create or replace a file", command_write},
    {"mem", "show memory statistics", command_mem},
    {"tasks", "show scheduler statistics", command_tasks},
    {"uptime", "show seconds since boot", command_uptime},
    {"net", "show network configuration", command_net},
    {"devices", "list ACPI and PCI hardware", command_devices},
    {"whoami", "show the task security identity", command_whoami},
    {"migrate-disk", "explicitly migrate validated SPLFS4 metadata", command_migrate_disk},
};

static void command_help(const char *arguments)
{
    (void)arguments;
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        print(commands[i].name); print(" - "); print(commands[i].description); print("\r\n");
    }
}

static void execute(char *line)
{
    while (*line == ' ') ++line;
    char *arguments = line;
    while (*arguments != '\0' && *arguments != ' ') ++arguments;
    if (*arguments != '\0') {
        *arguments++ = '\0';
        while (*arguments == ' ') ++arguments;
    }
    if (*line == '\0') return;
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        if (text_equal(line, commands[i].name)) { commands[i].run(arguments); return; }
    }
    print("command not found; type help\r\n");
}

static void shell_task(void *context)
{
    (void)context;
    char line[128];
    size_t length = 0;
    print("\r\nSplintOS command shell\r\nType help for commands.\r\nsplint> ");
    for (;;) {
        int input = console_take_character();
        if (input < 0) { task_sleep(1); continue; }
        char character = (char)input;
        if (character == '\n') {
            print("\r\n");
            line[length] = '\0';
            execute(line);
            length = 0;
            print("splint> ");
        } else if (character == '\b' || character == 127) {
            if (length != 0) { --length; print("\b \b"); }
        } else if (character >= 32 && character < 127 && length + 1 < sizeof(line)) {
            line[length++] = character;
            char echo[2] = {character, '\0'};
            print(echo);
        }
    }
}

void applications_init(bool recovery_mode)
{
    if (recovery_mode && task_create("recovery-shell", shell_task, NULL) < 0)
        serial_write("SplintOS: failed to start recovery shell\r\n");
    serial_write(recovery_mode ? "SplintOS: recovery console selected\r\n"
                               : "SplintOS: application runtime online\r\n");
}
