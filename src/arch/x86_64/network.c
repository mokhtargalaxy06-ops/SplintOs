#include <stddef.h>
#include <stdint.h>
#include "arch/x86_64/network.h"
#include "arch/x86_64/hardware.h"
#include "arch/x86_64/timer.h"
#include "arch/x86_64/dma.h"

#define PACKED __attribute__((packed))
enum { SOCKET_COUNT = 8, QUEUE_DEPTH = 4, PAYLOAD_MAX = 600,
       ETH_IPV4 = 0x0800, IP_UDP = 17, RTL_VENDOR = 0x10ec,
       RTL_DEVICE = 0x8139, RX_RING = 8192, RX_STORAGE = 8192 + 16 + 1536,
       TX_SLOTS = 4, TX_CAPACITY = 1536 };
struct socket { uint8_t used, read, write, queued; uint16_t port;
    uint16_t source[QUEUE_DEPTH], length[QUEUE_DEPTH];
    uint8_t payload[QUEUE_DEPTH][PAYLOAD_MAX]; };
struct PACKED ethernet { uint8_t destination[6], source[6]; uint16_t type; };
struct PACKED ipv4 { uint8_t version_ihl, service; uint16_t total, id, fragment;
    uint8_t ttl, protocol; uint16_t checksum; uint8_t source[4], destination[4]; };
struct PACKED udp { uint16_t source, destination, length, checksum; };
struct PACKED bootp {
    uint8_t operation, hardware_type, hardware_length, hops;
    uint32_t transaction; uint16_t seconds, flags;
    uint8_t client_ip[4], offered_ip[4], server_ip[4], relay_ip[4];
    uint8_t client_hardware[16], server_name[64], boot_file[128];
    uint32_t cookie;
};
static struct socket sockets[SOCKET_COUNT];
static uint8_t rx_buffer[RX_STORAGE] __attribute__((aligned(256)));
static uint8_t tx_buffer[TX_SLOTS][TX_CAPACITY] __attribute__((aligned(16)));
static uint16_t rtl_io, rx_offset;
static uint8_t rtl_irq, tx_index, rtl_mac[6];
static int rtl_ready;
static uint8_t configured_ip[4];
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
static uint16_t swap16(uint16_t value) { return (uint16_t)(value << 8 | value >> 8); }
static uint16_t ipv4_checksum(const void *data, size_t length)
{
    const uint8_t *bytes = data; uint32_t sum = 0;
    while (length > 1) { sum += (uint16_t)((uint16_t)bytes[0] << 8 | bytes[1]); bytes += 2; length -= 2; }
    if (length != 0) sum += (uint16_t)bytes[0] << 8;
    while ((sum >> 16) != 0) sum = (sum & UINT32_C(0xffff)) + (sum >> 16);
    return swap16((uint16_t)~sum);
}

