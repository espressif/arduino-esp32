//-----------------------------------------------------------------------------
// MacAddress.h - class to make it easier to handle BSSID and MAC addresses.
//
// Copyright 2022 David McCurley
// Modified by Espressif Systems 2024
//
// Licensed under the Apache License, Version 2.0 (the "License").
// You may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
//   limitations under the License.
//-----------------------------------------------------------------------------

#ifndef MacAddress_h
#define MacAddress_h

#include <stdint.h>
#include <WString.h>
#include <Printable.h>

enum MACType {
  MAC6,
  MAC8
};

// A class to make it easier to handle and pass around MAC addresses, supporting both 6-byte and 8-byte MAC addresses.
class MacAddress : public Printable {
private:
  union {
    uint8_t bytes[8];
    uint64_t val;
  } _mac;
  MACType _type;

public:
  //Default MAC6
  MacAddress();

  MacAddress(MACType mac_type);
  MacAddress(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6);
  MacAddress(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7, uint8_t b8);

  MacAddress(MACType mac_type, uint64_t mac);
  MacAddress(MACType mac_type, const uint8_t *macbytearray);

  //Default MAC6
  MacAddress(uint64_t mac) : MacAddress(MAC6, mac) {}
  MacAddress(const uint8_t *macbytearray) : MacAddress(MAC6, macbytearray) {}

  MacAddress(const char *macstr);
  MacAddress(const String &macstr);

  virtual ~MacAddress() {}

  bool fromString(const char *buf);
  bool fromString(const String &macstr) {
    return fromString(macstr.c_str());
  }

  void toBytes(uint8_t *buf);
  int toString(char *buf);
  String toString() const;
  uint64_t Value();

  uint8_t operator[](int index) const;
  uint8_t &operator[](int index);

  //MAC6 only
  MacAddress &operator=(const uint8_t *macbytearray);
  MacAddress &operator=(uint64_t macval);

  bool operator==(const uint8_t *macbytearray) const;
  bool operator==(const MacAddress &mac2) const;
  operator uint64_t() const;
  operator const uint8_t *() const;
  operator const uint64_t *() const;

  virtual size_t printTo(Print &p) const;

  // future use in Arduino Networking
  /*
    friend class EthernetClass;
    friend class UDP;
    friend class Client;
    friend class Server;
    friend class DhcpClass;
    friend class DNSClient;
    */

protected:
  bool fromString6(const char *buf);
  bool fromString8(const char *buf);

private:
  int EnforceIndexBounds(int i) const;
};

#endif
