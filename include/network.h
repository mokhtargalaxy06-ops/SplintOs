#ifndef SPLINTOS_NETWORK_H
#define SPLINTOS_NETWORK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool network_init(void);
void network_poll(void);
void network_interrupt(void);
int udp_open(uint16_t local_port);
int udp_close(int socket);
int udp_receive(int socket, void *data, size_t capacity, uint8_t source_ip[4],
                uint16_t *source_port);
int udp_send(int socket, const uint8_t destination_ip[4], uint16_t destination_port,
             const void *data, size_t length);
void network_address(uint8_t address[4]);
bool network_dhcp_configured(void);
bool udp_pending(int socket);
void network_configuration(uint8_t address[4], uint8_t subnet[4],
                           uint8_t gateway[4], uint8_t dns[4]);

#endif
