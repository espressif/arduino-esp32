#include "DNSServer.h"
#include <lwip/def.h>
#include <Arduino.h>
#include <WiFi.h>


// #define DEBUG_ESP_DNS
#ifdef DEBUG_ESP_PORT
#define DEBUG_OUTPUT DEBUG_ESP_PORT
#else
#define DEBUG_OUTPUT Serial
#endif

#define DNS_MIN_REQ_LEN 17  // minimal size for DNS request asking ROOT = DNS_HEADER_SIZE + 1 null byte for Name + 4 bytes type/class

DNSServer::DNSServer()
  : _port(DNS_DEFAULT_PORT), _ttl(htonl(DNS_DEFAULT_TTL)), _errorReplyCode(DNSReplyCode::NonExistentDomain) {}

DNSServer::DNSServer(const String& domainName)
  : _port(DNS_DEFAULT_PORT), _ttl(htonl(DNS_DEFAULT_TTL)), _errorReplyCode(DNSReplyCode::NonExistentDomain), _domainName(domainName){};


bool DNSServer::start() {
  if (_resolvedIP.operator uint32_t() == 0) {  // no address is set, try to obtain AP interface's IP
#if SOC_WIFI_SUPPORTED
    if (WiFi.getMode() & WIFI_AP) {
      _resolvedIP = WiFi.softAPIP();
      return true;
    }
#endif
    return false;  // won't run if WiFi is not in AP mode
  }

  _udp.close();
  _udp.onPacket([this](AsyncUDPPacket& pkt) {
    this->_handleUDP(pkt);
  });
  return _udp.listen(_port);
}

bool DNSServer::start(uint16_t port, const String& domainName, const IPAddress& resolvedIP) {
  _port = port;
  if (domainName != "*") {
    _domainName = domainName;
    downcaseAndRemoveWwwPrefix(_domainName);
  } else
    _domainName.clear();

  _resolvedIP = resolvedIP;
  _udp.close();
  _udp.onPacket([this](AsyncUDPPacket& pkt) {
    this->_handleUDP(pkt);
  });
  return _udp.listen(_port);
}

void DNSServer::setErrorReplyCode(const DNSReplyCode& replyCode) {
  _errorReplyCode = replyCode;
}

void DNSServer::setTTL(const uint32_t& ttl) {
  _ttl = htonl(ttl);
}

void DNSServer::stop() {
  _udp.close();
}

void DNSServer::downcaseAndRemoveWwwPrefix(String& domainName) {
  domainName.toLowerCase();
  domainName.replace("www.", "");
}

void DNSServer::_handleUDP(AsyncUDPPacket& pkt) {
  if (pkt.length() < DNS_MIN_REQ_LEN) return;  // truncated packet or not a DNS req

  // get DNS header (beginning of message)
  DNSHeader dnsHeader;
  DNSQuestion dnsQuestion;
  memcpy(&dnsHeader, pkt.data(), DNS_HEADER_SIZE);
  if (dnsHeader.QR != DNS_QR_QUERY) return;  // ignore non-query messages

  if (requestIncludesOnlyOneQuestion(dnsHeader)) {
    /*
      // The QName has a variable length, maximum 255 bytes and is comprised of multiple labels.
      // Each label contains a byte to describe its length and the label itself. The list of
      // labels terminates with a zero-valued byte. In "github.com", we have two labels "github" & "com"
*/
    const char* enoflbls = strchr(reinterpret_cast<const char*>(pkt.data()) + DNS_HEADER_SIZE, 0);  // find end_of_label marker
    ++enoflbls;                                                                                     // advance after null terminator
    dnsQuestion.QName = pkt.data() + DNS_HEADER_SIZE;                                               // we can reference labels from the request
    dnsQuestion.QNameLength = enoflbls - (char*)pkt.data() - DNS_HEADER_SIZE;
    /*
        check if we aint going out of pkt bounds
        proper dns req should have label terminator at least 4 bytes before end of packet
      */
    if (dnsQuestion.QNameLength > pkt.length() - DNS_HEADER_SIZE - sizeof(dnsQuestion.QType) - sizeof(dnsQuestion.QClass)) return;  // malformed packet

    // Copy the QType and QClass
    memcpy(&dnsQuestion.QType, enoflbls, sizeof(dnsQuestion.QType));
    memcpy(&dnsQuestion.QClass, enoflbls + sizeof(dnsQuestion.QType), sizeof(dnsQuestion.QClass));
  }

  // will reply with IP only to "*" or if domain matches without www. subdomain
  if (dnsHeader.OPCode == DNS_OPCODE_QUERY && requestIncludesOnlyOneQuestion(dnsHeader) && (_domainName.isEmpty() || getDomainNameWithoutWwwPrefix(static_cast<const unsigned char*>(dnsQuestion.QName), dnsQuestion.QNameLength) == _domainName)) {
    replyWithIP(pkt, dnsHeader, dnsQuestion);
    return;
  }

  // otherwise reply with custom code
  replyWithCustomCode(pkt, dnsHeader);
}