void x86_64_network_init(void)
{ for (size_t i = 0; i < SOCKET_COUNT; ++i) sockets[i] = (struct socket){0}; }
int x86_64_udp_open(uint16_t port)
{
    if (port == 0) return -1;
    for (size_t i = 0; i < SOCKET_COUNT; ++i) if (sockets[i].used && sockets[i].port == port) return -1;
    for (size_t i = 0; i < SOCKET_COUNT; ++i) if (!sockets[i].used) {
        sockets[i] = (struct socket){0}; sockets[i].used = 1; sockets[i].port = port; return (int)i;
    }
    return -1;
}
int x86_64_udp_close(int socket)
{
    if (socket < 0 || socket >= SOCKET_COUNT || !sockets[socket].used) return -1;
    sockets[socket] = (struct socket){0}; return 0;
}
static int enqueue(struct socket *target, uint16_t source, const void *data, size_t length)
{
    if (target->queued == QUEUE_DEPTH || length > PAYLOAD_MAX) return -1;
    uint8_t slot = target->write; const uint8_t *bytes = data;
    for (size_t i = 0; i < length; ++i) target->payload[slot][i] = bytes[i];
    target->source[slot] = source; target->length[slot] = (uint16_t)length;
    target->write = (uint8_t)((slot + 1U) % QUEUE_DEPTH); ++target->queued; return (int)length;
}
int x86_64_udp_send_loopback(int socket, uint16_t destination_port,
                             const void *data, size_t length)
{
    if (socket < 0 || socket >= SOCKET_COUNT || !sockets[socket].used ||
        data == NULL || length > PAYLOAD_MAX) return -1;
    for (size_t i = 0; i < SOCKET_COUNT; ++i)
        if (sockets[i].used && sockets[i].port == destination_port)
            return enqueue(&sockets[i], sockets[socket].port, data, length);
    return -1;
}
int x86_64_udp_receive(int socket, void *data, size_t capacity, uint16_t *source_port)
{
    if (socket < 0 || socket >= SOCKET_COUNT || !sockets[socket].used ||
        sockets[socket].queued == 0 || data == NULL) return -1;
    struct socket *entry = &sockets[socket]; uint8_t slot = entry->read;
    size_t length = entry->length[slot] < capacity ? entry->length[slot] : capacity;
    uint8_t *bytes = data;
    for (size_t i = 0; i < length; ++i) bytes[i] = entry->payload[slot][i];
    if (source_port != NULL) *source_port = entry->source[slot];
    entry->read = (uint8_t)((slot + 1U) % QUEUE_DEPTH); --entry->queued; return (int)length;
}
int x86_64_network_accept_frame(const void *frame, size_t length)
{
    if (frame == NULL || length < sizeof(struct ethernet) + sizeof(struct ipv4)) return 0;
    const struct ethernet *eth = frame;
    if (swap16(eth->type) != ETH_IPV4) return 0;
    const struct ipv4 *ip = (const struct ipv4 *)(eth + 1);
    size_t header = (size_t)(ip->version_ihl & 15U) * 4U; uint16_t total = swap16(ip->total);
    if ((ip->version_ihl >> 4) != 4 || header < sizeof(*ip) || total < header ||
        total > length - sizeof(*eth) || ip->protocol != IP_UDP ||
        (swap16(ip->fragment) & UINT16_C(0x3fff)) != 0 ||
        total < header + sizeof(struct udp)) return 0;
    const struct udp *udp = (const struct udp *)((const uint8_t *)ip + header);
    uint16_t udp_length = swap16(udp->length);
    if (udp_length < sizeof(*udp) || udp_length > total - header ||
        udp_length - sizeof(*udp) > PAYLOAD_MAX) return 0;
    for (size_t i = 0; i < SOCKET_COUNT; ++i)
        if (sockets[i].used && sockets[i].port == swap16(udp->destination))
            return enqueue(&sockets[i], swap16(udp->source), udp + 1,
                           udp_length - sizeof(*udp)) >= 0;
    return 0;
}
int x86_64_network_conformance_test(void)
{
    static const char message[] = "udp64"; char output[sizeof(message)] = {0}; uint16_t source = 0;
    x86_64_network_init(); int client = x86_64_udp_open(40000), server = x86_64_udp_open(53);
    if (client < 0 || server < 0 || x86_64_udp_open(53) >= 0 ||
        x86_64_udp_send_loopback(client, 53, message, sizeof(message)) != sizeof(message) ||
        x86_64_udp_receive(server, output, sizeof(output), &source) != sizeof(message) || source != 40000 ||
        x86_64_network_accept_frame(message, sizeof(message)) != 0) return 0;
    for (size_t i = 0; i < sizeof(message); ++i) if (message[i] != output[i]) return 0;
    return x86_64_udp_close(client) == 0 && x86_64_udp_close(server) == 0;
}

