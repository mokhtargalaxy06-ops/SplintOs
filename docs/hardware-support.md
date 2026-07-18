# Hardware support matrix

| Area | Supported | Notes |
|---|---|---|
| CPU | 32-bit x86 | GRUB Multiboot, GDT, IDT, legacy PIC/PIT |
| Graphics | VBE linear framebuffer | 800x600x32 software compositor |
| Keyboard/mouse | PS/2 | IRQ-driven with polling fallback; desktop Left/Right/Tab and Files Up/Down/Enter/Left navigation |
| Serial | 16550-compatible COM1 | Diagnostics and command shell |
| Bus discovery | PCI configuration mechanism 1 | Up to 64 recorded functions |
| Firmware | ACPI RSDP/RSDT discovery | Table interpretation incomplete |
| Ethernet | Realtek RTL8139 | ARP, IPv4, ICMP, UDP, DHCP |
| Storage | Legacy VirtIO block and `ramblk0` | QEMU persistence; polling queue, 512-byte sectors |

The parallel x86_64 migration profile currently verifies PCI/ACPI discovery,
a Multiboot linear framebuffer and software dirty-region compositor, plus
portable block, input, VFS, and UDP foundations. Its PS/2 keyboard and mouse IRQ
paths are online with bounded parsing. Its RTL8139 receive/transmit DMA and IRQ
paths are verified by a dedicated QEMU hardware profile. DHCP completes an
end-to-end Discover/Offer/Request/ACK exchange through that path. The x86_64
legacy VirtIO binding passes a disposable-disk
write, flush, reread, verification, and restoration profile. A checksummed VFS
record is created through that device and recovered on a second boot using the
same image. Full SPLFS5 compatibility remains pending.

USB, PCIe ECAM, AHCI, NVMe, Wi-Fi, audio, Bluetooth, and accelerated GPUs are
not supported. Add a tested driver before changing this table.
# Missing-device behavior

Legacy VirtIO block absence and unsupported BAR layouts are reported over the
serial diagnostic channel. SplintOS continues with the bounded volatile
`ramblk0` conformance device; it does not claim persistence in that mode.
Automated missing-device QEMU coverage requires this fallback and a complete
Ring 3 boot without either VirtIO storage or an RTL8139 adapter.
