#ifndef DNSServer_h
#define DNSServer_h
#include <WiFiUdp.h>

#define DNS_QR_QUERY 0
#define DNS_QR_RESPONSE 1
#define DNS_OPCODE_QUERY 0

enum class DNSReplyCode
{
  NoError = 0,
  FormError = 1,
  ServerFailure = 2,
  NonExistentDomain = 3,
  NotImplemented = 4,
  Refused = 5,
  YXDomain = 6,
  YXRRSet = 7,
  NXRRSet = 8
};

struct DNSHeader
{
  uint16_t ID;               // identification number
  union {
    struct {
      uint16_t RD : 1;      // recursion desired
      uint16_t TC : 1;      // truncated message
      uint16_t AA : 1;      // authoritive answer
      uint16_t OPCode : 4;  // message_type
      uint16_t QR : 1;      // query/response flag
      uint16_t RCode : 4;   // response code
      uint16_t Z : 3;       // its z! reserved
      uint16_t RA : 1;      // recursion available
    };
    uint16_t Flags;
  };
  uint16_t QDCount;          // number of question entries
  uint16_t ANCount;          // number of answer entries
  uint16_t NSCount;          // number of authority entries
  uint16_t ARCount;          // number of resource entries
};

class DNSServer
{
  public:
    DNSServer();
    void processNextRequest();
    void setErrorReplyCode(const DNSReplyCode &replyCode);
    void setTTL(const uint32_t &ttl);

    // Returns true if successful, false if there are no sockets available
    bool start(const uint16_t &port,
              const String &domainName,
              const IPAddress &resolvedIP);
    // stops the DNS server
    void stop();

  private:
    WiFiUDP _udp;
    uint16_t _port;
    String _domainName;
    unsigned char _resolvedIP[4];
    int _currentPacketSize;
    unsigned char* _buffer;
    DNSHeader* _dnsHeader;
    uint32_t _ttl;
    DNSReplyCode _errorReplyCode;

    void downcaseAndRemoveWwwPrefix(String &domainName);
    String getDomainNameWithoutWwwPrefix();
    bool requestIncludesOnlyOneQuestion();
    void replyWithIP();
    void replyWithCustomCode();
};
#endif