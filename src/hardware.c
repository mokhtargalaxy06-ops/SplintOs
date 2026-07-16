#include "hardware.h"

#include "devices.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PACKED __attribute__((packed))

enum { PCI_ADDRESS = 0xCF8, PCI_DATA = 0xCFC, MAX_PCI_DEVICES = 64 };

struct PACKED rsdp_descriptor {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
};

struct PACKED acpi_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision, creator_id, creator_revision;
};

static struct pci_device pci_devices[MAX_PCI_DEVICES];
static size_t pci_count;
static bool found_acpi;
static uint32_t found_acpi_tables;

static inline void outw(uint16_t port, uint16_t value)
{
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t value)
{
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static bool bytes_equal(const void *left, const void *right, size_t count)
{
    const uint8_t *a = left;
    const uint8_t *b = right;
    while (count-- != 0) if (*a++ != *b++) return false;
    return true;
}

static bool checksum_valid(const void *data, size_t count)
{
    const uint8_t *bytes = data;
    uint8_t sum = 0;
    while (count-- != 0) sum = (uint8_t)(sum + *bytes++);
    return sum == 0;
}

static uint16_t physical_read16(uintptr_t address)
{
    uint16_t value;
    __asm__ volatile ("movw (%1), %0" : "=r"(value) : "r"(address) : "memory");
    return value;
}

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset)
{
    uint32_t address = 0x80000000U | (uint32_t)bus << 16 | (uint32_t)slot << 11 |
                       (uint32_t)function << 8 | (offset & 0xFC);
    outl(PCI_ADDRESS, address);
    return inl(PCI_DATA);
}

void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t function,
                        uint8_t offset, uint32_t value)
{
    uint32_t address = 0x80000000U | (uint32_t)bus << 16 | (uint32_t)slot << 11 |
                       (uint32_t)function << 8 | (offset & 0xFC);
    outl(PCI_ADDRESS, address);
    outl(PCI_DATA, value);
}

static void pci_write_command(const struct pci_device *device, uint16_t command)
{
    uint32_t address = 0x80000000U | (uint32_t)device->bus << 16 |
                       (uint32_t)device->slot << 11 | (uint32_t)device->function << 8 | 4;
    outl(PCI_ADDRESS, address);
    outw(PCI_DATA, command);
}

static void pci_record(uint8_t bus, uint8_t slot, uint8_t function)
{
    if (pci_count >= MAX_PCI_DEVICES) return;
    uint32_t id = pci_config_read32(bus, slot, function, 0);
    if ((uint16_t)id == 0xFFFF) return;
    struct pci_device *device = &pci_devices[pci_count++];
    device->bus = bus; device->slot = slot; device->function = function;
    device->vendor_id = (uint16_t)id; device->device_id = (uint16_t)(id >> 16);
    uint32_t class_data = pci_config_read32(bus, slot, function, 8);
    device->programming_interface = (uint8_t)(class_data >> 8);
    device->subclass = (uint8_t)(class_data >> 16);
    device->class_code = (uint8_t)(class_data >> 24);
    for (uint8_t bar = 0; bar < 6; ++bar)
        device->bars[bar] = pci_config_read32(bus, slot, function, (uint8_t)(0x10 + bar * 4));
    device->interrupt_line = (uint8_t)pci_config_read32(bus, slot, function, 0x3C);
}

static void pci_enumerate(void)
{
    pci_count = 0;
    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            uint32_t id = pci_config_read32((uint8_t)bus, slot, 0, 0);
            if ((uint16_t)id == 0xFFFF) continue;
            uint8_t header = (uint8_t)(pci_config_read32((uint8_t)bus, slot, 0, 0x0C) >> 16);
            uint8_t functions = (header & 0x80U) != 0 ? 8 : 1;
            for (uint8_t function = 0; function < functions; ++function)
                pci_record((uint8_t)bus, slot, function);
        }
    }
}

static const struct rsdp_descriptor *rsdp_scan(uintptr_t start, uintptr_t end)
{
    start = (start + 15U) & ~15U;
    for (uintptr_t address = start; address + sizeof(struct rsdp_descriptor) <= end; address += 16) {
        const struct rsdp_descriptor *rsdp = (const struct rsdp_descriptor *)address;
        if (bytes_equal(rsdp->signature, "RSD PTR ", 8) && checksum_valid(rsdp, 20)) return rsdp;
    }
    return NULL;
}

static void acpi_discover(void)
{
    uint16_t ebda_segment = physical_read16(0x40E);
    uintptr_t ebda = (uintptr_t)ebda_segment << 4;
    const struct rsdp_descriptor *rsdp = rsdp_scan(ebda, ebda + 1024);
    if (rsdp == NULL) rsdp = rsdp_scan(0xE0000, 0x100000);
    if (rsdp == NULL || rsdp->rsdt_address == 0) return;
    const struct acpi_header *rsdt = (const struct acpi_header *)(uintptr_t)rsdp->rsdt_address;
    if (!bytes_equal(rsdt->signature, "RSDT", 4) || rsdt->length < sizeof(*rsdt) ||
        !checksum_valid(rsdt, rsdt->length)) return;
    found_acpi = true;
    found_acpi_tables = (rsdt->length - sizeof(*rsdt)) / sizeof(uint32_t);
}

void hardware_init(void)
{
    pci_enumerate();
    acpi_discover();
    serial_write("SplintOS: PCI and ACPI discovery complete\r\n");
}

size_t pci_device_count(void) { return pci_count; }
const struct pci_device *pci_device_get(size_t index)
{ return index < pci_count ? &pci_devices[index] : NULL; }

const struct pci_device *pci_find_device(uint16_t vendor, uint16_t device)
{
    for (size_t i = 0; i < pci_count; ++i)
        if (pci_devices[i].vendor_id == vendor && pci_devices[i].device_id == device)
            return &pci_devices[i];
    return NULL;
}

void pci_enable_device(const struct pci_device *device, bool io, bool memory, bool bus_master)
{
    if (device == NULL) return;
    uint16_t command = (uint16_t)pci_config_read32(device->bus, device->slot,
                                                   device->function, 4);
    if (io) command |= 1U;
    if (memory) command |= 2U;
    if (bus_master) command |= 4U;
    pci_write_command(device, command);
}

const char *pci_class_name(uint8_t class_code)
{
    static const char *const names[] = {
        "unclassified", "storage", "network", "display", "multimedia",
        "memory", "bridge", "communication", "system", "input", "dock",
        "processor", "serial bus", "wireless", "intelligent I/O", "satellite",
        "encryption", "signal processing",
    };
    return class_code < sizeof(names) / sizeof(names[0]) ? names[class_code] : "unknown";
}

bool acpi_available(void) { return found_acpi; }
uint32_t acpi_table_count(void) { return found_acpi_tables; }
