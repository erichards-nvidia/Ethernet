#ifndef linklocal_h_
#define linklocal_h_

// Implementation of link-local-address aka autoIP (RFC3927)
// Send an ARP PROBE for the autoip address
// https://tools.ietf.org/html/rfc3927#section-2.2

#define MAX_CONFLICTS 10
#define PROBE_NUM 5
#define PROBE_WAIT 1
#define PROBE_MIN 1
#define PROBE_MAX 2
#define RATE_LIMIT_INTERVAL 5
#define ANNOUNCE_WAIT 2
#define ANNOUNCE_NUM 1
#define ANNOUNCE_INTERVAL 1
   
typedef struct arp_eth_ip_struct {
	uint16_t hw_type;				// Only supporting Ethernet (1) at this time
	uint16_t protocol_type;			// Only supporting IPv4 (0x800) at this time
	uint8_t  hw_len;				// Assumes MAC is 6 bytes
	uint8_t  protocol_len;			// Only supporting IPv4 address being 4 bytes
	uint16_t opcode;
	uint8_t  source_hw[6];			// Assumes Ethernet (MAC)
	uint8_t  source_protocol[4];	// Assumes IPv4
	uint8_t  target_hw[6];			// Assumes Ethernet (MAC)
	uint8_t  target_protocol[4];	// Assumes IPv4
} Arp_eth_ip_packet;

typedef struct eth_struct {
	uint8_t  eth_dest[6];
	uint8_t  eth_src[6];
	uint16_t eth_type;
	union {
		Arp_eth_ip_packet arp;
		// TODO Add other ARP packet types like IPv6 on Ethernet
	};
	uint8_t padding[10];
	uint32_t crc;  
} Ethernet_packet;

#endif
