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

#ifndef IPAddress_h
#define IPAddress_h

#include <WString.h>
#include <Printable.h>
#include <lwip/netif.h>

enum IPType
{
    IPv4 = IPADDR_TYPE_V4,
    IPv6 = IPADDR_TYPE_V6
};

class IPAddress: public Printable
{
private:
    ip_addr_t _ip;

    // Access the raw byte array containing the address.  Because this returns a pointer
    // to the internal structure rather than a copy of the address this function should only
    // be used when you know that the usage of the returned uint8_t* will be transient and not
    // stored.
    uint8_t* raw_address() {
        return reinterpret_cast<uint8_t*>(&v4());
    }
    const uint8_t* raw_address() const {
        return reinterpret_cast<const uint8_t*>(&v4());
    }

    void ctor32 (uint32_t);

public:
    // Constructors
    IPAddress();
    IPAddress(const IPAddress& from);
    IPAddress(uint8_t first_octet, uint8_t second_octet, uint8_t third_octet, uint8_t fourth_octet);
    IPAddress(uint8_t o1, uint8_t o2, uint8_t o3, uint8_t o4, uint8_t o5, uint8_t o6, uint8_t o7, uint8_t o8, uint8_t o9, uint8_t o10, uint8_t o11, uint8_t o12, uint8_t o13, uint8_t o14, uint8_t o15, uint8_t o16);
    IPAddress(uint32_t address) { ctor32(address); }
    IPAddress(const uint8_t *address);      // v4 only
    IPAddress(IPType type, const uint8_t *address);

    bool fromString(const char *address);
    bool fromString(const String &address) { return fromString(address.c_str()); }

    // Overloaded cast operator to allow IPAddress objects to be used where a pointer
    // to a four-byte uint8_t array is expected
    operator uint32_t() const { return isV4()? v4(): (uint32_t)0; }
    operator uint32_t()       { return isV4()? v4(): (uint32_t)0; }

    // generic IPv4 wrapper to uint32-view like arduino loves to see it
    const uint32_t& v4() const { return ip_2_ip4(&_ip)->addr; } // for raw_address(const)
            uint32_t& v4()       { return ip_2_ip4(&_ip)->addr; }

    bool operator==(const IPAddress& addr) const {
        return ip_addr_cmp(&_ip, &addr._ip);
    }
    bool operator!=(const IPAddress& addr) const {
        return !ip_addr_cmp(&_ip, &addr._ip);
    }
    bool operator==(uint32_t addr) const {
        return isV4() && v4() == addr;
    }
    // bool operator==(unsigned long addr) const {
    //     return isV4() && v4() == (uint32_t)addr;
    // }
    bool operator!=(uint32_t addr) const {
        return !(isV4() && v4() == addr);
    }
    // bool operator!=(unsigned long addr) const {
    //     return isV4() && v4() != (uint32_t)addr;
    // }
    bool operator==(const uint8_t* addr) const;

    int operator>>(int n) const {
        return isV4()? v4() >> n: 0;
    }

    // Overloaded index operator to allow getting and setting individual octets of the address
    uint8_t operator[](int index) const {
        return isV4()? *(raw_address() + index): 0;
    }
    uint8_t& operator[](int index) {
        setV4();
        return *(raw_address() + index);
    }

    // Overloaded copy operators to allow initialisation of IPAddress objects from other types
    IPAddress& operator=(const uint8_t *address);
    IPAddress& operator=(uint32_t address);
    IPAddress& operator=(const IPAddress&) = default;

    IPType type() const { return (IPType)_ip.type; }

    virtual size_t printTo(Print& p) const;
    String toString() const;

    void clear();

    /*
            check if input string(arg) is a valid IPV4 address or not.
            return true on valid.
            return false on invalid.
    */
    static bool isValid(const String& arg);
    static bool isValid(const char* arg);

    friend class EthernetClass;
    friend class UDP;
    friend class Client;
    friend class Server;
    friend class DhcpClass;
    friend class DNSClient;

    operator       ip_addr_t () const { return  _ip; }
    operator const ip_addr_t*() const { return &_ip; }
    operator       ip_addr_t*()       { return &_ip; }

    bool isV4() const { return IP_IS_V4_VAL(_ip); }
    void setV4() { IP_SET_TYPE_VAL(_ip, IPADDR_TYPE_V4); }

    bool isLocal() const { return ip_addr_islinklocal(&_ip); }
    bool isAny() const { return ip_addr_isany_val(_ip); }

#if LWIP_IPV6
    IPAddress(const ip_addr_t& lwip_addr) { ip_addr_copy(_ip, lwip_addr); }
    IPAddress(const ip_addr_t* lwip_addr) { ip_addr_copy(_ip, *lwip_addr); }

    IPAddress& operator=(const ip_addr_t& lwip_addr) { ip_addr_copy(_ip, lwip_addr); return *this; }
    IPAddress& operator=(const ip_addr_t* lwip_addr) { ip_addr_copy(_ip, *lwip_addr); return *this; }

    uint16_t* raw6()
    {
        setV6();
        return reinterpret_cast<uint16_t*>(ip_2_ip6(&_ip));
    }

    const uint16_t* raw6() const
    {
        return isV6()? reinterpret_cast<const uint16_t*>(ip_2_ip6(&_ip)): nullptr;
    }

    // when not IPv6, ip_addr_t == ip4_addr_t so this one would be ambiguous
    // required otherwise
    operator const ip4_addr_t*() const { return isV4()? ip_2_ip4(&_ip): nullptr; }

    bool isV6() const { return IP_IS_V6_VAL(_ip); }
    void setV6() {
        IP_SET_TYPE_VAL(_ip, IPADDR_TYPE_V6);
        ip6_addr_clear_zone(ip_2_ip6(&_ip));
    }

protected:
    bool fromString6(const char *address);

#else

    // allow portable code when IPv6 is not enabled

    uint16_t* raw6() { return nullptr; }
    const uint16_t* raw6() const { return nullptr; }
    bool isV6() const { return false; }
    void setV6() { }

#endif

protected:
    bool fromString4(const char *address);
};

// changed to extern because const declaration creates copies in BSS of INADDR_NONE for each CPP unit that includes it
extern IPAddress INADDR_NONE;
extern IPAddress IN6ADDR_ANY;

#endif
