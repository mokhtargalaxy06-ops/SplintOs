# Hardware support matrix

| Area | Supported | Notes |
|---|---|---|
| CPU | 32-bit x86 | GRUB Multiboot, GDT, IDT, legacy PIC/PIT |
| Graphics | VBE linear framebuffer | 800x600x32 software compositor |
| Keyboard/mouse | PS/2 | IRQ-driven with polling fallback |
| Serial | 16550-compatible COM1 | Diagnostics and command shell |
| Bus discovery | PCI configuration mechanism 1 | Up to 64 recorded functions |
| Firmware | ACPI RSDP/RSDT discovery | Table interpretation incomplete |
| Ethernet | Realtek RTL8139 | ARP, IPv4, ICMP, UDP, DHCP |
| Storage | RAM filesystem | No persistent block driver yet |

USB, PCIe ECAM, AHCI, NVMe, Wi-Fi, audio, Bluetooth, and accelerated GPUs are
not supported. Add a tested driver before changing this table.
