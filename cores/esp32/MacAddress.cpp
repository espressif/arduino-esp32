#include <MacAddress.h>
#include <stdio.h>

//Default constructor, blank mac address.
MacAddress::MacAddress() {
    _mac.val = 0;
}

MacAddress::MacAddress(uint64_t mac) {
    _mac.val = mac;
}

MacAddress::MacAddress(const uint8_t *macbytearray) {
    memcpy(_mac.bytes, macbytearray, sizeof(_mac.bytes));
}

//Parse user entered string into MAC address
bool MacAddress::fromCStr(const char *buf) {
  char cs[18];
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
bool MacAddress::fromString(const String &macstr) {
    return fromCStr(macstr.c_str());
}

//Copy MAC into 6 byte array
void MacAddress::toBytes(uint8_t *buf) {
    memcpy(buf, _mac.bytes, sizeof(_mac.bytes));
}

//Print MAC address into a C string.
//MAC: Buffer must be at least 18 chars
int MacAddress::toCStr(char *buf) {
    return sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
        _mac.bytes[0], _mac.bytes[1], _mac.bytes[2],
        _mac.bytes[3], _mac.bytes[4], _mac.bytes[5]);
}

String MacAddress::toString() const {
    char buf[18];
    sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
        _mac.bytes[0], _mac.bytes[1], _mac.bytes[2],
        _mac.bytes[3], _mac.bytes[4], _mac.bytes[5]);
    return String(buf);
}

uint64_t MacAddress::Value() {
    return _mac.val;
}

//Implicit conversion object to number [same as .Value()]
MacAddress::operator uint64_t() const
{
    return _mac.val;
}

//Overloaded copy operators to allow initialisation of MacAddress objects from other types
MacAddress& MacAddress::operator=(const uint8_t *mac)
{
    memcpy(_mac.bytes, mac, sizeof(_mac.bytes));
    return *this;
}

MacAddress& MacAddress::operator=(uint64_t macval)
{
    _mac.val = macval;
    return *this;
}

//Compare class to byte array
bool MacAddress::operator==(const uint8_t *mac) const
{
    return !memcmp(_mac.bytes, mac, sizeof(_mac.bytes));
}

//Allow comparing value of two classes
bool MacAddress::operator==(const MacAddress& mac2) const
{
    return _mac.val == mac2._mac.val;
}
