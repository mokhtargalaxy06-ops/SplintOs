#include <splint/net.h>
#include <splint/syscall.h>

#include <stddef.h>
#include <stdint.h>

static uint16_t read16(const uint8_t *data)
{ return (uint16_t)((uint16_t)data[0] << 8 | data[1]); }

static void write16(uint8_t *data, uint16_t value)
{ data[0] = (uint8_t)(value >> 8); data[1] = (uint8_t)value; }

static int skip_name(const uint8_t *packet, size_t length, size_t *position)
{
    size_t labels = 0;
    while (*position < length && labels++ < 128) {
        uint8_t size = packet[*position];
        if ((size & 0xC0U) == 0xC0U) {
            if (*position + 2 > length) return -1;
            size_t target = (size_t)(size & 0x3FU) << 8 | packet[*position + 1];
            if (target >= length) return -1;
            *position += 2; return 0;
        }
        ++*position;
        if (size == 0) return 0;
        if (size > 63 || size > length - *position) return -1;
        *position += size;
    }
    return -1;
}

int splint_resolve_ipv4(const char *name, unsigned char address[4])
{
    if (name == NULL || address == NULL) return -1;
    uint8_t query[512] = {0};
    uint16_t identifier = (uint16_t)(0x5300U | ((unsigned int)sys_getpid() & 0xFFU));
    write16(query, identifier); write16(query + 2, 0x0100); write16(query + 4, 1);
    size_t position = 12, start = 0;
    for (size_t i = 0;; ++i) {
        if (name[i] != '.' && name[i] != '\0') continue;
        size_t label = i - start;
        if (label == 0 || label > 63 || position + label + 1 >= sizeof(query)) return -1;
        query[position++] = (uint8_t)label;
        for (size_t j = 0; j < label; ++j) query[position++] = (uint8_t)name[start + j];
        if (name[i] == '\0') break;
        start = i + 1;
    }
    query[position++] = 0; write16(query + position, 1); position += 2;
    write16(query + position, 1); position += 2;

    struct splint_network_config configuration;
    if (sys_network_config(&configuration) != 0) return -1;
    struct splint_udp_endpoint server = {{0,0,0,0}, 53, 0}, source;
    for (size_t i = 0; i < 4; ++i) server.address[i] = configuration.dns[i];
    int socket = sys_udp_open(0);
    if (socket < 0) return -1;
    int sent = -1;
    for (size_t attempt = 0; attempt < 4 && sent < 0; ++attempt) {
        sent = sys_udp_send(socket, &server, query, position);
        if (sent < 0) (void)sys_sleep(10);
    }
    struct splint_poll_entry poll = {socket, SPLINT_POLL_READ, 0};
    if (sent != (int)position || sys_poll(&poll, 1, 200) != 1) {
        (void)sys_close(socket); return -1;
    }
    uint8_t response[512];
    int received = sys_udp_receive(socket, &source, response, sizeof(response));
    (void)sys_close(socket);
    if (received < 0 || source.port != 53) return -1;
    for (size_t i = 0; i < 4; ++i)
        if (source.address[i] != server.address[i]) return -1;
    return splint_dns_parse_ipv4(response, (unsigned int)received, identifier, address);
}

int splint_dns_parse_ipv4(const unsigned char *response, unsigned int length,
                          unsigned int identifier, unsigned char address[4])
{
    if (response == NULL || address == NULL || length < 12 ||
        read16(response) != (uint16_t)identifier ||
        (read16(response + 2) & 0x800FU) != 0x8000U) return -1;
    uint16_t questions = read16(response + 4), answers = read16(response + 6);
    size_t offset = 12;
    for (uint16_t i = 0; i < questions; ++i) {
        if (skip_name(response, length, &offset) != 0 || offset + 4 > length) return -1;
        offset += 4;
    }
    for (uint16_t i = 0; i < answers; ++i) {
        if (skip_name(response, length, &offset) != 0 || offset + 10 > length) return -1;
        uint16_t type = read16(response + offset);
        uint16_t class_code = read16(response + offset + 2);
        uint16_t data_length = read16(response + offset + 8);
        offset += 10;
        if (data_length > length - offset) return -1;
        if (type == 1 && class_code == 1 && data_length == 4) {
            for (size_t j = 0; j < 4; ++j) address[j] = response[offset + j];
            return 0;
        }
        offset += data_length;
    }
    return -1;
}
