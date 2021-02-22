// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef IPv6Address_h
#define IPv6Address_h

#include <stdint.h>
#include <WString.h>
#include <Printable.h>

// A class to make it easier to handle and pass around IP addresses

class IPv6Address: public Printable
{
private:
    union {
        uint8_t bytes[16];  // IPv4 address
        uint32_t dword[4];
    } _address;

    // Access the raw byte array containing the address.  Because this returns a pointer
    // to the internal structure rather than a copy of the address this function should only
    // be used when you know that the usage of the returned uint8_t* will be transient and not
    // stored.
    uint8_t* raw_address()
    {
        return _address.bytes;
    }

public:
    // Constructors
    IPv6Address();
    IPv6Address(const uint8_t *address);
    IPv6Address(const uint32_t *address);
    virtual ~IPv6Address() {}

    bool fromString(const char *address);
    bool fromString(const String &address) { return fromString(address.c_str()); }

    operator const uint8_t*() const
    {
        return _address.bytes;
    }
    operator const uint32_t*() const
    {
        return _address.dword;
    }
    bool operator==(const IPv6Address& addr) const
    {
        return (_address.dword[0] == addr._address.dword[0])
            && (_address.dword[1] == addr._address.dword[1])
            && (_address.dword[2] == addr._address.dword[2])
            && (_address.dword[3] == addr._address.dword[3]);
    }
    bool operator==(const uint8_t* addr) const;

    // Overloaded index operator to allow getting and setting individual octets of the address
    uint8_t operator[](int index) const
    {
        return _address.bytes[index];
    }
    uint8_t& operator[](int index)
    {
        return _address.bytes[index];
    }

    // Overloaded copy operators to allow initialisation of IPv6Address objects from other types
    IPv6Address& operator=(const uint8_t *address);

    virtual size_t printTo(Print& p) const;
    String toString() const;

    friend class UDP;
    friend class Client;
    friend class Server;
};

#endif