int x86_64_rtl8139_init(void)
{
    const struct x86_64_pci_device *device = x86_64_pci_find(RTL_VENDOR, RTL_DEVICE);
    if (device == NULL || (device->bars[0] & 1U) == 0 || device->interrupt_line >= 16 ||
        !x86_64_dma_address_valid((uintptr_t)rx_buffer, sizeof(rx_buffer), UINT32_MAX) ||
        !x86_64_dma_address_valid((uintptr_t)tx_buffer, sizeof(tx_buffer), UINT32_MAX)) return 0;
    rtl_io = (uint16_t)(device->bars[0] & ~3U); rtl_irq = device->interrupt_line;
    x86_64_pci_enable(device, 1, 0, 1);
    outb((uint16_t)(rtl_io + 0x52), 0);
    outb((uint16_t)(rtl_io + 0x37), 0x10);
    uint32_t spin;
    for (spin = 0; spin < 100000; ++spin)
        if ((inb((uint16_t)(rtl_io + 0x37)) & 0x10U) == 0) break;
    if (spin == 100000) return 0;
    for (uint8_t i = 0; i < 6; ++i) rtl_mac[i] = inb((uint16_t)(rtl_io + i));
    for (size_t i = 0; i < sizeof(rx_buffer); ++i) rx_buffer[i] = 0;
    rx_offset = 0;
    outl((uint16_t)(rtl_io + 0x30), (uint32_t)(uintptr_t)rx_buffer);
    outw((uint16_t)(rtl_io + 0x3c), 0x0015);
    outl((uint16_t)(rtl_io + 0x44), 0x0000e70a);
    outb((uint16_t)(rtl_io + 0x37), 0x0c);
    tx_index = 0; rtl_ready = 1; x86_64_pic_unmask(rtl_irq);
    uint8_t probe[60] = {0};
    for (uint8_t i = 0; i < 6; ++i) { probe[i] = 0xff; probe[6 + i] = rtl_mac[i]; }
    probe[12] = 0x88; probe[13] = 0xb5;
    probe[14] = 'S'; probe[15] = 'P'; probe[16] = 'L'; probe[17] = '6';
    if (x86_64_rtl8139_transmit(probe, sizeof(probe)) < 0) { rtl_ready = 0; return 0; }
    return 1;
}

int x86_64_rtl8139_transmit(const void *frame, size_t length)
{
    if (!rtl_ready || frame == NULL || length < 14 || length > TX_CAPACITY) return -1;
    uint8_t slot = tx_index; uint16_t status_port = (uint16_t)(rtl_io + 0x10 + slot * 4U);
    const uint8_t *source = frame;
    for (size_t i = 0; i < length; ++i) tx_buffer[slot][i] = source[i];
    size_t wire_length = length < 60 ? 60 : length;
    for (size_t i = length; i < wire_length; ++i) tx_buffer[slot][i] = 0;
    outl((uint16_t)(rtl_io + 0x20 + slot * 4U), (uint32_t)(uintptr_t)tx_buffer[slot]);
    outl(status_port, (uint32_t)wire_length);
    for (uint32_t spin = 0; spin < 1000000; ++spin) {
        uint32_t status;
        __asm__ volatile ("inl %1, %0" : "=a"(status) : "Nd"(status_port));
        if ((status & (1U << 15)) != 0) {
            tx_index = (uint8_t)((slot + 1U) % TX_SLOTS); return (int)length;
        }
        if ((status & ((1U << 30) | (1U << 14))) != 0) return -1;
    }
    return -1;
}

void x86_64_rtl8139_irq(uint8_t irq)
{
    if (!rtl_ready || irq != rtl_irq) return;
    uint16_t status = inw((uint16_t)(rtl_io + 0x3e));
    for (uint32_t budget = 0; budget < 64 &&
         (inb((uint16_t)(rtl_io + 0x37)) & 1U) == 0; ++budget) {
        uint8_t *packet = rx_buffer + rx_offset;
        uint16_t receive_status = *(volatile uint16_t *)packet;
        uint16_t length = *(volatile uint16_t *)(packet + 2);
        if ((receive_status & 1U) == 0 || length < 4 || length > 1536) {
            rx_offset = 0; outl((uint16_t)(rtl_io + 0x30), (uint32_t)(uintptr_t)rx_buffer); break;
        }
        (void)x86_64_network_accept_frame(packet + 4, (size_t)length - 4U);
        rx_offset = (uint16_t)((rx_offset + length + 7U) & ~3U);
        rx_offset %= RX_RING;
        outw((uint16_t)(rtl_io + 0x38), (uint16_t)(rx_offset - 16U));
    }
    if (status != 0) outw((uint16_t)(rtl_io + 0x3e), status);
}

