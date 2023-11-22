/*
 IPAddress.cpp - Base class that provides IPAddress
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

#include <Arduino.h>
#include <IPAddress.h>
#include <Print.h>
#include <StreamString.h>

IPAddress::IPAddress() {
#if LWIP_IPV6
  _ip = *IP6_ADDR_ANY;
#else
  _ip = *IP_ADDR_ANY;
#endif
}

IPAddress::IPAddress(const IPAddress& from)
{
    ip_addr_copy(_ip, from._ip);
}

IPAddress::IPAddress(uint8_t first_octet, uint8_t second_octet, uint8_t third_octet, uint8_t fourth_octet) {
    uint8_t addr[] {
        first_octet,
        second_octet,
        third_octet,
        fourth_octet,
    };
    *this = &addr[0];
}

IPAddress::IPAddress(uint8_t o1, uint8_t o2, uint8_t o3, uint8_t o4, uint8_t o5, uint8_t o6, uint8_t o7, uint8_t o8, uint8_t o9, uint8_t o10, uint8_t o11, uint8_t o12, uint8_t o13, uint8_t o14, uint8_t o15, uint8_t o16) {
    setV6();
    (*this)[0] = o1;
    (*this)[1] = o2;
    (*this)[2] = o3;
    (*this)[3] = o4;
    (*this)[4] = o5;
    (*this)[5] = o6;
    (*this)[6] = o7;
    (*this)[7] = o8;
    (*this)[8] = o9;
    (*this)[9] = o10;
    (*this)[10] = o11;
    (*this)[11] = o12;
    (*this)[12] = o13;
    (*this)[13] = o14;
    (*this)[14] = o15;
    (*this)[15] = o16;
}

IPAddress::IPAddress(IPType type, const uint8_t *address) {
    IPAddress(type, address, 0);
}

IPAddress::IPAddress(IPType type, const uint8_t *address, uint8_t zone) {
    if (type == IPv4) {
        setV4();
        memcpy(&this->_ip.u_addr.ip4, address, 4);
    } else if (type == IPv6) {
        setV6();
        memcpy(&this->_ip.u_addr.ip6.addr[0], address, 16);
        setZone(zone);
    } else {
#if LWIP_IPV6
        _ip = *IP6_ADDR_ANY;
#else
        _ip = *IP_ADDR_ANY;
#endif
    }

}

bool IPAddress::fromString(const char *address) {
    if (!fromString4(address)) {
#if LWIP_IPV6
        return fromString6(address);
#else
        return false;
#endif
    }
    return true;
}

bool IPAddress::fromString4(const char *address) {
    // TODO: (IPv4) add support for "a", "a.b", "a.b.c" formats

    uint16_t acc = 0; // Accumulator
    uint8_t dots = 0;

    while (*address)
    {
        char c = *address++;
        if (c >= '0' && c <= '9')
        {
            acc = acc * 10 + (c - '0');
            if (acc > 255) {
                // Value out of [0..255] range
                return false;
            }
        }
        else if (c == '.')
        {
            if (dots == 3) {
                // Too much dots (there must be 3 dots)
                return false;
            }
            (*this)[dots++] = acc;
            acc = 0;
        }
        else
        {
            // Invalid char
            return false;
        }
    }

    if (dots != 3) {
        // Too few dots (there must be 3 dots)
        return false;
    }
    (*this)[3] = acc;

    setV4();
    return true;
}

IPAddress& IPAddress::operator=(const uint8_t *address) {
    uint32_t value;
    memcpy_P(&value, address, sizeof(value));
    *this = value;
    return *this;
}

IPAddress& IPAddress::operator=(uint32_t address) {
    setV4();
    v4() = address;
    return *this;
}

bool IPAddress::operator==(const uint8_t* addr) const {
    if (!isV4()) {
        return false;
    }

    uint32_t value;
    memcpy_P(&value, addr, sizeof(value));

    return v4() == value;
}

size_t IPAddress::printTo(Print& p) const {
    size_t n = 0;

#if LWIP_IPV6
    if (isV6()) {
        int count0 = 0;
        for (int i = 0; i < 8; i++) {
            uint16_t bit = PP_NTOHS(raw6()[i]);
            if (bit || count0 < 0) {
                n += p.printf("%x", bit);
                if (count0 > 0)
                    // no more hiding 0
                    count0 = -8;
            } else
                count0++;
            if ((i != 7 && count0 < 2) || count0 == 7)
                n += p.print(':');
        }
        // add a zone if zone-id si non-zero
        if (_ip.u_addr.ip6.zone) {
            n += p.print('%');
            char if_name[NETIF_NAMESIZE];
            netif_index_to_name(_ip.u_addr.ip6.zone, if_name);
            n += p.print(if_name);
        }
        return n;
    }
#endif

    for(int i = 0; i < 4; i++) {
        n += p.print((*this)[i], DEC);
        if (i != 3)
            n += p.print('.');
    }
    return n;
}

String IPAddress::toString() const
{
    StreamString sstr;
#if LWIP_IPV6
    if (isV6())
        sstr.reserve(44); // 8 shorts x 4 chars each + 7 colons + nullterm + '%' + zone-id
    else
#endif
        sstr.reserve(16); // 4 bytes with 3 chars max + 3 dots + nullterm, or '(IP unset)'
    printTo(sstr);
    return sstr;
}

bool IPAddress::isValid(const String& arg) {
	return IPAddress().fromString(arg);
}

bool IPAddress::isValid(const char* arg) {
	return IPAddress().fromString(arg);
}

const IPAddress INADDR46_ANY; // generic "0.0.0.0" for IPv4 & IPv6
const IPAddress INADDR46_NONE(255,255,255,255);

void IPAddress::clear() {
    (*this) = INADDR46_ANY;
}

/**************************************/

#if LWIP_IPV6

bool IPAddress::fromString6(const char *address) {
    ip6_addr_t ip6;
    if (ip6addr_aton(address, &ip6)) {
        setV6();
        memcpy(&this->_ip.u_addr.ip6.addr[0], ip6.addr, 16);
        // look for '%' in string
        const char *s = address;
        while (*s && *s != '%') { s++; }
        if (*s == '%') {
            // we have a zone id
            setZone(netif_name_to_index(s + 1));
        }
        return true;
    }
    return false;
}
#endif // LWIP_IPV6

// declared one time - as external in IPAddress.h
IPAddress INADDR_NONE(0, 0, 0, 0);      // TODO
