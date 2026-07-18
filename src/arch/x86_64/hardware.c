#include <stddef.h>
#include <stdint.h>

#include "arch/x86_64/hardware.h"

#define PACKED __attribute__((packed))

enum { PCI_ADDRESS = 0xcf8, PCI_DATA = 0xcfc, MAX_PCI_DEVICES = 64,
       MAX_ACPI_TABLE_LENGTH = 1024 * 1024 };

struct PACKED rsdp_v1 {
    uint8_t signature[8], checksum, oem_id[6], revision;
    uint32_t rsdt_address;
};
struct PACKED acpi_header {
    uint8_t signature[4];
    uint32_t length;
    uint8_t revision, checksum, oem_id[6], oem_table_id[8];
    uint32_t oem_revision, creator_id, creator_revision;
};

static struct x86_64_pci_device devices[MAX_PCI_DEVICES];
static size_t device_count;
static uint32_t table_count;

static inline void outl(uint16_t port, uint32_t value)
{ __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port)); }
static inline uint32_t inl(uint16_t port)
{ uint32_t value; __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port)); return value; }

static uint16_t physical_read16(uintptr_t address)
{
    uint16_t value;
    __asm__ volatile ("movw (%1), %0" : "=r"(value) : "r"(address) : "memory");
    return value;
}

static uint32_t config_read(uint8_t bus, uint8_t slot, uint8_t function,
                            uint8_t offset)
{
    uint32_t address = UINT32_C(0x80000000) | (uint32_t)bus << 16 |
        (uint32_t)slot << 11 | (uint32_t)function << 8 | (offset & 0xfcU);
    outl(PCI_ADDRESS, address);
    return inl(PCI_DATA);
}

static void config_write(uint8_t bus, uint8_t slot, uint8_t function,
                         uint8_t offset, uint32_t value)
{
    uint32_t address = UINT32_C(0x80000000) | (uint32_t)bus << 16 |
        (uint32_t)slot << 11 | (uint32_t)function << 8 | (offset & 0xfcU);
    outl(PCI_ADDRESS, address); outl(PCI_DATA, value);
}

static void record_device(uint8_t bus, uint8_t slot, uint8_t function)
{
    uint32_t id = config_read(bus, slot, function, 0);
    if ((uint16_t)id == UINT16_MAX || device_count == MAX_PCI_DEVICES) return;
    struct x86_64_pci_device *device = &devices[device_count++];
    *device = (struct x86_64_pci_device){0};
    device->bus = bus; device->slot = slot; device->function = function;
    device->vendor_id = (uint16_t)id; device->device_id = (uint16_t)(id >> 16);
    uint32_t class_data = config_read(bus, slot, function, 8);
    device->programming_interface = (uint8_t)(class_data >> 8);
    device->subclass = (uint8_t)(class_data >> 16);
    device->class_code = (uint8_t)(class_data >> 24);
    for (uint8_t bar = 0; bar < 6; ++bar)
        device->bars[bar] = config_read(bus, slot, function,
                                       (uint8_t)(0x10 + bar * 4));
    device->interrupt_line = (uint8_t)config_read(bus, slot, function, 0x3c);
}

static void enumerate_pci(void)
{
    device_count = 0;
    for (uint16_t bus = 0; bus < 256; ++bus)
        for (uint8_t slot = 0; slot < 32; ++slot) {
            uint32_t id = config_read((uint8_t)bus, slot, 0, 0);
            if ((uint16_t)id == UINT16_MAX) continue;
            uint8_t header = (uint8_t)(config_read((uint8_t)bus, slot, 0, 0x0c) >> 16);
            uint8_t functions = (header & 0x80U) != 0 ? 8 : 1;
            for (uint8_t function = 0; function < functions; ++function)
                record_device((uint8_t)bus, slot, function);
        }
}

static int bytes_equal(const uint8_t *left, const uint8_t *right, size_t count)
{
    for (size_t i = 0; i < count; ++i) if (left[i] != right[i]) return 0;
    return 1;
}

static int checksum_valid(const void *pointer, size_t length)
{
    const uint8_t *bytes = pointer;
    uint8_t sum = 0;
    for (size_t i = 0; i < length; ++i) sum = (uint8_t)(sum + bytes[i]);
    return sum == 0;
}

static const struct rsdp_v1 *scan_rsdp(uintptr_t start, uintptr_t end)
{
    static const uint8_t signature[8] = {'R','S','D',' ','P','T','R',' '};
    start = (start + 15U) & ~(uintptr_t)15U;
    for (uintptr_t address = start; address <= end - sizeof(struct rsdp_v1);
         address += 16) {
        const struct rsdp_v1 *rsdp = (const struct rsdp_v1 *)address;
        if (bytes_equal(rsdp->signature, signature, sizeof(signature)) &&
            checksum_valid(rsdp, sizeof(*rsdp))) return rsdp;
    }
    return NULL;
}

static int discover_acpi(void)
{
    uint16_t ebda_segment = physical_read16(0x40e);
    uintptr_t ebda = (uintptr_t)ebda_segment << 4;
    const struct rsdp_v1 *rsdp = ebda >= 0x400 && ebda <= 0x9fc00
        ? scan_rsdp(ebda, ebda + 1024) : NULL;
    if (rsdp == NULL) rsdp = scan_rsdp(0xe0000, 0x100000);
    if (rsdp == NULL || rsdp->rsdt_address < 0x1000) return 0;
    const struct acpi_header *rsdt =
        (const struct acpi_header *)(uintptr_t)rsdp->rsdt_address;
    static const uint8_t rsdt_signature[4] = {'R','S','D','T'};
    if (!bytes_equal(rsdt->signature, rsdt_signature, 4) ||
        rsdt->length < sizeof(*rsdt) || rsdt->length > MAX_ACPI_TABLE_LENGTH ||
        (uint64_t)rsdp->rsdt_address + rsdt->length > UINT32_MAX ||
        !checksum_valid(rsdt, rsdt->length)) return 0;
    table_count = (rsdt->length - sizeof(*rsdt)) / sizeof(uint32_t);
    return 1;
}

int x86_64_hardware_init(void)
{
    enumerate_pci();
    table_count = 0;
    return device_count != 0 && discover_acpi();
}

size_t x86_64_pci_device_count(void) { return device_count; }
const struct x86_64_pci_device *x86_64_pci_device_get(size_t index)
{ return index < device_count ? &devices[index] : NULL; }
uint32_t x86_64_acpi_table_count(void) { return table_count; }

const struct x86_64_pci_device *x86_64_pci_find(uint16_t vendor, uint16_t device)
{
    for (size_t i = 0; i < device_count; ++i)
        if (devices[i].vendor_id == vendor && devices[i].device_id == device)
            return &devices[i];
    return NULL;
}

void x86_64_pci_enable(const struct x86_64_pci_device *device,
                       int io, int memory, int bus_master)
{
    if (device == NULL) return;
    uint32_t command = config_read(device->bus, device->slot, device->function, 4);
    if (io) command |= 1U;
    if (memory) command |= 2U;
    if (bus_master) command |= 4U;
    config_write(device->bus, device->slot, device->function, 4, command);
}
