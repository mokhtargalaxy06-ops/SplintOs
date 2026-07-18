#include "virtio_block.h"
#include "arch/x86/io.h"
#include "arch/x86/cpu.h"

#include "block.h"
#include "devices.h"
#include "hardware.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PACKED __attribute__((packed))

enum {
    VIRTIO_VENDOR = 0x1AF4,
    VIRTIO_BLOCK_LEGACY = 0x1001,
    VIRTIO_HOST_FEATURES = 0x00,
    VIRTIO_GUEST_FEATURES = 0x04,
    VIRTIO_QUEUE_ADDRESS = 0x08,
    VIRTIO_QUEUE_SIZE = 0x0C,
    VIRTIO_QUEUE_SELECT = 0x0E,
    VIRTIO_QUEUE_NOTIFY = 0x10,
    VIRTIO_DEVICE_STATUS = 0x12,
    VIRTIO_ISR_STATUS = 0x13,
    VIRTIO_CONFIG = 0x14,
    VIRTIO_STATUS_ACKNOWLEDGE = 1,
    VIRTIO_STATUS_DRIVER = 2,
    VIRTIO_STATUS_DRIVER_OK = 4,
    VIRTIO_STATUS_FAILED = 128,
    VIRTIO_DESC_NEXT = 1,
    VIRTIO_DESC_WRITE = 2,
    VIRTIO_BLOCK_F_FLUSH = 9,
    VIRTIO_BLOCK_READ = 0,
    VIRTIO_BLOCK_WRITE = 1,
    VIRTIO_BLOCK_FLUSH = 4,
    VIRTIO_QUEUE_MAX = 256,
    VIRTIO_QUEUE_BYTES = 16384,
    VIRTIO_TIMEOUT = 10000000,
};

struct PACKED virtq_descriptor {
    uint64_t address;
    uint32_t length;
    uint16_t flags, next;
};

struct PACKED virtio_block_request {
    uint32_t type, reserved;
    uint64_t sector;
};

struct virtio_state {
    uint16_t io_base, queue_size, last_used;
    bool flush_supported, ready;
    struct block_device device;
    struct virtio_block_request request;
    uint8_t request_status;
};

static struct virtio_state state;
static uint8_t queue_memory[VIRTIO_QUEUE_BYTES] __attribute__((aligned(4096)));

static void bytes_zero(void *buffer, size_t count)
{ uint8_t *bytes = buffer; while (count-- != 0) *bytes++ = 0; }

static uintptr_t align_up(uintptr_t value, uintptr_t alignment)
{ return (value + alignment - 1) & ~(alignment - 1); }

static int submit(uint32_t type, uint64_t sector, void *buffer, size_t bytes)
{
    if (!state.ready) return KERNEL_ERROR_BUSY;
    if (bytes != 0 && buffer == NULL) return KERNEL_ERROR_INVALID;
    struct virtq_descriptor *descriptors = (struct virtq_descriptor *)queue_memory;
    volatile uint16_t *available = (volatile uint16_t *)(queue_memory +
        sizeof(struct virtq_descriptor) * state.queue_size);
    volatile uint16_t *used = (volatile uint16_t *)align_up(
        (uintptr_t)(available + 2 + state.queue_size), 4096);

    state.request = (struct virtio_block_request){type, 0, sector};
    state.request_status = 0xFF;
    descriptors[0] = (struct virtq_descriptor){
        (uintptr_t)&state.request, sizeof(state.request), VIRTIO_DESC_NEXT, 1};
    if (bytes == 0) {
        descriptors[1] = (struct virtq_descriptor){
            (uintptr_t)&state.request_status, 1, VIRTIO_DESC_WRITE, 0};
    } else {
        descriptors[1] = (struct virtq_descriptor){
            (uintptr_t)buffer, bytes,
            (uint16_t)(VIRTIO_DESC_NEXT |
                       (type == VIRTIO_BLOCK_READ ? VIRTIO_DESC_WRITE : 0)), 2};
        descriptors[2] = (struct virtq_descriptor){
            (uintptr_t)&state.request_status, 1, VIRTIO_DESC_WRITE, 0};
    }

    uint16_t available_index = available[1];
    available[2 + available_index % state.queue_size] = 0;
    arch_compiler_barrier();
    available[1] = (uint16_t)(available_index + 1);
    outw((uint16_t)(state.io_base + VIRTIO_QUEUE_NOTIFY), 0);

    for (uint32_t spin = 0; spin < VIRTIO_TIMEOUT; ++spin) {
        arch_compiler_barrier();
        if (used[1] != state.last_used) {
            state.last_used = used[1];
            (void)inb((uint16_t)(state.io_base + VIRTIO_ISR_STATUS));
            if (state.request_status == 0) return KERNEL_OK;
            return state.request_status == 2
                ? KERNEL_ERROR_UNSUPPORTED : KERNEL_ERROR_IO;
        }
        arch_cpu_relax();
    }
    return KERNEL_ERROR_TIMEOUT;
}

