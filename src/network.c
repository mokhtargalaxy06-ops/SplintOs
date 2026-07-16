#include "network.h"

#include "kernel.h"
#include "hardware.h"

#include <stddef.h>
#include <stdint.h>

#define PACKED __attribute__((packed))

enum {
    RTL_VENDOR = 0x10EC,
    RTL_DEVICE = 0x8139,
    RX_BUFFER_SIZE = 8192 + 16 + 1500,
    ETH_ARP = 0x0806,
    ETH_IPV4 = 0x0800,
    IP_UDP = 17,
    UDP_SOCKET_COUNT = 8,
    UDP_PAYLOAD_MAX = 512,
    UDP_QUEUE_DEPTH = 4,
    ARP_CACHE_SIZE = 8,
};

struct PACKED ethernet_header {
    uint8_t destination[6];
    uint8_t source[6];
    uint16_t type;
};

struct PACKED arp_packet {
    uint16_t hardware_type;
    uint16_t protocol_type;
    uint8_t hardware_size;
    uint8_t protocol_size;
    uint16_t operation;
    uint8_t sender_mac[6];
    uint8_t sender_ip[4];
    uint8_t target_mac[6];
    uint8_t target_ip[4];
};

struct PACKED ipv4_header {
    uint8_t version_ihl;
    uint8_t dscp_ecn;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint8_t source[4];
    uint8_t destination[4];
};

struct PACKED icmp_header {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
};

struct PACKED udp_header {
    uint16_t source_port;
    uint16_t destination_port;
    uint16_t length;
    uint16_t checksum;
};

struct PACKED dhcp_packet {
    uint8_t operation, hardware_type, hardware_length, hops;
    uint32_t transaction_id;
    uint16_t seconds, flags;
    uint8_t client_ip[4], offered_ip[4], server_ip[4], gateway_ip[4];
    uint8_t client_hardware[16];
    uint8_t server_name[64];
    uint8_t boot_file[128];
    uint32_t magic_cookie;
    uint8_t options[64];
};

struct arp_cache_entry { bool valid; uint8_t ip[4]; uint8_t mac[6]; };
struct udp_socket {
    bool used;
    uint16_t port;
    uint8_t read_index, write_index, queued;
    uint16_t source_port[UDP_QUEUE_DEPTH], length[UDP_QUEUE_DEPTH];
    uint8_t source_ip[UDP_QUEUE_DEPTH][4];
    uint8_t payload[UDP_QUEUE_DEPTH][UDP_PAYLOAD_MAX];
};

