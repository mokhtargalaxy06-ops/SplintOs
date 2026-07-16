CROSS ?=
CC := $(CROSS)gcc
LD := $(CROSS)ld

BUILD := build
ISO_ROOT := $(BUILD)/iso
KERNEL := $(BUILD)/splintos.bin
ISO := $(BUILD)/splintos.iso
USER_BUILD := $(BUILD)/user
USER_HELLO := $(USER_BUILD)/hello.elf
USER_CAT := $(USER_BUILD)/cat.elf
USER_RUNNER := $(USER_BUILD)/runner.elf
USER_SHELL := $(USER_BUILD)/sh.elf
USER_FDTEST := $(USER_BUILD)/fdtest.elf
USER_ECHO := $(USER_BUILD)/echo.elf
USER_PIPETEST := $(USER_BUILD)/pipetest.elf
USER_WC := $(USER_BUILD)/wc.elf
USER_LS := $(USER_BUILD)/ls.elf
USER_MEM := $(USER_BUILD)/mem.elf
USER_UPTIME := $(USER_BUILD)/uptime.elf
USER_PS := $(USER_BUILD)/ps.elf
USER_HEAPTEST := $(USER_BUILD)/heaptest.elf
USER_DISK := $(USER_BUILD)/disk.elf
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
OBJECTS += $(BUILD)/syscall.o $(BUILD)/elf.o $(BUILD)/user_hello_blob.o
OBJECTS += $(BUILD)/block.o
OBJECTS += $(BUILD)/partition.o
OBJECTS += $(BUILD)/diskfs.o
OBJECTS += $(BUILD)/virtio_block.o

.PHONY: all iso run clean check check-user test test-boot test-storage debug toolchain

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

$(BUILD)/devices.o: src/devices.c include/devices.h include/scheduler.h | $(BUILD)
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

$(BUILD)/syscall.o: src/syscall.c include/syscall.h include/interrupts.h include/memory.h include/scheduler.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/elf.o: src/elf.c include/elf.h include/filesystem.h include/memory.h include/scheduler.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/block.o: src/block.c include/block.h include/devices.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/partition.o: src/partition.c include/partition.h include/block.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/diskfs.o: src/diskfs.c include/diskfs.h include/partition.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/virtio_block.o: src/virtio_block.c include/virtio_block.h include/block.h include/hardware.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(USER_BUILD):
	mkdir -p $@

USER_CFLAGS := -m32 -std=gnu11 -ffreestanding -fno-pie -fno-stack-protector \
	-Wall -Wextra -Werror -O2 -Iuser/include

$(USER_BUILD)/crt0.o: user/crt0.S | $(USER_BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD)/syscall.o: user/libc/syscall.S | $(USER_BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD)/hello.o: user/programs/hello.c user/include/splint/syscall.h | $(USER_BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD)/cat.o: user/programs/cat.c user/include/splint/syscall.h | $(USER_BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD)/runner.o: user/programs/runner.c user/include/splint/syscall.h | $(USER_BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD)/sh.o: user/programs/sh.c user/include/splint/syscall.h | $(USER_BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD)/fdtest.o: user/programs/fdtest.c user/include/splint/syscall.h | $(USER_BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD)/echo.o: user/programs/echo.c user/include/splint/syscall.h | $(USER_BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD)/pipetest.o: user/programs/pipetest.c user/include/splint/syscall.h | $(USER_BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD)/wc.o: user/programs/wc.c user/include/splint/syscall.h | $(USER_BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD)/ls.o: user/programs/ls.c user/include/splint/syscall.h | $(USER_BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD)/mem.o: user/programs/mem.c user/include/splint/syscall.h | $(USER_BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD)/uptime.o: user/programs/uptime.c user/include/splint/syscall.h | $(USER_BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD)/ps.o: user/programs/ps.c user/include/splint/syscall.h | $(USER_BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD)/heap.o: user/libc/heap.c user/include/splint/stdlib.h user/include/splint/syscall.h | $(USER_BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD)/dns.o: user/libc/dns.c user/include/splint/net.h user/include/splint/syscall.h | $(USER_BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD)/heaptest.o: user/programs/heaptest.c user/include/splint/stdlib.h user/include/splint/syscall.h | $(USER_BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_BUILD)/disk.o: user/programs/disk.c user/include/splint/syscall.h | $(USER_BUILD)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_HELLO): $(USER_BUILD)/crt0.o $(USER_BUILD)/syscall.o $(USER_BUILD)/hello.o user/linker.ld
	$(LD) -m elf_i386 -T user/linker.ld -nostdlib -o $@ $(filter %.o,$^)

