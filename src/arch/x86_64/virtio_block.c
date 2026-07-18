#include <stddef.h>
#include <stdint.h>
#include "arch/x86_64/hardware.h"
#include "arch/x86_64/virtio_block.h"
#include "arch/x86_64/dma.h"

#define PACKED __attribute__((packed))
#define ALIGNED(value) __attribute__((aligned(value)))
enum { VENDOR = 0x1af4, DEVICE = 0x1001, QUEUE_BYTES = 16384,
       QUEUE_MAX = 256, DESC_NEXT = 1, DESC_WRITE = 2,
       REQUEST_READ = 0, REQUEST_WRITE = 1, REQUEST_FLUSH = 4,
       STATUS_ACK = 1, STATUS_DRIVER = 2, STATUS_OK = 4, STATUS_FAILED = 128,
       FEATURE_FLUSH = 9 };
struct PACKED descriptor { uint64_t address; uint32_t length; uint16_t flags, next; };
struct PACKED request { uint32_t type, reserved; uint64_t sector; };
static uint8_t queue[QUEUE_BYTES] ALIGNED(4096);
static struct request request_header;
static uint8_t request_status;
static uint16_t io_base, queue_size, last_used;
static uint64_t capacity;
static int ready, flush_supported;

static inline void outb(uint16_t port, uint8_t value)
{ __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port)); }
static inline void outw(uint16_t port, uint16_t value)
{ __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port)); }
static inline void outl(uint16_t port, uint32_t value)
{ __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port)); }
static inline uint8_t inb(uint16_t port)
{ uint8_t value; __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port)); return value; }
static inline uint16_t inw(uint16_t port)
{ uint16_t value; __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port)); return value; }
static inline uint32_t inl(uint16_t port)
{ uint32_t value; __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port)); return value; }
static uintptr_t align_up(uintptr_t value, uintptr_t alignment)
{ return (value + alignment - 1U) & ~(alignment - 1U); }

static int submit(uint32_t type, uint64_t sector, void *buffer, size_t bytes)
{
    if (!ready || (bytes != 0 && buffer == NULL) || bytes > UINT32_MAX) return 0;
    struct descriptor *descriptors = (struct descriptor *)queue;
    volatile uint16_t *available = (volatile uint16_t *)(queue + sizeof(*descriptors) * queue_size);
    volatile uint16_t *used = (volatile uint16_t *)align_up(
        (uintptr_t)(available + 2 + queue_size), 4096);
    request_header = (struct request){type, 0, sector}; request_status = 0xff;
    descriptors[0] = (struct descriptor){(uintptr_t)&request_header,
        sizeof(request_header), DESC_NEXT, 1};
    if (bytes == 0) descriptors[1] = (struct descriptor){(uintptr_t)&request_status,
        1, DESC_WRITE, 0};
    else {
        descriptors[1] = (struct descriptor){(uintptr_t)buffer, (uint32_t)bytes,
            (uint16_t)(DESC_NEXT | (type == REQUEST_READ ? DESC_WRITE : 0)), 2};
        descriptors[2] = (struct descriptor){(uintptr_t)&request_status, 1, DESC_WRITE, 0};
    }
    uint16_t index = available[1]; available[2 + index % queue_size] = 0;
    __asm__ volatile ("" : : : "memory"); available[1] = (uint16_t)(index + 1U);
    outw((uint16_t)(io_base + 0x10), 0);
    for (uint32_t spin = 0; spin < 10000000; ++spin) {
        __asm__ volatile ("" : : : "memory");
        if (used[1] != last_used) {
            last_used = used[1]; (void)inb((uint16_t)(io_base + 0x13));
            return request_status == 0;
        }
    }
    return 0;
}

int x86_64_virtio_block_init(void)
{
    const struct x86_64_pci_device *device = x86_64_pci_find(VENDOR, DEVICE);
    if (device == NULL || (device->bars[0] & 1U) == 0 ||
        !x86_64_dma_address_valid((uintptr_t)queue, sizeof(queue), UINT32_MAX) ||
        !x86_64_dma_address_valid((uintptr_t)&request_header,
                                  sizeof(request_header), UINT32_MAX)) return 0;
    io_base = (uint16_t)(device->bars[0] & ~3U); x86_64_pci_enable(device, 1, 0, 1);
    outb((uint16_t)(io_base + 0x12), 0);
    outb((uint16_t)(io_base + 0x12), STATUS_ACK);
    outb((uint16_t)(io_base + 0x12), STATUS_ACK | STATUS_DRIVER);
    uint32_t features = inl(io_base); flush_supported = (features & (1U << FEATURE_FLUSH)) != 0;
    outl((uint16_t)(io_base + 4), flush_supported ? 1U << FEATURE_FLUSH : 0);
    outw((uint16_t)(io_base + 0x0e), 0); queue_size = inw((uint16_t)(io_base + 0x0c));
    if (queue_size < 3 || queue_size > QUEUE_MAX) goto fail;
    for (size_t i = 0; i < sizeof(queue); ++i) queue[i] = 0;
    outl((uint16_t)(io_base + 8), (uint32_t)((uintptr_t)queue >> 12));
    capacity = inl((uint16_t)(io_base + 0x14));
    capacity |= (uint64_t)inl((uint16_t)(io_base + 0x18)) << 32;
    if (capacity < 2) goto fail;
    last_used = 0; ready = 1;
    outb((uint16_t)(io_base + 0x12), STATUS_ACK | STATUS_DRIVER | STATUS_OK);
    return 1;
fail:
    ready = 0; outb((uint16_t)(io_base + 0x12), STATUS_FAILED); return 0;
}

int x86_64_virtio_block_conformance_test(void)
{
    if (!ready || capacity < 2) return 0;
    uint8_t original[512], pattern[512], result[512];
    if (!submit(REQUEST_READ, 1, original, sizeof(original))) return 0;
    for (size_t i = 0; i < sizeof(pattern); ++i) pattern[i] = (uint8_t)(i ^ 0x6dU);
    if (!submit(REQUEST_WRITE, 1, pattern, sizeof(pattern)) ||
        (flush_supported && !submit(REQUEST_FLUSH, 0, NULL, 0)) ||
        !submit(REQUEST_READ, 1, result, sizeof(result))) return 0;
    for (size_t i = 0; i < sizeof(pattern); ++i)
        if (pattern[i] != result[i]) return 0;
    if (!submit(REQUEST_WRITE, 1, original, sizeof(original)) ||
        (flush_supported && !submit(REQUEST_FLUSH, 0, NULL, 0))) return 0;
    return 1;
}

int x86_64_virtio_block_read(uint64_t sector, void *buffer, size_t sectors)
{
    if (sectors == 0 || sectors > SIZE_MAX / 512U || sector >= capacity ||
        sectors > capacity - sector) return 0;
    return submit(REQUEST_READ, sector, buffer, sectors * 512U);
}

int x86_64_virtio_block_write(uint64_t sector, const void *buffer, size_t sectors)
{
    if (sectors == 0 || sectors > SIZE_MAX / 512U || sector >= capacity ||
        sectors > capacity - sector) return 0;
    return submit(REQUEST_WRITE, sector, (void *)buffer, sectors * 512U);
}

int x86_64_virtio_block_flush(void)
{ return !flush_supported || submit(REQUEST_FLUSH, 0, NULL, 0); }