static uint16_t io_base;
static uint16_t rx_offset;
static uint8_t mac[6];
static uint8_t ip[4] = {10, 0, 2, 15};
static uint8_t subnet_mask[4] = {255, 255, 255, 0};
static uint8_t gateway_address[4] = {10, 0, 2, 2};
static uint8_t dns_address[4] = {10, 0, 2, 3};
static uint8_t rx_buffer[RX_BUFFER_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buffer[4][1536] __attribute__((aligned(16)));
static uint8_t tx_index;
static bool network_ready;
static bool dhcp_configured;
static uint8_t dhcp_state;
static uint8_t dhcp_server[4];
static struct arp_cache_entry arp_cache[ARP_CACHE_SIZE];
static struct udp_socket udp_sockets[UDP_SOCKET_COUNT];
static uint8_t arp_next;
static uint16_t next_ephemeral_port = 49152;
static const uint32_t dhcp_transaction = 0x534F5301U;

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t value)
{
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t value)
{
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void memory_copy(void *destination, const void *source, size_t length)
{
    uint8_t *to = destination;
    const uint8_t *from = source;
    while (length-- != 0) {
        *to++ = *from++;
    }
}

static bool udp_enqueue(struct udp_socket *socket, const void *payload, size_t length,
                        const uint8_t source_ip[4], uint16_t source_port)
{
    if (socket->queued == UDP_QUEUE_DEPTH || length > UDP_PAYLOAD_MAX) return false;
    uint8_t slot = socket->write_index;
    memory_copy(socket->payload[slot], payload, length);
    memory_copy(socket->source_ip[slot], source_ip, 4);
    socket->source_port[slot] = source_port;
    socket->length[slot] = (uint16_t)length;
    socket->write_index = (uint8_t)((slot + 1) % UDP_QUEUE_DEPTH);
    ++socket->queued;
    return true;
}

static bool memory_equal(const void *left, const void *right, size_t length)
{
    const uint8_t *a = left;
    const uint8_t *b = right;
    while (length-- != 0) {
        if (*a++ != *b++) {
            return false;
        }
    }
    return true;
}

static bool ipv4_usable(const uint8_t address[4])
{
    static const uint8_t zero[4] = {0,0,0,0};
    static const uint8_t broadcast[4] = {255,255,255,255};
    return !memory_equal(address, zero, 4) && !memory_equal(address, broadcast, 4);
}

static bool subnet_contiguous(const uint8_t mask[4])
{
    uint32_t value = (uint32_t)mask[0] << 24 | (uint32_t)mask[1] << 16 |
                     (uint32_t)mask[2] << 8 | mask[3];
    uint32_t inverse = ~value;
    return value != 0 && (inverse & (inverse + 1U)) == 0;
}

static uint16_t swap16(uint16_t value)
{
    return (uint16_t)((value << 8) | (value >> 8));
}

static uint16_t checksum(const void *data, size_t length)
{
    const uint8_t *bytes = data;
    uint32_t sum = 0;

    while (length > 1) {
        sum += (uint16_t)((uint16_t)bytes[0] << 8 | bytes[1]);
        bytes += 2;
        length -= 2;
    }
    if (length != 0) {
        sum += (uint16_t)bytes[0] << 8;
    }
    while ((sum >> 16) != 0) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return swap16((uint16_t)~sum);
}

static uint16_t udp_checksum(const struct ipv4_header *ipv4,
                             const struct udp_header *udp, uint16_t length)
{
    uint32_t sum = 0;
    const uint8_t *address = ipv4->source;
    for (size_t i = 0; i < 8; i += 2)
        sum += (uint16_t)((uint16_t)address[i] << 8 | address[i + 1]);
    sum += IP_UDP;
    sum += length;
    const uint8_t *bytes = (const uint8_t *)udp;
    size_t remaining = length;
    while (remaining > 1) {
        sum += (uint16_t)((uint16_t)bytes[0] << 8 | bytes[1]);
        bytes += 2; remaining -= 2;
    }
    if (remaining != 0) sum += (uint16_t)bytes[0] << 8;
    while ((sum >> 16) != 0) sum = (sum & 0xFFFFU) + (sum >> 16);
    return swap16((uint16_t)~sum);
}

static void transmit(const void *frame, uint16_t length)
{
    uint16_t wire_length = length < 60 ? 60 : length;
    uint8_t *buffer = tx_buffer[tx_index];
    memory_copy(buffer, frame, length);
    for (uint16_t i = length; i < wire_length; ++i) {
        buffer[i] = 0;
    }

    outl((uint16_t)(io_base + 0x20 + tx_index * 4), (uint32_t)(uintptr_t)buffer);
    outl((uint16_t)(io_base + 0x10 + tx_index * 4), wire_length);
    tx_index = (uint8_t)((tx_index + 1) & 3);
}

static void arp_remember(const uint8_t address[4], const uint8_t hardware[6])
{
    for (uint8_t i = 0; i < ARP_CACHE_SIZE; ++i) {
        if (arp_cache[i].valid && memory_equal(arp_cache[i].ip, address, 4)) {
            memory_copy(arp_cache[i].mac, hardware, 6);
            return;
        }
    }
    struct arp_cache_entry *entry = &arp_cache[arp_next++ % ARP_CACHE_SIZE];
    entry->valid = true;
    memory_copy(entry->ip, address, 4);
    memory_copy(entry->mac, hardware, 6);
}

static const uint8_t *arp_lookup(const uint8_t address[4])
{
    for (uint8_t i = 0; i < ARP_CACHE_SIZE; ++i)
        if (arp_cache[i].valid && memory_equal(arp_cache[i].ip, address, 4))
            return arp_cache[i].mac;
    return NULL;
}

static const uint8_t *route_next_hop(const uint8_t destination[4])
{
    for (size_t i = 0; i < 4; ++i)
        if ((destination[i] & subnet_mask[i]) != (ip[i] & subnet_mask[i]))
            return gateway_address;
    return destination;
}

static void arp_request(const uint8_t address[4])
{
    uint8_t frame[sizeof(struct ethernet_header) + sizeof(struct arp_packet)] = {0};
    struct ethernet_header *ethernet = (struct ethernet_header *)frame;
    struct arp_packet *arp = (struct arp_packet *)(ethernet + 1);
    for (uint8_t i = 0; i < 6; ++i) ethernet->destination[i] = 0xFF;
    memory_copy(ethernet->source, mac, 6);
    ethernet->type = swap16(ETH_ARP);
    arp->hardware_type = swap16(1);
    arp->protocol_type = swap16(ETH_IPV4);
    arp->hardware_size = 6;
    arp->protocol_size = 4;
    arp->operation = swap16(1);
    memory_copy(arp->sender_mac, mac, 6);
    memory_copy(arp->sender_ip, ip, 4);
    memory_copy(arp->target_ip, address, 4);
    transmit(frame, sizeof(frame));
}

static int udp_send_raw(uint16_t source_port, const uint8_t destination_ip[4],
                        uint16_t destination_port, const void *data, size_t length,
                        bool broadcast)
{
    if (length > UDP_PAYLOAD_MAX) return -1;
    uint8_t frame[sizeof(struct ethernet_header) + sizeof(struct ipv4_header) +
                  sizeof(struct udp_header) + UDP_PAYLOAD_MAX];
    struct ethernet_header *ethernet = (struct ethernet_header *)frame;
    struct ipv4_header *ipv4 = (struct ipv4_header *)(ethernet + 1);
    struct udp_header *udp = (struct udp_header *)(ipv4 + 1);
    const uint8_t *next_hop = broadcast ? NULL : route_next_hop(destination_ip);
    const uint8_t *destination_mac = broadcast ? NULL : arp_lookup(next_hop);
    if (!broadcast && destination_mac == NULL) {
        arp_request(next_hop);
        return -2;
    }
    if (broadcast) for (uint8_t i = 0; i < 6; ++i) ethernet->destination[i] = 0xFF;
    else memory_copy(ethernet->destination, destination_mac, 6);
    memory_copy(ethernet->source, mac, 6);
    ethernet->type = swap16(ETH_IPV4);
    ipv4->version_ihl = 0x45;
    ipv4->dscp_ecn = 0;
    ipv4->total_length = swap16((uint16_t)(sizeof(*ipv4) + sizeof(*udp) + length));
    ipv4->identification = 0;
    ipv4->flags_fragment = swap16(0x4000);
    ipv4->ttl = 64;
    ipv4->protocol = IP_UDP;
    ipv4->checksum = 0;
    memory_copy(ipv4->source, ip, 4);
    memory_copy(ipv4->destination, destination_ip, 4);
    ipv4->checksum = checksum(ipv4, sizeof(*ipv4));
    udp->source_port = swap16(source_port);
    udp->destination_port = swap16(destination_port);
    udp->length = swap16((uint16_t)(sizeof(*udp) + length));
    udp->checksum = 0;
    memory_copy(udp + 1, data, length);
    udp->checksum = udp_checksum(ipv4, udp, (uint16_t)(sizeof(*udp) + length));
    if (udp->checksum == 0) udp->checksum = 0xFFFFU;
    transmit(frame, (uint16_t)(sizeof(*ethernet) + sizeof(*ipv4) + sizeof(*udp) + length));
    return (int)length;
}

static void dhcp_send(uint8_t message_type, const uint8_t requested[4],
                      const uint8_t server[4])
{
    struct dhcp_packet packet = {0};
    packet.operation = 1;
    packet.hardware_type = 1;
    packet.hardware_length = 6;
    packet.transaction_id = dhcp_transaction;
    packet.flags = swap16(0x8000);
    memory_copy(packet.client_hardware, mac, 6);
    packet.magic_cookie = 0x63538263U;
    size_t option = 0;
    packet.options[option++] = 53; packet.options[option++] = 1;
    packet.options[option++] = message_type;
    packet.options[option++] = 61; packet.options[option++] = 7;
    packet.options[option++] = 1;
    memory_copy(packet.options + option, mac, 6); option += 6;
    if (requested != NULL) {
        packet.options[option++] = 50; packet.options[option++] = 4;
        memory_copy(packet.options + option, requested, 4); option += 4;
    }
    if (server != NULL) {
        packet.options[option++] = 54; packet.options[option++] = 4;
        memory_copy(packet.options + option, server, 4); option += 4;
    }
    packet.options[option++] = 55; packet.options[option++] = 3;
    packet.options[option++] = 1; packet.options[option++] = 3; packet.options[option++] = 6;
    packet.options[option++] = 255;
    const uint8_t broadcast[4] = {255,255,255,255};
    uint8_t old_ip[4];
    memory_copy(old_ip, ip, 4);
    for (uint8_t i = 0; i < 4; ++i) ip[i] = 0;
    (void)udp_send_raw(68, broadcast, 67, &packet,
                       offsetof(struct dhcp_packet, options) + option, true);
    memory_copy(ip, old_ip, 4);
}

static uint8_t dhcp_option(const struct dhcp_packet *packet, size_t length,
                           uint8_t wanted, uint8_t output[4])
{
    if (length <= offsetof(struct dhcp_packet, options)) return 0;
    size_t position = 0;
    size_t available = length - offsetof(struct dhcp_packet, options);
    while (position < available) {
        uint8_t type = packet->options[position++];
        if (type == 255) break;
        if (type == 0) continue;
        if (position >= available) break;
        uint8_t size = packet->options[position++];
        if (position + size > available) break;
        if (type == wanted) {
            if (output != NULL && size >= 4) memory_copy(output, packet->options + position, 4);
            return size == 1 ? packet->options[position] : size;
        }
        position += size;
    }
    return 0;
}

static void handle_dhcp(const uint8_t *payload, size_t length)
{
    if (length < offsetof(struct dhcp_packet, options)) return;
    const struct dhcp_packet *packet = (const struct dhcp_packet *)payload;
    if (packet->operation != 2 || packet->hardware_type != 1 ||
        packet->hardware_length != 6 ||
        packet->magic_cookie != 0x63538263U ||
        packet->transaction_id != dhcp_transaction ||
        !memory_equal(packet->client_hardware, mac, 6)) return;
    uint8_t server[4];
    uint8_t type = dhcp_option(packet, length, 53, NULL);
    if (type == 2 && dhcp_state == 1 && ipv4_usable(packet->offered_ip) &&
        dhcp_option(packet, length, 54, server) >= 4 && ipv4_usable(server)) {
        memory_copy(dhcp_server, server, 4);
        dhcp_state = 2;
        dhcp_send(3, packet->offered_ip, server);
    } else if (type == 5 && dhcp_state == 2 &&
               dhcp_option(packet, length, 54, server) >= 4 &&
               memory_equal(server, dhcp_server, 4) &&
               ipv4_usable(packet->offered_ip)) {
        uint8_t option[4];
        memory_copy(ip, packet->offered_ip, 4);
        if (dhcp_option(packet, length, 1, option) == 4 && subnet_contiguous(option))
            memory_copy(subnet_mask, option, 4);
        if (dhcp_option(packet, length, 3, option) >= 4 && ipv4_usable(option))
            memory_copy(gateway_address, option, 4);
        if (dhcp_option(packet, length, 6, option) >= 4 && ipv4_usable(option))
            memory_copy(dns_address, option, 4);
        dhcp_configured = true;
        dhcp_state = 3;
    }
}

static void handle_arp(uint8_t *frame, uint16_t length)
{
    if (length < sizeof(struct ethernet_header) + sizeof(struct arp_packet)) {
        return;
    }
    struct ethernet_header *ethernet = (struct ethernet_header *)frame;
    struct arp_packet *arp = (struct arp_packet *)(ethernet + 1);
    uint16_t operation = swap16(arp->operation);
    if (swap16(arp->hardware_type) != 1 || swap16(arp->protocol_type) != ETH_IPV4 ||
        arp->hardware_size != 6 || arp->protocol_size != 4 ||
        (operation != 1 && operation != 2) ||
        !memory_equal(ethernet->source, arp->sender_mac, 6) ||
        !memory_equal(arp->target_ip, ip, 4)) return;
    static const uint8_t zero_ip[4] = {0,0,0,0};
    if (!memory_equal(arp->sender_ip, zero_ip, 4))
        arp_remember(arp->sender_ip, arp->sender_mac);
    if (operation != 1) return;

    uint8_t requester_mac[6];
    uint8_t requester_ip[4];
    memory_copy(requester_mac, arp->sender_mac, 6);
    memory_copy(requester_ip, arp->sender_ip, 4);
    memory_copy(ethernet->destination, requester_mac, 6);
    memory_copy(ethernet->source, mac, 6);
    arp->operation = swap16(2);
    memory_copy(arp->target_mac, requester_mac, 6);
    memory_copy(arp->target_ip, requester_ip, 4);
    memory_copy(arp->sender_mac, mac, 6);
    memory_copy(arp->sender_ip, ip, 4);
    transmit(frame, sizeof(*ethernet) + sizeof(*arp));
}

static void handle_udp(const struct ipv4_header *ipv4, uint16_t total_length,
                       uint8_t header_length)
{
    if (total_length < header_length + sizeof(struct udp_header)) return;
    const struct udp_header *udp = (const struct udp_header *)((const uint8_t *)ipv4 + header_length);
    uint16_t udp_length = swap16(udp->length);
    if (udp_length < sizeof(*udp) || header_length + udp_length > total_length) return;
    if (udp->checksum != 0 && udp_checksum(ipv4, udp, udp_length) != 0) return;
    uint16_t destination = swap16(udp->destination_port);
    const uint8_t *payload = (const uint8_t *)(udp + 1);
    size_t payload_length = udp_length - sizeof(*udp);
    if (destination == 68 && swap16(udp->source_port) == 67) {
        handle_dhcp(payload, payload_length);
        return;
    }
    for (uint8_t i = 0; i < UDP_SOCKET_COUNT; ++i) {
        if (!udp_sockets[i].used || udp_sockets[i].port != destination) continue;
        if (payload_length > UDP_PAYLOAD_MAX) payload_length = UDP_PAYLOAD_MAX;
        (void)udp_enqueue(&udp_sockets[i], payload, payload_length, ipv4->source,
                          swap16(udp->source_port));
        return;
    }
}

static void handle_ipv4(uint8_t *frame, uint16_t length)
{
    if (length < sizeof(struct ethernet_header) + sizeof(struct ipv4_header)) {
        return;
    }
    struct ethernet_header *ethernet = (struct ethernet_header *)frame;
    struct ipv4_header *ipv4 = (struct ipv4_header *)(ethernet + 1);
    uint8_t header_length = (uint8_t)((ipv4->version_ihl & 0x0F) * 4);
    uint16_t total_length = swap16(ipv4->total_length);
    uint16_t fragment = swap16(ipv4->flags_fragment);
    static const uint8_t broadcast[4] = {255,255,255,255};
    if ((ipv4->version_ihl >> 4) != 4 || header_length < 20 ||
        total_length < header_length || sizeof(*ethernet) + total_length > length ||
        checksum(ipv4, header_length) != 0 || (fragment & 0x3FFFU) != 0 ||
        (!memory_equal(ipv4->destination, ip, 4) &&
         !memory_equal(ipv4->destination, broadcast, 4))) {
        return;
    }
    arp_remember(ipv4->source, ethernet->source);
    if (ipv4->protocol == IP_UDP) {
        handle_udp(ipv4, total_length, header_length);
        return;
    }
    if (ipv4->protocol != 1 || total_length < header_length + sizeof(struct icmp_header)) return;
    struct icmp_header *icmp = (struct icmp_header *)((uint8_t *)ipv4 + header_length);
    if (!memory_equal(ipv4->destination, ip, 4) ||
        checksum(icmp, total_length - header_length) != 0 ||
        icmp->type != 8 || icmp->code != 0) {
        return;
    }

    uint8_t old_mac[6];
    uint8_t old_ip[4];
    memory_copy(old_mac, ethernet->source, 6);
    memory_copy(old_ip, ipv4->source, 4);
    memory_copy(ethernet->destination, old_mac, 6);
    memory_copy(ethernet->source, mac, 6);
    memory_copy(ipv4->destination, old_ip, 4);
    memory_copy(ipv4->source, ip, 4);
    ipv4->ttl = 64;
    ipv4->checksum = 0;
    ipv4->checksum = checksum(ipv4, header_length);
    icmp->type = 0;
    icmp->checksum = 0;
    icmp->checksum = checksum(icmp, total_length - header_length);
    transmit(frame, (uint16_t)(sizeof(*ethernet) + total_length));
}

static void handle_frame(uint8_t *frame, uint16_t length)
{
    if (length < sizeof(struct ethernet_header)) {
        return;
    }
    struct ethernet_header *ethernet = (struct ethernet_header *)frame;
    uint16_t type = swap16(ethernet->type);
    if (type == ETH_ARP) {
        handle_arp(frame, length);
    } else if (type == ETH_IPV4) {
        handle_ipv4(frame, length);
    }
}

bool network_init(void)
{
    const struct pci_device *device = pci_find_device(RTL_VENDOR, RTL_DEVICE);
    if (device == NULL) return false;

    uint32_t bar0 = device->bars[0];
    if ((bar0 & 1U) == 0) {
        terminal_write("RTL8139 memory BAR is not supported.\n");
        return false;
    }
    io_base = (uint16_t)(bar0 & ~3U);

    pci_enable_device(device, true, false, true);
    outb((uint16_t)(io_base + 0x52), 0);
    outb((uint16_t)(io_base + 0x37), 0x10);
    while ((inb((uint16_t)(io_base + 0x37)) & 0x10) != 0) {
    }

    for (uint8_t i = 0; i < 6; ++i) {
        mac[i] = inb((uint16_t)(io_base + i));
    }
    outl((uint16_t)(io_base + 0x30), (uint32_t)(uintptr_t)rx_buffer);
    /* Receive-OK and transmit-OK interrupts; the IDT/PIC layer dispatches them. */
    outw((uint16_t)(io_base + 0x3C), 0x0005);
    outl((uint16_t)(io_base + 0x44), 0x0000E70A);
    outb((uint16_t)(io_base + 0x37), 0x0C);
    network_ready = true;
    dhcp_state = 1;
    dhcp_send(1, NULL, NULL);
    return true;
}

void network_poll(void)
{
    while ((inb((uint16_t)(io_base + 0x37)) & 1U) == 0) {
        uint8_t *packet = rx_buffer + rx_offset;
        uint16_t status = *(volatile uint16_t *)packet;
        uint16_t length = *(volatile uint16_t *)(packet + 2);
        if ((status & 1U) == 0 || length < 4 || length > 1536) {
            outb((uint16_t)(io_base + 0x37), 0x04);
            rx_offset = 0;
            outl((uint16_t)(io_base + 0x30), (uint32_t)(uintptr_t)rx_buffer);
            return;
        }

        handle_frame(packet + 4, (uint16_t)(length - 4));
        rx_offset = (uint16_t)((rx_offset + length + 4 + 3) & ~3U);
        rx_offset %= 8192;
        outw((uint16_t)(io_base + 0x38), (uint16_t)(rx_offset - 16));
    }
}

void network_interrupt(void)
{
    if (!network_ready) return;
    uint16_t status = inw((uint16_t)(io_base + 0x3E));
    if (status != 0) outw((uint16_t)(io_base + 0x3E), status);
    network_poll();
}

int udp_open(uint16_t local_port)
{
    if (local_port == 0) {
        for (uint32_t attempt = 0; attempt < 16384; ++attempt) {
            uint16_t candidate = next_ephemeral_port++;
            if (next_ephemeral_port == 0) next_ephemeral_port = 49152;
            bool available = true;
            for (uint8_t i = 0; i < UDP_SOCKET_COUNT; ++i)
                if (udp_sockets[i].used && udp_sockets[i].port == candidate)
                    available = false;
            if (available) { local_port = candidate; break; }
        }
        if (local_port == 0) return -1;
    }
    for (uint8_t i = 0; i < UDP_SOCKET_COUNT; ++i)
        if (udp_sockets[i].used && udp_sockets[i].port == local_port) return -1;
    for (uint8_t i = 0; i < UDP_SOCKET_COUNT; ++i) {
        if (udp_sockets[i].used) continue;
        udp_sockets[i] = (struct udp_socket){0};
        udp_sockets[i].used = true;
        udp_sockets[i].port = local_port;
        return i;
    }
    return -1;
}

int udp_close(int socket)
{
    if (socket < 0 || socket >= UDP_SOCKET_COUNT || !udp_sockets[socket].used) return -1;
    udp_sockets[socket].used = false;
    return 0;
}

int udp_receive(int socket, void *data, size_t capacity, uint8_t source_ip[4],
                uint16_t *source_port)
{
    if (socket < 0 || socket >= UDP_SOCKET_COUNT || !udp_sockets[socket].used ||
        udp_sockets[socket].queued == 0 || data == NULL) return -1;
    struct udp_socket *entry = &udp_sockets[socket];
    uint8_t slot = entry->read_index;
    size_t length = entry->length[slot] < capacity ? entry->length[slot] : capacity;
    memory_copy(data, entry->payload[slot], length);
    if (source_ip != NULL) memory_copy(source_ip, entry->source_ip[slot], 4);
    if (source_port != NULL) *source_port = entry->source_port[slot];
    entry->read_index = (uint8_t)((slot + 1) % UDP_QUEUE_DEPTH);
    --entry->queued;
    return (int)length;
}

int udp_send(int socket, const uint8_t destination_ip[4], uint16_t destination_port,
             const void *data, size_t length)
{
    if (socket < 0 || socket >= UDP_SOCKET_COUNT || !udp_sockets[socket].used ||
        destination_ip == NULL || data == NULL) return -1;
    static const uint8_t loopback[4] = {127,0,0,1};
    if (memory_equal(destination_ip, loopback, 4) || memory_equal(destination_ip, ip, 4)) {
        if (length > UDP_PAYLOAD_MAX) return -1;
        for (uint8_t i = 0; i < UDP_SOCKET_COUNT; ++i) {
            if (!udp_sockets[i].used || udp_sockets[i].port != destination_port) continue;
            const uint8_t *source = memory_equal(destination_ip, loopback, 4)
                ? loopback : ip;
            (void)udp_enqueue(&udp_sockets[i], data, length, source,
                              udp_sockets[socket].port);
            /* UDP delivery is best-effort: a full receive queue drops the
             * datagram after a successful send, just like hardware receive. */
            return (int)length;
        }
        return -1;
    }
    static const uint8_t broadcast[4] = {255,255,255,255};
    return udp_send_raw(udp_sockets[socket].port, destination_ip, destination_port,
                        data, length, memory_equal(destination_ip, broadcast, 4));
}

void network_address(uint8_t address[4])
{
    if (address != NULL) memory_copy(address, ip, 4);
}

bool network_dhcp_configured(void) { return dhcp_configured; }
bool udp_pending(int socket)
{
    return socket >= 0 && socket < UDP_SOCKET_COUNT && udp_sockets[socket].used &&
           udp_sockets[socket].queued != 0;
}

void network_configuration(uint8_t address[4], uint8_t subnet[4],
                           uint8_t gateway[4], uint8_t dns[4])
{
    if (address != NULL) memory_copy(address, ip, 4);
    if (subnet != NULL) memory_copy(subnet, subnet_mask, 4);
    if (gateway != NULL) memory_copy(gateway, gateway_address, 4);
    if (dns != NULL) memory_copy(dns, dns_address, 4);
}
