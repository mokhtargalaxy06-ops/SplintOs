CROSS ?=
CC := $(CROSS)gcc
LD := $(CROSS)ld

BUILD := build
ISO_ROOT := $(BUILD)/iso
KERNEL := $(BUILD)/splintos.bin
ISO := $(BUILD)/splintos.iso
QEMU := ./scripts/qemu-clean.sh qemu-system-i386

CFLAGS := -m32 -std=gnu11 -ffreestanding -fno-pie -fstack-protector-strong \
	-mstack-protector-guard=global \
	-Wall -Wextra -Werror -O2 -Iinclude
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib

OBJECTS := $(BUILD)/boot.o $(BUILD)/kernel.o $(BUILD)/network.o $(BUILD)/gui.o \
	$(BUILD)/devices.o $(BUILD)/interrupt_stubs.o $(BUILD)/interrupts.o
OBJECTS += $(BUILD)/memory.o
OBJECTS += $(BUILD)/scheduler.o
OBJECTS += $(BUILD)/filesystem.o
OBJECTS += $(BUILD)/applications.o
OBJECTS += $(BUILD)/hardware.o
OBJECTS += $(BUILD)/security.o

.PHONY: all iso run clean check test test-boot debug toolchain

all: $(KERNEL)

$(BUILD):
	mkdir -p $@

$(BUILD)/boot.o: src/boot.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.o: src/kernel.c include/kernel.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/network.o: src/network.c include/network.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/gui.o: src/gui.c include/gui.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/devices.o: src/devices.c include/devices.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/interrupt_stubs.o: src/interrupt_stubs.S | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/interrupts.o: src/interrupts.c include/interrupts.h include/devices.h include/network.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/memory.o: src/memory.c include/memory.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/scheduler.o: src/scheduler.c include/scheduler.h include/interrupts.h include/memory.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/filesystem.o: src/filesystem.c include/filesystem.h include/memory.h include/devices.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/applications.o: src/applications.c include/applications.h include/filesystem.h include/scheduler.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/hardware.o: src/hardware.c include/hardware.h include/devices.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/security.o: src/security.c include/security.h include/scheduler.h include/devices.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)
	@echo "Built $@"

check: $(KERNEL)
	@command -v grub-file >/dev/null || { echo "grub-file is required for this check"; exit 1; }
	@grub-file --is-x86-multiboot $(KERNEL)
	@echo "Multiboot header verified"
	@./scripts/static-check.sh $(KERNEL)

test: check
	@echo "All static kernel checks passed"

iso: $(ISO)

$(ISO): $(KERNEL) grub/grub.cfg
	@command -v grub-mkrescue >/dev/null || { echo "grub-mkrescue is required"; exit 1; }
	@command -v mformat >/dev/null || { echo "mformat is required; install the mtools package"; exit 1; }
	@test -d /usr/lib/grub/i386-pc || { echo "GRUB i386-pc modules are required; install grub-pc-bin"; exit 1; }
	mkdir -p $(ISO_ROOT)/boot/grub
	cp $(KERNEL) $(ISO_ROOT)/boot/splintos.bin
	cp grub/grub.cfg $(ISO_ROOT)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISO_ROOT)

run: $(ISO)
	@command -v qemu-system-i386 >/dev/null || { echo "qemu-system-i386 is required"; exit 1; }
	$(QEMU) -cdrom $(ISO) -nic user,model=rtl8139 -serial stdio

test-boot: $(ISO)
	@command -v qemu-system-i386 >/dev/null || { echo "qemu-system-i386 is required"; exit 1; }
	@QEMU="$(QEMU)" ./scripts/boot-test.sh $(ISO) $(BUILD)/boot-test.log

debug: $(ISO)
	@command -v qemu-system-i386 >/dev/null || { echo "qemu-system-i386 is required"; exit 1; }
	$(QEMU) -cdrom $(ISO) -nic user,model=rtl8139 -serial stdio -s -S

toolchain:
	@echo "CC: $$($(CC) --version | head -1)"
	@echo "LD: $$($(LD) --version | head -1)"
	@echo "Prefix: $(if $(CROSS),$(CROSS),host tools)"

clean:
	rm -rf $(BUILD)