$(USER_CAT): $(USER_BUILD)/crt0.o $(USER_BUILD)/syscall.o $(USER_BUILD)/cat.o user/linker.ld
	$(LD) -m elf_i386 -T user/linker.ld -nostdlib -o $@ $(filter %.o,$^)

$(USER_RUNNER): $(USER_BUILD)/crt0.o $(USER_BUILD)/syscall.o $(USER_BUILD)/runner.o user/linker.ld
	$(LD) -m elf_i386 -T user/linker.ld -nostdlib -o $@ $(filter %.o,$^)

$(USER_SHELL): $(USER_BUILD)/crt0.o $(USER_BUILD)/syscall.o $(USER_BUILD)/sh.o user/linker.ld
	$(LD) -m elf_i386 -T user/linker.ld -nostdlib -o $@ $(filter %.o,$^)

$(USER_FDTEST): $(USER_BUILD)/crt0.o $(USER_BUILD)/syscall.o $(USER_BUILD)/dns.o $(USER_BUILD)/fdtest.o user/linker.ld
	$(LD) -m elf_i386 -T user/linker.ld -nostdlib -o $@ $(filter %.o,$^)

$(USER_ECHO): $(USER_BUILD)/crt0.o $(USER_BUILD)/syscall.o $(USER_BUILD)/echo.o user/linker.ld
	$(LD) -m elf_i386 -T user/linker.ld -nostdlib -o $@ $(filter %.o,$^)

$(USER_PIPETEST): $(USER_BUILD)/crt0.o $(USER_BUILD)/syscall.o $(USER_BUILD)/pipetest.o user/linker.ld
	$(LD) -m elf_i386 -T user/linker.ld -nostdlib -o $@ $(filter %.o,$^)

$(USER_WC): $(USER_BUILD)/crt0.o $(USER_BUILD)/syscall.o $(USER_BUILD)/wc.o user/linker.ld
	$(LD) -m elf_i386 -T user/linker.ld -nostdlib -o $@ $(filter %.o,$^)

$(USER_LS): $(USER_BUILD)/crt0.o $(USER_BUILD)/syscall.o $(USER_BUILD)/ls.o user/linker.ld
	$(LD) -m elf_i386 -T user/linker.ld -nostdlib -o $@ $(filter %.o,$^)

$(USER_MEM): $(USER_BUILD)/crt0.o $(USER_BUILD)/syscall.o $(USER_BUILD)/mem.o user/linker.ld
	$(LD) -m elf_i386 -T user/linker.ld -nostdlib -o $@ $(filter %.o,$^)

$(USER_UPTIME): $(USER_BUILD)/crt0.o $(USER_BUILD)/syscall.o $(USER_BUILD)/uptime.o user/linker.ld
	$(LD) -m elf_i386 -T user/linker.ld -nostdlib -o $@ $(filter %.o,$^)

$(USER_PS): $(USER_BUILD)/crt0.o $(USER_BUILD)/syscall.o $(USER_BUILD)/ps.o user/linker.ld
	$(LD) -m elf_i386 -T user/linker.ld -nostdlib -o $@ $(filter %.o,$^)

$(USER_HEAPTEST): $(USER_BUILD)/crt0.o $(USER_BUILD)/syscall.o $(USER_BUILD)/heap.o $(USER_BUILD)/heaptest.o user/linker.ld
	$(LD) -m elf_i386 -T user/linker.ld -nostdlib -o $@ $(filter %.o,$^)

