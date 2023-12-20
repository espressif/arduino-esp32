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

IPAddress::IPAddress() : IPAddress(IPv4) {}

IPAddress::IPAddress(IPType ip_type)
{
    _type = ip_type;
    memset(_address.bytes, 0, sizeof(_address.bytes));
}

IPAddress::IPAddress(uint8_t first_octet, uint8_t second_octet, uint8_t third_octet, uint8_t fourth_octet)
{
    _type = IPv4;
    memset(_address.bytes, 0, sizeof(_address.bytes));
    _address.bytes[IPADDRESS_V4_BYTES_INDEX] = first_octet;
    _address.bytes[IPADDRESS_V4_BYTES_INDEX + 1] = second_octet;
    _address.bytes[IPADDRESS_V4_BYTES_INDEX + 2] = third_octet;
    _address.bytes[IPADDRESS_V4_BYTES_INDEX + 3] = fourth_octet;
}

IPAddress::IPAddress(uint8_t o1, uint8_t o2, uint8_t o3, uint8_t o4, uint8_t o5, uint8_t o6, uint8_t o7, uint8_t o8, uint8_t o9, uint8_t o10, uint8_t o11, uint8_t o12, uint8_t o13, uint8_t o14, uint8_t o15, uint8_t o16) {
    _type = IPv6;
    _address.bytes[0] = o1;
    _address.bytes[1] = o2;
    _address.bytes[2] = o3;
    _address.bytes[3] = o4;
    _address.bytes[4] = o5;
    _address.bytes[5] = o6;
    _address.bytes[6] = o7;
    _address.bytes[7] = o8;
    _address.bytes[8] = o9;
    _address.bytes[9] = o10;
    _address.bytes[10] = o11;
    _address.bytes[11] = o12;
    _address.bytes[12] = o13;
    _address.bytes[13] = o14;
    _address.bytes[14] = o15;
    _address.bytes[15] = o16;
}

IPAddress::IPAddress(uint32_t address)
{
    // IPv4 only
    _type = IPv4;
    memset(_address.bytes, 0, sizeof(_address.bytes));
    _address.dword[IPADDRESS_V4_DWORD_INDEX] = address;

    // NOTE on conversion/comparison and uint32_t:
    // These conversions are host platform dependent.
    // There is a defined integer representation of IPv4 addresses,
    // based on network byte order (will be the value on big endian systems),
    // e.g. http://2398766798 is the same as http://142.250.70.206,
    // However on little endian systems the octets 0x83, 0xFA, 0x46, 0xCE,
    // in that order, will form the integer (uint32_t) 3460758158 .
}

IPAddress::IPAddress(const uint8_t *address) : IPAddress(IPv4, address) {}

IPAddress::IPAddress(IPType ip_type, const uint8_t *address)
{
    _type = ip_type;
    if (ip_type == IPv4) {
        memset(_address.bytes, 0, sizeof(_address.bytes));
        memcpy(&_address.bytes[IPADDRESS_V4_BYTES_INDEX], address, sizeof(uint32_t));
    } else {
        memcpy(_address.bytes, address, sizeof(_address.bytes));
    }
}

IPAddress::IPAddress(const char *address)
{
    fromString(address);
}

IPAddress& IPAddress::operator=(const uint8_t *address)
{
    // IPv4 only conversion from byte pointer
    _type = IPv4;
    memset(_address.bytes, 0, sizeof(_address.bytes));
    memcpy(&_address.bytes[IPADDRESS_V4_BYTES_INDEX], address, sizeof(uint32_t));
    return *this;
}

IPAddress& IPAddress::operator=(const char *address)
{
    fromString(address);
    return *this;
}

IPAddress& IPAddress::operator=(uint32_t address)
{
    // IPv4 conversion
    // See note on conversion/comparison and uint32_t
    _type = IPv4;
    memset(_address.bytes, 0, sizeof(_address.bytes));
    _address.dword[IPADDRESS_V4_DWORD_INDEX] = address;
    return *this;
}

bool IPAddress::operator==(const IPAddress& addr) const
{
    return (addr._type == _type)
        && (memcmp(addr._address.bytes, _address.bytes, sizeof(_address.bytes)) == 0);
}

bool IPAddress::operator==(const uint8_t* addr) const
{
    // IPv4 only comparison to byte pointer
    // Can't support IPv6 as we know our type, but not the length of the pointer
    return _type == IPv4 && memcmp(addr, &_address.bytes[IPADDRESS_V4_BYTES_INDEX], sizeof(uint32_t)) == 0;
}

uint8_t IPAddress::operator[](int index) const {
    if (_type == IPv4) {
        return _address.bytes[IPADDRESS_V4_BYTES_INDEX + index];
    }
    return _address.bytes[index];
}

uint8_t& IPAddress::operator[](int index) {
    if (_type == IPv4) {
        return _address.bytes[IPADDRESS_V4_BYTES_INDEX + index];
    }
    return _address.bytes[index];
}

