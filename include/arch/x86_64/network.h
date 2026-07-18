#ifndef SPLINTOS_ARCH_X86_64_NETWORK_H
#define SPLINTOS_ARCH_X86_64_NETWORK_H
#include <stddef.h>
#include <stdint.h>
void x86_64_network_init(void);
int x86_64_udp_open(uint16_t port);
int x86_64_udp_close(int socket);
int x86_64_udp_send_loopback(int socket, uint16_t destination_port,
                             const void *data, size_t length);
int x86_64_udp_receive(int socket, void *data, size_t capacity,
                       uint16_t *source_port);
int x86_64_network_accept_frame(const void *frame, size_t length);
int x86_64_network_conformance_test(void);
int x86_64_rtl8139_init(void);
void x86_64_rtl8139_irq(uint8_t irq);
int x86_64_rtl8139_transmit(const void *frame, size_t length);
int x86_64_dhcp_configure(void);
#endif
