#pragma once
#include <AsyncUDP.h>

#define DNS_QR_QUERY 0
#define DNS_QR_RESPONSE 1
#define DNS_OPCODE_QUERY 0
#define DNS_DEFAULT_TTL 60        // Default Time To Live : time interval in seconds that the resource record should be cached before being discarded
#define DNS_HEADER_SIZE 12 
#define DNS_OFFSET_DOMAIN_NAME DNS_HEADER_SIZE // Offset in bytes to reach the domain name labels in the DNS message
#define DNS_DEFAULT_PORT  53

enum class DNSReplyCode:uint16_t
{
  NoError   = 0,
  FormError = 1,
  ServerFailure     = 2,
  NonExistentDomain = 3,
  NotImplemented    = 4,
  Refused   = 5,
  YXDomain  = 6,
  YXRRSet   = 7,
  NXRRSet   = 8
};

enum DNSType
{
  DNS_TYPE_A      = 1,  // Host Address
  DNS_TYPE_AAAA   = 28, // IPv6 Address
  DNS_TYPE_SOA    = 6,  // Start Of a zone of Authority
  DNS_TYPE_PTR    = 12, // Domain name PoinTeR
  DNS_TYPE_DNAME  = 39  // Delegation Name
} ; 

enum DNSClass
{
  DNS_CLASS_IN = 1, // INternet
  DNS_CLASS_CH = 3  // CHaos
} ; 

enum DNSRDLength
{
  DNS_RDLENGTH_IPV4 = 4 // 4 bytes for an IPv4 address 
} ; 

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
  uint16_t ANCount;          // number of ANswer entries
  uint16_t NSCount;          // number of authority entries
  uint16_t ARCount;          // number of Additional Resource entries
};

struct DNSQuestion
{
  const uint8_t* QName;
  uint16_t  QNameLength ; 
  uint16_t  QType ; 
  uint16_t  QClass ; 
} ; 

class DNSServer
{
  public:
    /**
     * @brief Construct a new DNSServer object
     * by default server is configured to run in "Captive-portal" mode
     * it must be started with start() call to establish a listening socket
     * 
     */
    DNSServer();

    /**
     * @brief Construct a new DNSServer object
     * builds DNS server with default parameters
     * @param domainName - domain name to serve
     */
    DNSServer(const String &domainName);
    ~DNSServer(){};                   // default d-tor

    // Copy semantics not implemented (won't run on same UDP port anyway)
    DNSServer(const DNSServer&) = delete;
    DNSServer& operator=(const DNSServer&) = delete;


    /**
     * @brief stub, left for compatibility with an old version
     * does nothing actually
     * 
     */
    void processNextRequest(){};

    /**
     * @brief Set the Error Reply Code for all req's not matching predifined domain
     * 
     * @param replyCode 
     */
    void setErrorReplyCode(const DNSReplyCode &replyCode);

    /**
     * @brief set TTL for successfull replies
     * 
     * @param ttl in seconds
     */
    void setTTL(const uint32_t &ttl);

    /**
     * @brief (re)Starts a server with current configuration or with default parameters
     * if it's the first call.
     * Defaults are:
     * port: 53
     * domainName: any
     * ip: WiFi AP's IP address
     * 
     * @return true on success
     * @return false if IP or socket error
     */
    bool start();

    /**
     * @brief (re)Starts a server with provided configuration
     * 
     * @return true on success
     * @return false if IP or socket error
     */
    bool start(uint16_t port,
              const String &domainName,
              const IPAddress &resolvedIP);

    /**
     * @brief stops the server and close UDP socket
     * 
     */
    void stop();

    /**
     * @brief returns true if DNS server runs in captive-portal mode
     * i.e. all requests are served with AP's ip address
     * 
     * @return true if catch-all mode active
     * @return false otherwise
     */
    inline bool isCaptive() const { return _domainName.isEmpty(); };

    /**
     * @brief returns 'true' if server is up and UDP socket is listening for UDP req's
     * 
     * @return true if server is up
     * @return false otherwise
     */
    inline bool isUp() { return _udp.connected(); };

  private:
    AsyncUDP _udp;
    uint16_t _port;
    uint32_t _ttl;
    DNSReplyCode _errorReplyCode;
    String _domainName;
    IPAddress _resolvedIP;


    void downcaseAndRemoveWwwPrefix(String &domainName);

    /**
     * @brief Get the Domain Name Without Www Prefix object
     * scan labels in DNS packet and build a string of a domain name
     * truncate any www. label if found 
     * @param start a pointer to the start of labels records in DNS packet
     * @param len labels length
     * @return String 
     */
    String getDomainNameWithoutWwwPrefix(const unsigned char* start, size_t len);
    inline bool requestIncludesOnlyOneQuestion(DNSHeader& dnsHeader);
    void replyWithIP(AsyncUDPPacket& req, DNSHeader& dnsHeader, DNSQuestion& dnsQuestion);
    inline void replyWithCustomCode(AsyncUDPPacket& req, DNSHeader& dnsHeader);
    void _handleUDP(AsyncUDPPacket& pkt);
};
