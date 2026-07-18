#ifndef SPLINT_USER_NET_H
#define SPLINT_USER_NET_H

int splint_resolve_ipv4(const char *name, unsigned char address[4]);
int splint_dns_parse_ipv4(const unsigned char *response, unsigned int length,
                          unsigned int identifier, unsigned char address[4]);

#endif