bool DNSServer::requestIncludesOnlyOneQuestion(DNSHeader& dnsHeader) {
  return ntohs(dnsHeader.QDCount) == 1 && dnsHeader.ANCount == 0 && dnsHeader.NSCount == 0 && dnsHeader.ARCount == 0;
}


String DNSServer::getDomainNameWithoutWwwPrefix(const unsigned char* start, size_t len) {
  String parsedDomainName(start, --len);  // exclude trailing null byte from labels length, String constructor will add it anyway

  int pos = 0;
  while (pos < len) {
    parsedDomainName.setCharAt(pos, 0x2e);  // replace label len byte with dot char "."
    pos += *(start + pos);
    ++pos;
  }
  parsedDomainName.remove(0, 1);  // remove first "." char
  downcaseAndRemoveWwwPrefix(parsedDomainName);
  return parsedDomainName;
}

void DNSServer::replyWithIP(AsyncUDPPacket& req, DNSHeader& dnsHeader, DNSQuestion& dnsQuestion) {
  AsyncUDPMessage rpl;

  // Change the type of message to a response and set the number of answers equal to
  // the number of questions in the header
  dnsHeader.QR = DNS_QR_RESPONSE;
  dnsHeader.ANCount = dnsHeader.QDCount;
  rpl.write((unsigned char*)&dnsHeader, DNS_HEADER_SIZE);

  // Write the question
  rpl.write(dnsQuestion.QName, dnsQuestion.QNameLength);
  rpl.write((uint8_t*)&dnsQuestion.QType, 2);
  rpl.write((uint8_t*)&dnsQuestion.QClass, 2);

  // Write the answer
  // Use DNS name compression : instead of repeating the name in this RNAME occurrence,
  // set the two MSB of the byte corresponding normally to the length to 1. The following
  // 14 bits must be used to specify the offset of the domain name in the message
  // (<255 here so the first byte has the 6 LSB at 0)
  rpl.write((uint8_t)0xC0);
  rpl.write((uint8_t)DNS_OFFSET_DOMAIN_NAME);

  // DNS type A : host address, DNS class IN for INternet, returning an IPv4 address
  uint16_t answerType = htons(DNS_TYPE_A), answerClass = htons(DNS_CLASS_IN), answerIPv4 = htons(DNS_RDLENGTH_IPV4);
  rpl.write((unsigned char*)&answerType, 2);
  rpl.write((unsigned char*)&answerClass, 2);
  rpl.write((unsigned char*)&_ttl, 4);  // DNS Time To Live
  rpl.write((unsigned char*)&answerIPv4, 2);
  uint32_t ip = _resolvedIP;
  rpl.write(reinterpret_cast<uint8_t*>(&ip), sizeof(uint32_t));  // The IPv4 address to return

  _udp.sendTo(rpl, req.remoteIP(), req.remotePort());

#ifdef DEBUG_ESP_DNS
  DEBUG_OUTPUT.printf("DNS responds: %s for %s\n",
                      _resolvedIP.toString().c_str(), getDomainNameWithoutWwwPrefix(static_cast<const unsigned char*>(dnsQuestion.QName), dnsQuestion.QNameLength).c_str());
#endif
}

void DNSServer::replyWithCustomCode(AsyncUDPPacket& req, DNSHeader& dnsHeader) {
  dnsHeader.QR = DNS_QR_RESPONSE;
  dnsHeader.RCode = static_cast<uint16_t>(_errorReplyCode);
  dnsHeader.QDCount = 0;

  AsyncUDPMessage rpl(sizeof(DNSHeader));
  rpl.write(reinterpret_cast<const uint8_t*>(&dnsHeader), sizeof(DNSHeader));
  _udp.sendTo(rpl, req.remoteIP(), req.remotePort());
}
