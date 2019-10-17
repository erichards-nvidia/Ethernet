// Link Local Library v0.1 - Oct 16, 2019
// Author: Edward Richards

#include <Arduino.h>
#include "Ethernet.h"
#include "EthernetRaw.h"
#include "Linklocal.h"
#include "utility/w5100.h"


int LinkLocalClass::beginWithLinkLocal(uint8_t *mac)
{
	memcpy(_llMacAddr, mac, 6);

	// Make up an AutoIP address that falls in the local LAN range only (169.254.167/24)
	_llLocalIp[0] = 169;
	_llLocalIp[1] = 254;
	_llLocalIp[2] = 167;
	_llLocalIp[3] = 0;
	_llSubnetMask[0] = 255;
	_llSubnetMask[1] = 255;
	_llSubnetMask[2] = 255;
	_llSubnetMask[3] = 0;
	_llLocalPort = 0;

	// Form an ARP_PROBE packet
	// ETHERNET HEADER
	memset(&_llep.eth_dest, 0, 6);  					// Destination Address (6 bytes) - 00:00:00:00:00:00
	memcpy(&_llep.eth_src , _llMacAddr, 6);				// Source Address (6 bytes) - local mac address 
	_llep.eth_type = htons(0x0806);						// Ethernet Packet Type (2 bytes) - IPv4 (0x0800), ARP (0x0806)
	// ARP PACKET
	{
		_llep.arp.hw_type = 1;							// HW Type (2 bytes) - Ethernet (1)
		_llep.arp.protocol_type = htons(0x0800); 		// Protocol type (2 bytes) - IPv4 (0x0800)
		_llep.arp.hw_len = 6;							// Hardware address length (1 byte)
		_llep.arp.protocol_len = 4;						// Protocol address length (1 byte)
		_llep.arp.opcode = 1;							// Operation code (2 bytes) - ARP_request(1), ARP_reply(2)
		memcpy(&_llep.arp.source_hw, _llMacAddr, 6);	// Source HW address (6 bytes)
		memset(&_llep.arp.source_protocol, 0, 4);		// Source Protocol address (4 bytes) 0.0.0.0
		memset(&_llep.arp.target_hw, 0, 6);				// Target HW address (6 bytes) 00:00:00:00:00:00
		memset(&_llep.arp.target_protocol, 0, 4);		// Target Protocol address (4 bytes) 0.0.0.0 - dummy value for now
	}
	memset(&_llep.padding, 0, 10);						// PADDING (10 bytes)
	_llep.crc = 0;   									// CRC (4 bytes) TODO - IMPLEMENT

	// Send it after a random interval between 0..PROBE_WAIT after booting
	delay( random(0, PROBE_WAIT*1000) );

	// Setup a raw ethernet socket
	_llRawSocket.begin();

	uint8_t tries;
	uint8_t found_conflict[MAX_CONFLICTS];
	memset(found_conflict,0,sizeof(found_conflict));

	for(tries=0; tries< MAX_CONFLICTS; tries++)
	{
		// Generate a new autoip address for each try skipping prior conflicts
		uint8_t match;
		do {
			_llLocalIp[3] = rand()%253 + 1;
			for(match=0; match<tries; match++) {
				if (_llLocalIp[3] == found_conflict[match])
					break;
			}
		} while (match != tries);
		memset(&_llep.arp.target_protocol, _llLocalIp, 4);  // update with the candidate IP address

		// Send it PROBE_NUM times
		for(uint8_t n=0; n< PROBE_NUM; n++) {
			// Open the MACRAW socket - it is forced onto Socket[0]
			_llRawSocket.beginPacket(_llLocalIp, _llLocalPort);
			_llRawSocket.write((uint8_t *)&_llep,sizeof(_llep));

			// Wait PROBE_MIN...PROBE_MAX seconds before sending each packet
			delay( random(PROBE_MIN*1000, PROBE_MAX*1000) );

			// Send ARP PROBE
			_llRawSocket.endPacket();
		}

		// Wait ANNOUCE_WAIT period after sending the last packet before checking for a response
		delay( ANNOUNCE_WAIT*1000 );  // Wait n seconds before checking for reply

		// Check for inbound ARP_RESPONSE. If no conflict after ANNOUNCE_WAIT seconds it's ok to use it.
		while( _llRawSocket.parsePacket() ) {
			Ethernet_packet reply;
			int size = _llRawSocket.read((char*)&reply, sizeof(reply));
			// if we don't even have enough for a full arp packet bail early.
			if( size < sizeof(reply) )
				continue;
			if( reply.eth_type == htons(0x0806) ) {
				// Recieved ARP PACKET
/* 				Serial.printf("\nARP PACKET RECEIVED");
				Serial.printf("\n\t hw_type=%d (%s)", reply.arp.hw_type,
												reply.arp.hw_type==1 ? "Ethernet" : "UNKNOWN");
				Serial.printf("\n\t hw_len=%d", reply.arp.hw_len); */

				if( reply.arp.hw_type==htons(1) ) {
					// Recieved ARP PACKET FOR ETHERNET
/* 					Serial.printf("\n\t source_hw=%02X::%02X::%02X::%02X::%02X::%02X", 
													reply.arp.source_hw[0],
													reply.arp.source_hw[1],
													reply.arp.source_hw[2],
													reply.arp.source_hw[3],
													reply.arp.source_hw[4],
													reply.arp.source_hw[5] );

					Serial.printf("\n\t target_hw=%02X::%02X::%02X::%02X::%02X::%02X", 
													reply.arp.target_hw[0],
													reply.arp.target_hw[1],
													reply.arp.target_hw[2],
													reply.arp.target_hw[3],
													reply.arp.target_hw[4],
													reply.arp.target_hw[5] );

					Serial.printf("\n\t protocol_type=%d (%s)", reply.arp.protocol_type, 
													reply.arp.protocol_type==htons(0x0800))? "IPv4" : "UNKNOWN");
					Serial.printf("\n\t protocol_len=%d", reply.arp.protocol_len);  */

					if( reply.arp.protocol_type == htons(0x0800) ) {
						// Recieved ARP PACKET FOR IPv4 OVER ETHERNET
/* 						Serial.printf("\n\t source_protocol=%u.%u.%u.%u",
													reply.arp.source_protocol[0],
													reply.arp.source_protocol[1],
													reply.arp.source_protocol[2],
													reply.arp.source_protocol[3] );
						Serial.printf("\n\t target_protocol=%u.%u.%u.%u",
													reply.arp.target_protocol[0],
													reply.arp.target_protocol[1],
													reply.arp.target_protocol[2],
													reply.arp.target_protocol[3] );
						Serial.printf("\n\t opcode=%d (%s)", reply.arp.opcode,
													(reply.arp.opcode == 1) ? "ARP_REQUEST" : (reply.arp.opcode == 2) ? "ARP_RESPONSE" : "UNKNOWN" ); */

						if( reply.arp.opcode == htons(1) | reply.arp.opcode == htons(2) ) {
							// ETHERNET IPv4 ARP_REQUEST or ARP_RESPONSE
							// check sourceIP to see if it is a conflict, save it if it is
							found_conflict[tries] = (memcmp(reply.eth_src, _llLocalIp, 4) == 0)? _llLocalIp[3]:0;
							if(found_conflict[tries]) {
								// Drain remaining packets and start again
								while( _llRawSocket.parsePacket() );
							}
						}
					}
				}
			}
		}
		if(found_conflict[tries]) {
			continue;
		}
		// Success!!
		break;
	}
	if(tries == MAX_CONFLICTS)
	{
		Serial.println("Too many IP link-local assignment conflicts.");
		return 0;
	}
	// Save the negotiated settings
	memcpy(_llGatewayIp, _llLocalIp, 4);
	Ethernet.setLocalIP    ( IPAddress(_llLocalIp)    );
	Ethernet.setSubnetMask ( IPAddress(_llSubnetMask) );
	Ethernet.setGatewayIP  ( IPAddress(_llGatewayIp)  );
	Ethernet.setDnsServerIP( IPAddress(_llLocalIp)    );
	
	return 1;
}

IPAddress LinkLocalClass::getLocalIp()
{
	return IPAddress(_llLocalIp);
}

IPAddress LinkLocalClass::getSubnetMask()
{
	return IPAddress(_llSubnetMask);
}

IPAddress LinkLocalClass::getGatewayIp()
{
	return IPAddress(_llGatewayIp);
}

void LinkLocalClass::printByte(char * buf, uint8_t n )
{
	char *str = &buf[1];
	buf[0]='0';
	do {
		unsigned long m = n;
		n /= 16;
		char c = m - 16 * n;
		*str-- = c < 10 ? c + '0' : c + 'A' - 10;
	} while(n);
}
