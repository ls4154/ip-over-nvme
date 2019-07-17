/* useful headers */
/* <linux/in.h> */
/* <linux/ip.h> */

#ifndef IP_H
#define IP_H

#include <stdint.h>

/* IP version */
#define IPV4 0x04

/* IP protocol numbers */
#define IP_ICMP 0x01
#define IP_TCP 0x06
#define IP_UDP 0x11

struct ip_hdr {
	uint8_t ihl : 4;
	uint8_t version : 4;
	uint8_t tos;
	uint16_t len;
	uint16_t id;
	uint16_t frag_offset;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t check;
	uint32_t saddr;
	uint32_t daddr;
	uint8_t data[];
} __attribute__((packed));

int ip_addr(const char *name, const char *addr, const char *netmask);

#endif
