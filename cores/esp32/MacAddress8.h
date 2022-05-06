//-----------------------------------------------------------------------------
// MacAddress8.h - class to make it easier to handle 8-byte EUI-64 addresses.
// 
// Copyright 2022 David McCurley
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

#ifndef MacAddress8_h
#define MacAddress8_h

#include <stdint.h>
#include <WString.h>
#include <Printable.h>

// A class to make it easier to handle and pass around 8-byte EUI-64(used for IEEE 802.15.4) addresses. See <esp_mac.h>.
class MacAddress8 : public Printable {
private:
    union {
        uint8_t bytes[8];
        uint64_t val;
    } _mac;

public:
    MacAddress8();
    MacAddress8(uint64_t mac);
    MacAddress8(const uint8_t *macbytearray);
    MacAddress8(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7, uint8_t b8);
    virtual ~MacAddress8() {}
    bool fromCStr(const char *buf);
    bool fromString(const String &macstr);
    void toBytes(uint8_t *buf);
    int  toCStr(char *buf);
    String toString() const;
    uint64_t Value();

    uint8_t operator[](int index) const;
    uint8_t& operator[](int index);
    MacAddress8& operator=(const uint8_t *macbytearray);
    MacAddress8& operator=(uint64_t macval);
    bool operator==(const uint8_t *macbytearray) const;
    bool operator==(const MacAddress8& mac2) const;
    operator uint64_t() const;
    operator const uint8_t*() const;
    operator const uint64_t*() const;

    virtual size_t printTo(Print& p) const;

    // future use in Arduino Networking 
    friend class EthernetClass;
    friend class UDP;
    friend class Client;
    friend class Server;
    friend class DhcpClass;
    friend class DNSClient;

private:
    int EnforceIndexBounds(int i) const;
};

#endif