static int virtio_read(struct block_device *device, uint64_t sector,
                       void *buffer, size_t count)
{
    (void)device;
    if (count > SIZE_MAX / 512) return KERNEL_ERROR_INVALID;
    return submit(VIRTIO_BLOCK_READ, sector, buffer, count * 512);
}

static int virtio_write(struct block_device *device, uint64_t sector,
                        const void *buffer, size_t count)
{
    (void)device;
    if (count > SIZE_MAX / 512) return KERNEL_ERROR_INVALID;
    return submit(VIRTIO_BLOCK_WRITE, sector, (void *)buffer, count * 512);
}

static int virtio_flush(struct block_device *device)
{
    (void)device;
    return state.flush_supported
        ? submit(VIRTIO_BLOCK_FLUSH, 0, NULL, 0) : KERNEL_OK;
}

static const struct block_operations operations = {
    virtio_read, virtio_write, virtio_flush
};

void virtio_block_init(void)
{
    const struct pci_device *pci = pci_find_device(VIRTIO_VENDOR, VIRTIO_BLOCK_LEGACY);
    if (pci == NULL) {
        serial_write("SplintOS: VirtIO block absent; using ramblk0\r\n");
        return;
    }
    if ((pci->bars[0] & 1U) == 0) {
        serial_write("SplintOS: VirtIO block has unsupported BAR; using ramblk0\r\n");
        return;
    }
    state = (struct virtio_state){0};
    state.io_base = (uint16_t)(pci->bars[0] & ~3U);
    pci_enable_device(pci, true, false, true);

    outb((uint16_t)(state.io_base + VIRTIO_DEVICE_STATUS), 0);
    outb((uint16_t)(state.io_base + VIRTIO_DEVICE_STATUS), VIRTIO_STATUS_ACKNOWLEDGE);
    outb((uint16_t)(state.io_base + VIRTIO_DEVICE_STATUS),
         VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);
    uint32_t host_features = inl((uint16_t)(state.io_base + VIRTIO_HOST_FEATURES));
    state.flush_supported = (host_features & (1U << VIRTIO_BLOCK_F_FLUSH)) != 0;
    outl((uint16_t)(state.io_base + VIRTIO_GUEST_FEATURES),
         state.flush_supported ? 1U << VIRTIO_BLOCK_F_FLUSH : 0);

    outw((uint16_t)(state.io_base + VIRTIO_QUEUE_SELECT), 0);
    state.queue_size = inw((uint16_t)(state.io_base + VIRTIO_QUEUE_SIZE));
    if (state.queue_size < 3 || state.queue_size > VIRTIO_QUEUE_MAX) goto fail;
    bytes_zero(queue_memory, sizeof(queue_memory));
    outl((uint16_t)(state.io_base + VIRTIO_QUEUE_ADDRESS),
         (uint32_t)((uintptr_t)queue_memory >> 12));

    uint64_t capacity = inl((uint16_t)(state.io_base + VIRTIO_CONFIG));
    capacity |= (uint64_t)inl((uint16_t)(state.io_base + VIRTIO_CONFIG + 4)) << 32;
    if (capacity == 0) goto fail;
    state.device = (struct block_device){
        "virtblk0", 512, capacity, false, &operations, &state};
    state.ready = true;
    outb((uint16_t)(state.io_base + VIRTIO_DEVICE_STATUS),
         VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_DRIVER_OK);
    if (block_register(&state.device) != 0) goto fail;
    serial_write("SplintOS: legacy VirtIO block device online\r\n");
    return;

fail:
    state.ready = false;
    outb((uint16_t)(state.io_base + VIRTIO_DEVICE_STATUS), VIRTIO_STATUS_FAILED);
    serial_write("SplintOS: VirtIO block initialization failed\r\n");
}
