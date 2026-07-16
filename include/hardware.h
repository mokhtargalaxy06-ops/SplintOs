#ifndef SPLINTOS_HARDWARE_H
#define SPLINTOS_HARDWARE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct pci_device {
    uint8_t bus, slot, function;
    uint16_t vendor_id, device_id;
    uint8_t class_code, subclass, programming_interface;
    uint8_t interrupt_line;
    uint32_t bars[6];
};

void hardware_init(void);
size_t pci_device_count(void);
const struct pci_device *pci_device_get(size_t index);
const struct pci_device *pci_find_device(uint16_t vendor, uint16_t device);
uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t function,
                        uint8_t offset, uint32_t value);
void pci_enable_device(const struct pci_device *device, bool io, bool memory,
                       bool bus_master);
const char *pci_class_name(uint8_t class_code);
bool acpi_available(void);
uint32_t acpi_table_count(void);

#endif
