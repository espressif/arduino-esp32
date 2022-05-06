#include <MacAddress8.h>
#include <stdio.h>
#include <Print.h>

//Default constructor, blank mac address.
MacAddress8::MacAddress8() {
    _mac.val = 0;
}

MacAddress8::MacAddress8(uint64_t mac) {
    _mac.val = mac;
}

MacAddress8::MacAddress8(const uint8_t *macbytearray) {
    memcpy(_mac.bytes, macbytearray, sizeof(_mac.bytes));
}

MacAddress8::MacAddress8(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7, uint8_t b8) {
    _mac.bytes[0] = b1;
    _mac.bytes[1] = b2;
    _mac.bytes[2] = b3;
    _mac.bytes[3] = b4;
    _mac.bytes[4] = b5;
    _mac.bytes[5] = b6;
    _mac.bytes[6] = b7;
    _mac.bytes[7] = b8;
}

//Parse user entered string into MAC address
bool MacAddress8::fromCStr(const char *buf) {
  char cs[24];
  char *token;
  char *next; //Unused but required
  int i;

  strncpy(cs, buf, sizeof(cs)); //strtok modifies the buffer: copy to working buffer.

  for(i=0; i<sizeof(_mac.bytes); i++) {
    token = strtok((i==0) ? cs : NULL, ":");    //Find first or next token
    if(!token) {                                //No more tokens found
        return false;
    }
    _mac.bytes[i] = strtol(token, &next, 16);
  }
  return true;
}

//Parse user entered string into MAC address
bool MacAddress8::fromString(const String &macstr) {
    return fromCStr(macstr.c_str());
}

//Copy MAC into 8 byte array
void MacAddress8::toBytes(uint8_t *buf) {
    memcpy(buf, _mac.bytes, sizeof(_mac.bytes));
}

//Print MAC address into a C string.
//EUI-64: Buffer must be at least 24 chars
int MacAddress8::toCStr(char *buf) {
    return sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
        _mac.bytes[0], _mac.bytes[1], _mac.bytes[2], _mac.bytes[3],
        _mac.bytes[4], _mac.bytes[5], _mac.bytes[6], _mac.bytes[7]);
}

String MacAddress8::toString() const {
    char buf[24];
    sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
        _mac.bytes[0], _mac.bytes[1], _mac.bytes[2], _mac.bytes[3],
        _mac.bytes[4], _mac.bytes[5], _mac.bytes[6], _mac.bytes[7]);
    return String(buf);
}

uint64_t MacAddress8::Value() {
    return _mac.val;
}

uint8_t MacAddress8::operator[](int index) const {
    index = EnforceIndexBounds(index);
    return _mac.bytes[index];
}

//Allow setting individual octets of the address. e.g. ma[2] = 255;
uint8_t& MacAddress8::operator[](int index) {
    index = EnforceIndexBounds(index);
    return _mac.bytes[index];
}

//Overloaded copy operator: init MacAddress object from byte array
MacAddress8& MacAddress8::operator=(const uint8_t *macbytearray) {
    memcpy(_mac.bytes, macbytearray, sizeof(_mac.bytes));
    return *this;
}

//Overloaded copy operator: init MacAddress object from uint64_t
MacAddress8& MacAddress8::operator=(uint64_t macval) {
    _mac.val = macval;
    return *this;
}

//Compare class to byte array
bool MacAddress8::operator==(const uint8_t *macbytearray) const {
    return !memcmp(_mac.bytes, macbytearray, sizeof(_mac.bytes));
}

//Allow comparing value of two classes
bool MacAddress8::operator==(const MacAddress8& mac2) const {
    return _mac.val == mac2._mac.val;
}

//Type converter object to uint64_t [same as .Value()]
MacAddress8::operator uint64_t() const {
    return _mac.val;
}

//Type converter object to read only pointer to mac bytes. e.g. const uint8_t *ip_8 = ma;
MacAddress8::operator const uint8_t*() const {
    return _mac.bytes;
}

//Type converter object to read only pointer to mac value. e.g. const uint32_t *ip_64 = ma;
MacAddress8::operator const uint64_t*() const {
    return &_mac.val;
}

size_t MacAddress8::printTo(Print& p) const
{
    size_t n = 0;
    for(int i = 0; i < 8; i++) {
        if(i){
            n += p.print(':');
        }
        n += p.printf("%02X", _mac.bytes[i]);
    }
    return n;
}

//Bounds checking
int MacAddress8::EnforceIndexBounds(int i) const {
    if(i < 0) {
        return 0;
    }
    if(i >= sizeof(_mac.bytes)) {
        return sizeof(_mac.bytes)-1;
    }
    return i;
}
