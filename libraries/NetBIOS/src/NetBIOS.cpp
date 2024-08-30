#include "NetBIOS.h"
#include <functional>
extern "C" {
#include <lwip/netif.h>
};  // extern "C"

#define NBNS_PORT             137
#define NBNS_MAX_HOSTNAME_LEN 32

typedef struct {
  uint16_t id;
  uint8_t flags1;
  uint8_t flags2;
  uint16_t qcount;
  uint16_t acount;  // codespell:ignore acount
  uint16_t nscount;
  uint16_t adcount;
  uint8_t name_len;
  char name[NBNS_MAX_HOSTNAME_LEN + 1];
  uint16_t type;
  uint16_t clas;
} __attribute__((packed)) nbns_question_t;

typedef struct {
  uint16_t id;
  uint8_t flags1;
  uint8_t flags2;
  uint16_t qcount;
  uint16_t acount;  // codespell:ignore acount
  uint16_t nscount;
  uint16_t adcount;
  uint8_t name_len;
  char name[NBNS_MAX_HOSTNAME_LEN + 1];
  uint16_t type;
  uint16_t clas;
  uint32_t ttl;
  uint16_t data_len;
  uint16_t flags;
  uint32_t addr;
} __attribute__((packed)) nbns_answer_t;

static void _getnbname(const char *nbname, char *name, uint8_t maxlen) {
  uint8_t b;
  uint8_t c = 0;

  while ((*nbname) && (c < maxlen)) {
    b = (*nbname++ - 'A') << 4;
    c++;
    if (*nbname) {
      b |= *nbname++ - 'A';
      c++;
    }
    if (!b || b == ' ') {
      break;
    }
    *name++ = b;
  }
  *name = 0;
}

static void append_16(void *dst, uint16_t value) {
  uint8_t *d = (uint8_t *)dst;
  *d++ = (value >> 8) & 0xFF;
  *d++ = value & 0xFF;
}

static void append_32(void *dst, uint32_t value) {
  uint8_t *d = (uint8_t *)dst;
  *d++ = (value >> 24) & 0xFF;
  *d++ = (value >> 16) & 0xFF;
  *d++ = (value >> 8) & 0xFF;
  *d++ = value & 0xFF;
}

void NetBIOS::_onPacket(AsyncUDPPacket &packet) {
  if (packet.length() >= sizeof(nbns_question_t)) {
    nbns_question_t *question = (nbns_question_t *)packet.data();
    if (0 == (question->flags1 & 0x80)) {
      char name[NBNS_MAX_HOSTNAME_LEN + 1];
      _getnbname(&question->name[0], (char *)&name, question->name_len);
      if (_name.equals(name)) {
        nbns_answer_t nbnsa;
        nbnsa.id = question->id;
        nbnsa.flags1 = 0x85;
        nbnsa.flags2 = 0;
        append_16((void *)&nbnsa.qcount, 0);
        append_16((void *)&nbnsa.acount, 1);  // codespell:ignore acount
        append_16((void *)&nbnsa.nscount, 0);
        append_16((void *)&nbnsa.adcount, 0);
        nbnsa.name_len = question->name_len;
        memcpy(&nbnsa.name[0], &question->name[0], question->name_len + 1);
        append_16((void *)&nbnsa.type, 0x20);
        append_16((void *)&nbnsa.clas, 1);
        append_32((void *)&nbnsa.ttl, 300000);
        append_16((void *)&nbnsa.data_len, 6);
        append_16((void *)&nbnsa.flags, 0);
        nbnsa.addr = packet.localIP();  // By default, should be overridden below
        // Iterate over all netifs, see if the incoming address matches one of the netmaskes networks
        for (auto netif = netif_list; netif; netif = netif->next) {
          auto maskedip = ip4_addr_get_u32(netif_ip4_addr(netif)) & ip4_addr_get_u32(netif_ip4_netmask(netif));
          auto maskedin = ((uint32_t)packet.localIP()) & ip4_addr_get_u32(netif_ip4_netmask(netif));
          if (maskedip == maskedin) {
            nbnsa.addr = ip4_addr_get_u32(netif_ip4_addr(netif));
            break;
          }
        }
        _udp.writeTo((uint8_t *)&nbnsa, sizeof(nbnsa), packet.remoteIP(), NBNS_PORT);
      }
    }
  }
}

NetBIOS::NetBIOS() {}

NetBIOS::~NetBIOS() {
  end();
}

bool NetBIOS::begin(const char *name) {
  _name = name;
  _name.toUpperCase();

  if (_udp.connected()) {
    return true;
  }

  _udp.onPacket(
    [](void *arg, AsyncUDPPacket &packet) {
      ((NetBIOS *)(arg))->_onPacket(packet);
    },
    this
  );
  return _udp.listen(NBNS_PORT);
}

void NetBIOS::end() {
  if (_udp.connected()) {
    _udp.close();
  }
}

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_NETBIOS)
NetBIOS NBNS;
#endif

// EOF
