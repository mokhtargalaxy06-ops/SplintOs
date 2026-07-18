#include <splint/syscall.h>
#include <splint/net.h>

int main(int argument_count, char **arguments)
{
    (void)argument_count;
    (void)arguments;
    static const char message[] = "dup2: shared descriptor online\r\n";
    struct splint_poll_entry invalid_poll = {0, 0x80000000U, 0};
    if (sys_write(1, (const void *)0xC0000000U, 1) != -1 ||
        sys_stat("/README", (struct splint_directory_entry *)0xC0000000U) != -1 ||
        sys_poll(&invalid_poll, 1, 0) != -1 || sys_sleep(0x80000000U) != -1 ||
        sys_brk((void *)0x7FFFFFFFU) != (void *)-1 || sys_fsync(-1) != -1) return 1;
    if (sys_dup2(1, 5) != 5) return 1;
    unsigned int inherited_group = sys_getpgrp();
    unsigned int process = (unsigned int)sys_getpid();
    if (inherited_group == 0 || sys_setpgid(0, 0) != 0 ||
        sys_getpgrp() != process || sys_setpgid(0, inherited_group) != 0 ||
        sys_getpgrp() != inherited_group || sys_setpgid(0, 0xFFFFFFFFU) != -1)
        return 1;
    if (sys_write(5, message, sizeof(message) - 1) < 0) return 2;
    if (sys_close(5) != 0) return 3;
    int file = sys_open("/tmp/seek-test", SPLINT_READ | SPLINT_WRITE |
                        SPLINT_CREATE | SPLINT_TRUNCATE);
    static const char data[] = "abcdef";
    char result[2];
    struct splint_directory_entry metadata;
    if (file < 0 || sys_write(file, data, 6) != 6 || sys_dup2(file, 6) != 6 ||
        sys_seek(file, 2) != 0 || sys_read(6, result, 2) != 2 ||
        result[0] != 'c' || result[1] != 'd' ||
        sys_truncate(file, (size_t)-1) != -1 ||
        sys_stat("/tmp/seek-test", &metadata) != 0 || metadata.size != 6 ||
        sys_truncate(file, 3) != 0 ||
        sys_close(6) != 0 ||
        sys_close(file) != 0 || sys_chmod("/tmp/seek-test", 0600) != 0 ||
        sys_stat("/tmp/seek-test", &metadata) != 0 ||
        metadata.type != 0 || metadata.size != 3 || metadata.mode != 0600 ||
        sys_unlink("/tmp/seek-test") != 0) return 4;
    if (sys_mkdir("/tmp/fdtest-dir") != 0 ||
        sys_stat("/tmp/fdtest-dir", &metadata) != 0 || metadata.type != 1 ||
        sys_rmdir("/tmp/fdtest-dir") != 0 ||
        sys_stat("/tmp/fdtest-dir", &metadata) == 0) return 5;
    int socket = sys_udp_open(40000);
    struct splint_network_config network;
    struct splint_poll_entry socket_poll = {socket, SPLINT_POLL_READ | SPLINT_POLL_WRITE, 0};
    struct splint_udp_endpoint broadcast = {{255,255,255,255}, 40001, 0};
    if (socket < 0 || sys_network_config(&network) != 0 ||
        network.address[0] == 0 || network.subnet[0] == 0 ||
        network.gateway[0] == 0 || network.dns[0] == 0) return 6;
    if (sys_poll(&socket_poll, 1, 0) != 1 ||
        socket_poll.returned_events != SPLINT_POLL_WRITE) return 7;
    if (sys_udp_send(socket, &broadcast, "udp", 3) != 3) return 8;
    int receiver = sys_udp_open(40001);
    int alternate = sys_udp_open(40002);
    struct splint_udp_endpoint loopback = {{127,0,0,1}, 40001, 0}, source;
    char udp_data[4];
    struct splint_poll_entry socket_polls[2] = {
        {receiver, SPLINT_POLL_READ, 0}, {alternate, SPLINT_POLL_READ, 0}
    };
    if (receiver < 0 || alternate < 0 || sys_udp_open(40001) != -1 ||
        sys_udp_send(socket, &loopback, "one", 3) != 3 ||
        sys_udp_send(socket, &loopback, "two", 3) != 3 ||
        sys_udp_send(socket, &loopback, "tri", 3) != 3 ||
        sys_udp_send(socket, &loopback, "for", 3) != 3 ||
        sys_udp_send(socket, &loopback, "fiv", 3) != 3 ||
        sys_poll(socket_polls, 2, 10) != 1 ||
        socket_polls[0].returned_events != SPLINT_POLL_READ ||
        socket_polls[1].returned_events != 0 ||
        sys_udp_receive(receiver, &source, udp_data, sizeof(udp_data)) != 3 ||
        source.address[0] != 127 || source.port != 40000 ||
        udp_data[0] != 'o' || udp_data[2] != 'e' ||
        sys_udp_receive(receiver, &source, udp_data, sizeof(udp_data)) != 3 ||
        udp_data[0] != 't' || udp_data[2] != 'o' ||
        sys_udp_receive(receiver, &source, udp_data, sizeof(udp_data)) != 3 ||
        udp_data[0] != 't' || udp_data[2] != 'i' ||
        sys_udp_receive(receiver, &source, udp_data, sizeof(udp_data)) != 3 ||
        udp_data[0] != 'f' || udp_data[2] != 'r')
        return 9;
    socket_poll = (struct splint_poll_entry){receiver, SPLINT_POLL_READ, 0};
    if (sys_poll(&socket_poll, 1, 0) != 0 || socket_poll.returned_events != 0)
        return 9;
    int ephemeral = sys_udp_open(0);
    if (ephemeral < 0 || sys_udp_send(ephemeral, &loopback, "eph", 3) != 3 ||
        sys_udp_receive(receiver, &source, udp_data, sizeof(udp_data)) != 3 ||
        source.port < 49152 || udp_data[0] != 'e' || udp_data[2] != 'h' ||
        sys_close(ephemeral) != 0 || sys_close(alternate) != 0 ||
        sys_close(receiver) != 0) return 9;
    if (sys_close(socket) != 0) return 9;
    static const unsigned char dns_response[] = {
        0x53,0x01, 0x81,0x80, 0,0, 0,1, 0,0, 0,0,
        0xC0,0x0C, 0,1, 0,1, 0,0,0,30, 0,4, 1,2,3,4
    };
    unsigned char resolved[4];
    if (splint_dns_parse_ipv4(dns_response, sizeof(dns_response), 0x5301,
                              resolved) != 0 ||
        resolved[0] != 1 || resolved[3] != 4) return 10;
    static const unsigned char bad_pointer[] = {
        0x53,0x01, 0x81,0x80, 0,0, 0,1, 0,0, 0,0,
        0xFF,0xFF, 0,1, 0,1, 0,0,0,30, 0,4, 1,2,3,4
    };
    if (splint_dns_parse_ipv4(dns_response, sizeof(dns_response) - 1,
                              0x5301, resolved) == 0 ||
        splint_dns_parse_ipv4(dns_response, sizeof(dns_response),
                              0x5302, resolved) == 0 ||
        splint_dns_parse_ipv4(bad_pointer, sizeof(bad_pointer),
                              0x5301, resolved) == 0) return 10;
    struct splint_wall_clock wall;
    if (sys_wall_clock(&wall) != 0 || wall.year < 2020 || wall.year > 2199 ||
        wall.month < 1 || wall.month > 12 || wall.day < 1 || wall.day > 31 ||
        wall.hour > 23 || wall.minute > 59 || wall.second > 59) return 11;
    return 0;
}
