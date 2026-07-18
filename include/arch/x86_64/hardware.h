#ifndef SPLINTOS_ARCH_X86_64_HARDWARE_H
#define SPLINTOS_ARCH_X86_64_HARDWARE_H

#include <stddef.h>
#include <stdint.h>

struct x86_64_pci_device {
    uint8_t bus, slot, function;
    uint16_t vendor_id, device_id;
    uint8_t class_code, subclass, programming_interface, interrupt_line;
    uint32_t bars[6];
};

int x86_64_hardware_init(void);
size_t x86_64_pci_device_count(void);
const struct x86_64_pci_device *x86_64_pci_device_get(size_t index);
uint32_t x86_64_acpi_table_count(void);
const struct x86_64_pci_device *x86_64_pci_find(uint16_t vendor, uint16_t device);
void x86_64_pci_enable(const struct x86_64_pci_device *device,
                       int io, int memory, int bus_master);

#endif