static int dhcp_send(uint8_t message_type, const uint8_t requested[4],
                     const uint8_t server[4])
{
    enum { TRANSACTION = 0x31535053 };
    uint8_t frame[sizeof(struct ethernet) + sizeof(struct ipv4) +
                  sizeof(struct udp) + sizeof(struct bootp) + 64] = {0};
    struct ethernet *eth = (struct ethernet *)frame;
    struct ipv4 *ip = (struct ipv4 *)(eth + 1);
    struct udp *udp = (struct udp *)(ip + 1);
    struct bootp *bootp = (struct bootp *)(udp + 1);
    uint8_t *options = (uint8_t *)(bootp + 1); size_t option = 0;
    for (uint8_t i = 0; i < 6; ++i) { eth->destination[i] = 0xff; eth->source[i] = rtl_mac[i]; }
    eth->type = swap16(ETH_IPV4);
    bootp->operation = 1; bootp->hardware_type = 1; bootp->hardware_length = 6;
    bootp->transaction = TRANSACTION; bootp->flags = swap16(0x8000);
    for (uint8_t i = 0; i < 6; ++i) bootp->client_hardware[i] = rtl_mac[i];
    bootp->cookie = UINT32_C(0x63538263);
    options[option++] = 53; options[option++] = 1; options[option++] = message_type;
    options[option++] = 61; options[option++] = 7; options[option++] = 1;
    for (uint8_t i = 0; i < 6; ++i) options[option++] = rtl_mac[i];
    if (requested != NULL) {
        options[option++] = 50; options[option++] = 4;
        for (uint8_t i = 0; i < 4; ++i) options[option++] = requested[i];
    }
    if (server != NULL) {
        options[option++] = 54; options[option++] = 4;
        for (uint8_t i = 0; i < 4; ++i) options[option++] = server[i];
    }
    options[option++] = 55; options[option++] = 3;
    options[option++] = 1; options[option++] = 3; options[option++] = 6;
    options[option++] = 255;
    size_t payload_length = sizeof(*bootp) + option;
    udp->source = swap16(68); udp->destination = swap16(67);
    udp->length = swap16((uint16_t)(sizeof(*udp) + payload_length)); udp->checksum = 0;
    ip->version_ihl = 0x45; ip->total = swap16((uint16_t)(sizeof(*ip) + sizeof(*udp) + payload_length));
    ip->fragment = swap16(0x4000); ip->ttl = 64; ip->protocol = IP_UDP;
    for (uint8_t i = 0; i < 4; ++i) ip->destination[i] = 0xff;
    ip->checksum = ipv4_checksum(ip, sizeof(*ip));
    return x86_64_rtl8139_transmit(frame, sizeof(*eth) + sizeof(*ip) +
                                   sizeof(*udp) + payload_length) >= 0;
}

static uint8_t dhcp_option(const uint8_t *payload, size_t length, uint8_t wanted,
                           uint8_t output[4])
{
    size_t position = sizeof(struct bootp);
    while (position < length) {
        uint8_t type = payload[position++];
        if (type == 255) break;
        if (type == 0) continue;
        if (position >= length) return 0;
        uint8_t size = payload[position++];
        if (size > length - position) return 0;
        if (type == wanted) {
            if (output != NULL && size >= 4)
                for (uint8_t i = 0; i < 4; ++i) output[i] = payload[position + i];
            return size == 1 ? payload[position] : size;
        }
        position += size;
    }
    return 0;
}

int x86_64_dhcp_configure(void)
{
    enum { TRANSACTION = 0x31535053 };
    if (!rtl_ready) return 0;
    int socket = x86_64_udp_open(68); if (socket < 0 || !dhcp_send(1, NULL, NULL)) return 0;
    uint8_t payload[PAYLOAD_MAX], offered[4] = {0}, server[4] = {0}; int state = 1;
    uint64_t deadline = x86_64_timer_ticks() + 300;
    while (x86_64_timer_ticks() < deadline) {
        int length = x86_64_udp_receive(socket, payload, sizeof(payload), NULL);
        if (length < (int)sizeof(struct bootp)) { __asm__ volatile ("hlt"); continue; }
        const struct bootp *reply = (const struct bootp *)payload;
        if (reply->operation != 2 || reply->hardware_type != 1 ||
            reply->hardware_length != 6 || reply->transaction != TRANSACTION ||
            reply->cookie != UINT32_C(0x63538263)) continue;
        uint8_t type = dhcp_option(payload, (size_t)length, 53, NULL);
        if (state == 1 && type == 2 && dhcp_option(payload, (size_t)length, 54, server) >= 4) {
            for (uint8_t i = 0; i < 4; ++i) offered[i] = reply->offered_ip[i];
            if (!dhcp_send(3, offered, server)) break;
            state = 2;
        } else if (state == 2 && type == 5) {
            for (uint8_t i = 0; i < 4; ++i) configured_ip[i] = reply->offered_ip[i];
            (void)x86_64_udp_close(socket);
            return configured_ip[0] != 0;
        }
    }
    (void)x86_64_udp_close(socket); return 0;
}
