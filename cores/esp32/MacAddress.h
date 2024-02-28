//-----------------------------------------------------------------------------
// MacAddress.h - class to make it easier to handle BSSID and MAC addresses.
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

#ifndef MacAddress_h
#define MacAddress_h

#include <stdint.h>
#include <WString.h>
#include <Printable.h>

// A class to make it easier to handle and pass around 6-byte BSSID and MAC addresses.
class MacAddress : public Printable {
private:
    union {
        struct {
            uint8_t align[2];
            uint8_t bytes[6];
        };
        uint64_t val;
    } _mac;

public:
    MacAddress();
    MacAddress(uint64_t mac);
    MacAddress(const uint8_t *macbytearray);
    MacAddress(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6);
    virtual ~MacAddress() {}
    bool fromCStr(const char *buf);
    bool fromString(const String &macstr);
    void toBytes(uint8_t *buf);
    int  toCStr(char *buf);
    String toString() const;
    uint64_t Value();

    uint8_t operator[](int index) const;
    uint8_t& operator[](int index);
    MacAddress& operator=(const uint8_t *macbytearray);
    MacAddress& operator=(uint64_t macval);
    bool operator==(const uint8_t *macbytearray) const;
    bool operator==(const MacAddress& mac2) const;
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
