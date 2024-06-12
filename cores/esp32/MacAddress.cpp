#include <MacAddress.h>
#include <stdio.h>
#include <Print.h>

//Default constructor, blank mac address.
MacAddress::MacAddress() : MacAddress(MAC6) {}

MacAddress::MacAddress(MACType mac_type) {
  _type = mac_type;
  memset(_mac.bytes, 0, sizeof(_mac.bytes));
}
MacAddress::MacAddress(MACType mac_type, uint64_t mac) {
  _type = mac_type;
  _mac.val = mac;
}

MacAddress::MacAddress(MACType mac_type, const uint8_t *macbytearray) {
  _type = mac_type;
  memset(_mac.bytes, 0, sizeof(_mac.bytes));
  if (_type == MAC6) {
    memcpy(_mac.bytes, macbytearray, 6);
  } else {
    memcpy(_mac.bytes, macbytearray, 8);
  }
}

MacAddress::MacAddress(const char *macstr) {
  fromString(macstr);
}

MacAddress::MacAddress(const String &macstr) {
  fromString(macstr.c_str());
}

MacAddress::MacAddress(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6) {
  _type = MAC6;
  memset(_mac.bytes, 0, sizeof(_mac.bytes));
  _mac.bytes[0] = b1;
  _mac.bytes[1] = b2;
  _mac.bytes[2] = b3;
  _mac.bytes[3] = b4;
  _mac.bytes[4] = b5;
  _mac.bytes[5] = b6;
}

MacAddress::MacAddress(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7, uint8_t b8) {
  _type = MAC8;
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
bool MacAddress::fromString(const char *buf) {
  if (strlen(buf) == 17) {
    return fromString6(buf);
  } else if (strlen(buf) == 23) {
    return fromString8(buf);
  }
  return false;
}

//Parse user entered string into MAC address
bool MacAddress::fromString6(const char *buf) {
  char cs[18];  // 17 + 1 for null terminator
  char *token;
  char *next;  //Unused but required
  int i;

  strncpy(cs, buf, sizeof(cs) - 1);  //strtok modifies the buffer: copy to working buffer.

  for (i = 0; i < 6; i++) {
    token = strtok((i == 0) ? cs : NULL, ":");  //Find first or next token
    if (!token) {                               //No more tokens found
      return false;
    }
    _mac.bytes[i] = strtol(token, &next, 16);
  }
  _type = MAC6;
  return true;
}

bool MacAddress::fromString8(const char *buf) {
  char cs[24];  // 23 + 1 for null terminator
  char *token;
  char *next;  //Unused but required
  int i;

  strncpy(cs, buf, sizeof(cs) - 1);  //strtok modifies the buffer: copy to working buffer.

  for (i = 0; i < 8; i++) {
    token = strtok((i == 0) ? cs : NULL, ":");  //Find first or next token
    if (!token) {                               //No more tokens found
      return false;
    }
    _mac.bytes[i] = strtol(token, &next, 16);
  }
  _type = MAC8;
  return true;
}

//Copy MAC into byte array
void MacAddress::toBytes(uint8_t *buf) {
  if (_type == MAC6) {
    memcpy(buf, _mac.bytes, 6);
  } else {
    memcpy(buf, _mac.bytes, sizeof(_mac.bytes));
  }
}

//Print MAC address into a C string.
//MAC: Buffer must be at least 18 chars
int MacAddress::toString(char *buf) {
  if (_type == MAC6) {
    return sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", _mac.bytes[0], _mac.bytes[1], _mac.bytes[2], _mac.bytes[3], _mac.bytes[4], _mac.bytes[5]);
  } else {
    return sprintf(
      buf, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", _mac.bytes[0], _mac.bytes[1], _mac.bytes[2], _mac.bytes[3], _mac.bytes[4], _mac.bytes[5], _mac.bytes[6],
      _mac.bytes[7]
    );
  }
}

String MacAddress::toString() const {
  uint8_t bytes = (_type == MAC6) ? 18 : 24;
  char buf[bytes];
  if (_type == MAC6) {
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", _mac.bytes[0], _mac.bytes[1], _mac.bytes[2], _mac.bytes[3], _mac.bytes[4], _mac.bytes[5]);
  } else {
    snprintf(
      buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", _mac.bytes[0], _mac.bytes[1], _mac.bytes[2], _mac.bytes[3], _mac.bytes[4], _mac.bytes[5],
      _mac.bytes[6], _mac.bytes[7]
    );
  }
  return String(buf);
}

uint64_t MacAddress::Value() {
  return _mac.val;
}

//Allow getting individual octets of the address. e.g. uint8_t b0 = ma[0];
uint8_t MacAddress::operator[](int index) const {
  index = EnforceIndexBounds(index);
  return _mac.bytes[index];
}

//Allow setting individual octets of the address. e.g. ma[2] = 255;
uint8_t &MacAddress::operator[](int index) {
  index = EnforceIndexBounds(index);
  return _mac.bytes[index];
}

//Overloaded copy operator: init MacAddress object from byte array
MacAddress &MacAddress::operator=(const uint8_t *macbytearray) {
  // 6-bytes MacAddress only
  _type = MAC6;
  memset(_mac.bytes, 0, sizeof(_mac.bytes));
  memcpy(_mac.bytes, macbytearray, 6);
  return *this;
}

//Overloaded copy operator: init MacAddress object from uint64_t
MacAddress &MacAddress::operator=(uint64_t macval) {
  // 6-bytes MacAddress only
  _type = MAC6;
  _mac.val = macval;
  return *this;
}

//Compare class to byte array
bool MacAddress::operator==(const uint8_t *macbytearray) const {
  return !memcmp(_mac.bytes, macbytearray, 6);
}

//Allow comparing value of two classes
bool MacAddress::operator==(const MacAddress &mac2) const {
  return _mac.val == mac2._mac.val;
}

//Type converter object to uint64_t [same as .Value()]
MacAddress::operator uint64_t() const {
  return _mac.val;
}

//Type converter object to read only pointer to mac bytes. e.g. const uint8_t *ip_8 = ma;
MacAddress::operator const uint8_t *() const {
  return _mac.bytes;
}

//Type converter object to read only pointer to mac value. e.g. const uint32_t *ip_64 = ma;
MacAddress::operator const uint64_t *() const {
  return &_mac.val;
}

size_t MacAddress::printTo(Print &p) const {
  uint8_t bytes = (_type == MAC6) ? 6 : 8;
  size_t n = 0;
  for (int i = 0; i < bytes; i++) {
    if (i) {
      n += p.print(':');
    }
    n += p.printf("%02X", _mac.bytes[i]);
  }
  return n;
}

//Bounds checking
int MacAddress::EnforceIndexBounds(int i) const {
  if (i < 0) {
    return 0;
  }
  if (_type == MAC6) {
    if (i >= 6) {
      return 5;
    }
  } else {
    if (i >= 8) {
      return 7;
    }
  }
  return i;
}