$(USER_DISK): $(USER_BUILD)/crt0.o $(USER_BUILD)/syscall.o $(USER_BUILD)/disk.o user/linker.ld
	$(LD) -m elf_i386 -T user/linker.ld -nostdlib -o $@ $(filter %.o,$^)

$(BUILD)/user_hello_blob.o: $(USER_HELLO) $(USER_CAT) $(USER_RUNNER) $(USER_SHELL) $(USER_FDTEST) $(USER_ECHO) $(USER_PIPETEST) $(USER_WC) $(USER_LS) $(USER_MEM) $(USER_UPTIME) $(USER_PS) $(USER_HEAPTEST) $(USER_DISK)
	$(CC) $(CFLAGS) -c src/user_blob.S -o $@

$(KERNEL): $(OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)
	@echo "Built $@"

check: $(KERNEL)
	@command -v grub-file >/dev/null || { echo "grub-file is required for this check"; exit 1; }
	@grub-file --is-x86-multiboot $(KERNEL)
	@echo "Multiboot header verified"
	@./scripts/static-check.sh $(KERNEL)
	@grep -q 'SplintOS (recovery)' grub/grub.cfg

check-user: $(USER_HELLO) $(USER_CAT) $(USER_RUNNER) $(USER_SHELL) $(USER_FDTEST) $(USER_ECHO) $(USER_PIPETEST) $(USER_WC) $(USER_LS) $(USER_MEM) $(USER_UPTIME) $(USER_PS) $(USER_HEAPTEST) $(USER_DISK)
	@readelf -h $(USER_HELLO) | grep -q 'Class:.*ELF32'
	@readelf -h $(USER_HELLO) | grep -q 'Type:.*EXEC'
	@readelf -h $(USER_HELLO) | grep -q 'Machine:.*Intel 80386'
	@readelf -h $(USER_CAT) | grep -q 'Type:.*EXEC'
	@readelf -h $(USER_RUNNER) | grep -q 'Type:.*EXEC'
	@readelf -h $(USER_SHELL) | grep -q 'Type:.*EXEC'
	@readelf -h $(USER_FDTEST) | grep -q 'Type:.*EXEC'
	@readelf -h $(USER_ECHO) | grep -q 'Type:.*EXEC'
	@readelf -h $(USER_PIPETEST) | grep -q 'Type:.*EXEC'
	@readelf -h $(USER_WC) | grep -q 'Type:.*EXEC'
	@readelf -h $(USER_LS) | grep -q 'Type:.*EXEC'
	@readelf -h $(USER_MEM) | grep -q 'Type:.*EXEC'
	@readelf -h $(USER_UPTIME) | grep -q 'Type:.*EXEC'
	@readelf -h $(USER_PS) | grep -q 'Type:.*EXEC'
	@readelf -h $(USER_HEAPTEST) | grep -q 'Type:.*EXEC'
	@readelf -h $(USER_DISK) | grep -q 'Type:.*EXEC'
	@echo "User ELF format verified"

test: check check-user
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

test-storage: $(ISO)
	@command -v qemu-system-i386 >/dev/null || { echo "qemu-system-i386 is required"; exit 1; }
	@QEMU="$(QEMU)" ./scripts/storage-test.sh $(ISO) $(BUILD)/storage-test.img $(BUILD)/storage-test

debug: $(ISO)
	@command -v qemu-system-i386 >/dev/null || { echo "qemu-system-i386 is required"; exit 1; }
	$(QEMU) -cdrom $(ISO) -nic user,model=rtl8139 -serial stdio -s -S

toolchain:
	@echo "CC: $$($(CC) --version | head -1)"
	@echo "LD: $$($(LD) --version | head -1)"
	@echo "Prefix: $(if $(CROSS),$(CROSS),host tools)"

clean:
	rm -rf $(BUILD)