size_t IPAddress::printTo(Print& p) const
{
    size_t n = 0;

    if (_type == IPv6) {
        // IPv6 IETF canonical format: compress left-most longest run of two or more zero fields, lower case
        int8_t longest_start = -1;
        int8_t longest_length = 1;
        int8_t current_start = -1;
        int8_t current_length = 0;
        for (int8_t f = 0; f < 8; f++) {
            if (_address.bytes[f * 2] == 0 && _address.bytes[f * 2 + 1] == 0) {
                if (current_start == -1) {
                    current_start = f;
                    current_length = 1;
                } else {
                    current_length++;
                }
                if (current_length > longest_length) {
                    longest_start = current_start;
                    longest_length = current_length;
                }
            } else {
                current_start = -1;
            }
        }
        for (int f = 0; f < 8; f++) {
            if (f < longest_start || f >= longest_start + longest_length) {
                uint8_t c1 = _address.bytes[f * 2] >> 4;
                uint8_t c2 = _address.bytes[f * 2] & 0xf;
                uint8_t c3 = _address.bytes[f * 2 + 1] >> 4;
                uint8_t c4 = _address.bytes[f * 2 + 1] & 0xf;
                if (c1 > 0) {
                    n += p.print((char)(c1 < 10 ? '0' + c1 : 'a' + c1 - 10));
                }
                if (c1 > 0 || c2 > 0) {
                    n += p.print((char)(c2 < 10 ? '0' + c2 : 'a' + c2 - 10));
                }
                if (c1 > 0 || c2 > 0 || c3 > 0) {
                    n += p.print((char)(c3 < 10 ? '0' + c3 : 'a' + c3 - 10));
                }
                n += p.print((char)(c4 < 10 ? '0' + c4 : 'a' + c4 - 10));
                if (f < 7) {
                    n += p.print(':');
                }
            } else if (f == longest_start) {
                if (longest_start == 0) {
                    n += p.print(':');
                }
                n += p.print(':');
            }
        }
        return n;
    }

    // IPv4
    for (int i =0; i < 3; i++)
    {
        n += p.print(_address.bytes[IPADDRESS_V4_BYTES_INDEX + i], DEC);
        n += p.print('.');
    }
    n += p.print(_address.bytes[IPADDRESS_V4_BYTES_INDEX + 3], DEC);
    return n;
}

String IPAddress::toString4() const
{
    char szRet[16];
    snprintf(szRet, sizeof(szRet), "%u.%u.%u.%u", _address.bytes[IPADDRESS_V4_BYTES_INDEX], _address.bytes[IPADDRESS_V4_BYTES_INDEX + 1], _address.bytes[IPADDRESS_V4_BYTES_INDEX + 2], _address.bytes[IPADDRESS_V4_BYTES_INDEX + 3]);
    return String(szRet);
}

String IPAddress::toString6() const
{
    StreamString s;
    s.reserve(40);
    printTo(s);
    return s;
}

String IPAddress::toString() const
{
    if (_type == IPv4) {
        return toString4();
    } else {
        return toString6();
    }
}

bool IPAddress::fromString(const char *address)
{
    if (!fromString4(address))
    {
        return fromString6(address);
    }
    return true;
}

bool IPAddress::fromString4(const char *address)
{
    // TODO: add support for "a", "a.b", "a.b.c" formats

    int16_t acc = -1; // Accumulator
    uint8_t dots = 0;

    memset(_address.bytes, 0, sizeof(_address.bytes));
    while (*address)
    {
        char c = *address++;
        if (c >= '0' && c <= '9')
        {
            acc = (acc < 0) ? (c - '0') : acc * 10 + (c - '0');
            if (acc > 255) {
                // Value out of [0..255] range
                return false;
            }
        }
        else if (c == '.')
        {
            if (dots == 3) {
                // Too many dots (there must be 3 dots)
                return false;
            }
            if (acc < 0) {
                /* No value between dots, e.g. '1..' */
                return false;
            }
            _address.bytes[IPADDRESS_V4_BYTES_INDEX + dots++] = acc;
            acc = -1;
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
    if (acc < 0) {
        /* No value between dots, e.g. '1..' */
        return false;
    }
    _address.bytes[IPADDRESS_V4_BYTES_INDEX + 3] = acc;
    _type = IPv4;
    return true;
}

bool IPAddress::fromString6(const char *address) {
    uint32_t acc = 0; // Accumulator
    int colons = 0, double_colons = -1;

    while (*address)
    {
        char c = tolower(*address++);
        if (isalnum(c) && c <= 'f') {
            if (c >= 'a')
                c -= 'a' - '0' - 10;
            acc = acc * 16 + (c - '0');
            if (acc > 0xffff)
                // Value out of range
                return false;
        }
        else if (c == ':') {
            if (*address == ':') {
                if (double_colons >= 0) {
                    // :: allowed once
                    return false;
                }
                if (*address != '\0' && *(address + 1) == ':') {
                    // ::: not allowed
                    return false;
                }
                // remember location
                double_colons = colons + !!acc;
                address++;
            } else if (*address == '\0') {
                // can't end with a single colon
                return false;
            }
            if (colons == 7)
                // too many separators
                return false;
            _address.bytes[colons * 2] = acc >> 8;
            _address.bytes[colons * 2 + 1] = acc & 0xff;
            colons++;
            acc = 0;
        }
        else
            // Invalid char
            return false;
    }

    if (double_colons == -1 && colons != 7) {
        // Too few separators
        return false;
    }
    if (double_colons > -1 && colons > 6) {
        // Too many segments (double colon must be at least one zero field)
        return false;
    }
    _address.bytes[colons * 2] = acc >> 8;
    _address.bytes[colons * 2 + 1] = acc & 0xff;
    colons++;

    if (double_colons != -1) {
        for (int i = colons * 2 - double_colons * 2 - 1; i >= 0; i--)
            _address.bytes[16 - colons * 2 + double_colons * 2 + i] = _address.bytes[double_colons * 2 + i];
        for (int i = double_colons * 2; i < 16 - colons * 2 + double_colons * 2; i++)
            _address.bytes[i] = 0;
    }

    _type = IPv6;
    return true;
}

// declared one time - as external in IPAddress.h
IPAddress INADDR_NONE(0, 0, 0, 0);
