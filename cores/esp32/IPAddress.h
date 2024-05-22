/*
  IPAddress.h - Base class that provides IPAddress
  Copyright (c) 2011 Adrian McEwen.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#pragma once

#include <stdint.h>
#include "Printable.h"
#include "WString.h"
#include "lwip/ip_addr.h"
#include "esp_netif_ip_addr.h"

#define IPADDRESS_V4_BYTES_INDEX 12
#define IPADDRESS_V4_DWORD_INDEX 3

// A class to make it easier to handle and pass around IP addresses

enum IPType {
  IPv4,
  IPv6
};

class IPAddress : public Printable {
private:
  union {
    uint8_t bytes[16];
    uint32_t dword[4];
  } _address;
  IPType _type;
  uint8_t _zone;

  // Access the raw byte array containing the address.  Because this returns a pointer
  // to the internal structure rather than a copy of the address this function should only
  // be used when you know that the usage of the returned uint8_t* will be transient and not
  // stored.
  uint8_t *raw_address() {
    return _type == IPv4 ? &_address.bytes[IPADDRESS_V4_BYTES_INDEX] : _address.bytes;
  }

public:
  // Constructors

  // Default IPv4
  IPAddress();
  IPAddress(IPType ip_type);
  IPAddress(uint8_t first_octet, uint8_t second_octet, uint8_t third_octet, uint8_t fourth_octet);
  IPAddress(
    uint8_t o1, uint8_t o2, uint8_t o3, uint8_t o4, uint8_t o5, uint8_t o6, uint8_t o7, uint8_t o8, uint8_t o9, uint8_t o10, uint8_t o11, uint8_t o12,
    uint8_t o13, uint8_t o14, uint8_t o15, uint8_t o16, uint8_t z = 0
  );
  // IPv4; see implementation note
  IPAddress(uint32_t address);
  // Default IPv4
  IPAddress(const uint8_t *address);
  IPAddress(IPType ip_type, const uint8_t *address, uint8_t z = 0);
  // If IPv4 fails tries IPv6 see fromString function
  IPAddress(const char *address);
  IPAddress(const IPAddress &address);

  bool fromString(const char *address);
  bool fromString(const String &address) {
    return fromString(address.c_str());
  }

  // Overloaded cast operator to allow IPAddress objects to be used where a uint32_t is expected
  // NOTE: IPv4 only; see implementation note
  operator uint32_t() const {
    return _type == IPv4 ? _address.dword[IPADDRESS_V4_DWORD_INDEX] : 0;
  };

  bool operator==(const IPAddress &addr) const;
  bool operator!=(const IPAddress &addr) const {
    return !(*this == addr);
  };

  // NOTE: IPv4 only; we don't know the length of the pointer
  bool operator==(const uint8_t *addr) const;

  // Overloaded index operator to allow getting and setting individual octets of the address
  uint8_t operator[](int index) const;
  uint8_t &operator[](int index);

  // Overloaded copy operators to allow initialization of IPAddress objects from other types
  // NOTE: IPv4 only
  IPAddress &operator=(const uint8_t *address);
  // NOTE: IPv4 only; see implementation note
  IPAddress &operator=(uint32_t address);
  // If IPv4 fails tries IPv6 see fromString function
  IPAddress &operator=(const char *address);
  IPAddress &operator=(const IPAddress &address);

  virtual size_t printTo(Print &p) const;
  String toString(bool includeZone = false) const;

  IPType type() const {
    return _type;
  }

  // Espresif LwIP conversions
  IPAddress(const ip_addr_t *addr);
  void to_ip_addr_t(ip_addr_t *addr) const;
  IPAddress &from_ip_addr_t(const ip_addr_t *addr);
  esp_ip6_addr_type_t addr_type() const;
  uint8_t zone() const {
    return (type() == IPv6) ? _zone : 0;
  }
  size_t printTo(Print &p, bool includeZone) const;

  friend class UDP;
  friend class Client;
  friend class Server;
  friend class EthernetClass;
  friend class DhcpClass;
  friend class DNSClient;

protected:
  bool fromString4(const char *address);
  bool fromString6(const char *address);
};

extern const IPAddress IN6ADDR_ANY;
extern const IPAddress INADDR_NONE;
