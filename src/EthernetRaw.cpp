/*
 *  EthernetRaw.cpp: Library to send/receive RAW packets with the Arduino ethernet shield.
 *  This version only offers minimal wrapping of socket.cpp
 *  Drop EthernetRaw.h/.cpp into the Ethernet library directory at hardware/libraries/Ethernet/
 *
 * MIT License:
 * Copyright (c) 2019 Edward Richards
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <Arduino.h>
#include "Ethernet.h"
#include "utility/w5100.h"

/* Start EthernetRaw socket */
uint8_t EthernetRaw::begin()
{
	// Note: Only Socket 0 can do raw sockets
	// Open raw Ethernet packet with IPv6 and Multicast RX disabled
	sockindex = Ethernet.socketBegin(SnMR::MACRAW|SnMR::MIP6B|SnMR::MMB, 0);
	if (sockindex != 0) return 0;
	_remaining = 0;
	return 1;
}

/* return number of bytes available in the current packet,
   will return zero if parsePacket hasn't been called yet */
int EthernetRaw::available()
{
	return _remaining;
}

/* Release any resources being used by this EthernetRaw instance */
void EthernetRaw::stop()
{
	if (sockindex == 0) {
		Ethernet.socketClose(sockindex);
		sockindex = MAX_SOCK_NUM;
	}
}

int EthernetRaw::beginPacket(IPAddress ip, uint16_t port)
{
	_offset = 0;
	Serial.println("Raw beginPacket\n");
	SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
	W5100.writeSnDIPR(0, ip);
	W5100.writeSnDPORT(0, port);
	SPI.endTransaction();
	return true;
}

int EthernetRaw::endPacket()
{
	return Ethernet.socketSendUDP(sockindex);
}

size_t EthernetRaw::write(uint8_t byte)
{
	return write(&byte, 1);
}

size_t EthernetRaw::write(const uint8_t *buffer, size_t size)
{
	//Serial.printf("UDP write %d\n", size);
	uint16_t bytes_written = Ethernet.socketBufferData(sockindex, _offset, buffer, size);
	_offset += bytes_written;
	return bytes_written;
}

int EthernetRaw::parsePacket()
{
	// discard any remaining bytes in the last packet
	while (_remaining) {
		// could this fail (loop endlessly) if _remaining > 0 and recv in read fails?
		// should only occur if recv fails after telling us the data is there, lets
		// hope the w5100 always behaves :)
		read((uint8_t *)NULL, _remaining);
	}

	_remaining = Ethernet.socketRecvAvailable(sockindex);
	return _remaining;
}

int EthernetRaw::read()
{
	uint8_t byte;

	if ((_remaining > 0) && (Ethernet.socketRecv(sockindex, &byte, 1) > 0)) {
		// We read things without any problems
		_remaining--;
		return byte;
	}

	// If we get here, there's no data available
	return -1;
}

int EthernetRaw::read(unsigned char *buffer, size_t len)
{
	if (_remaining > 0) {
		int got;
		if (_remaining <= len) {
			// data should fit in the buffer
			got = Ethernet.socketRecv(sockindex, buffer, _remaining);
		} else {
			// too much data for the buffer,
			// grab as much as will fit
			got = Ethernet.socketRecv(sockindex, buffer, len);
		}
		if (got > 0) {
			_remaining -= got;
			//Serial.printf("UDP read %d\n", got);
			return got;
		}
	}
	// If we get here, there's no data available or recv failed
	return -1;
}

int EthernetRaw::peek()
{
	// Unlike recv, peek doesn't check to see if there's any data available, so we must.
	// If the user hasn't called parsePacket yet then return nothing otherwise they
	// may get the UDP header
	if (sockindex >= MAX_SOCK_NUM || _remaining == 0) return -1;
	return Ethernet.socketPeek(sockindex);
}

void EthernetRaw::flush()
{
	// TODO: we should wait for TX buffer to be emptied
}

