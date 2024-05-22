#include "Arduino.h"
#include "AsyncUDP.h"

extern "C" {
#include "lwip/opt.h"
#include "lwip/inet.h"
#include "lwip/udp.h"
#include "lwip/igmp.h"
#include "lwip/ip_addr.h"
#include "lwip/mld6.h"
#include "lwip/prot/ethernet.h"
#include <esp_err.h>
#include <esp_wifi.h>
}

#include "lwip/priv/tcpip_priv.h"

static const char *netif_ifkeys[TCPIP_ADAPTER_IF_MAX] = {"WIFI_STA_DEF", "WIFI_AP_DEF", "ETH_DEF", "PPP_DEF"};

static esp_err_t tcpip_adapter_get_netif(tcpip_adapter_if_t tcpip_if, void **netif) {
  *netif = NULL;
  if (tcpip_if < TCPIP_ADAPTER_IF_MAX) {
    esp_netif_t *esp_netif = esp_netif_get_handle_from_ifkey(netif_ifkeys[tcpip_if]);
    if (esp_netif == NULL) {
      return ESP_FAIL;
    }
    int netif_index = esp_netif_get_netif_impl_index(esp_netif);
    if (netif_index < 0) {
      return ESP_FAIL;
    }
    *netif = (void *)netif_get_by_index(netif_index);
  } else {
    *netif = netif_default;
  }
  return (*netif != NULL) ? ESP_OK : ESP_FAIL;
}

typedef struct {
  struct tcpip_api_call_data call;
  udp_pcb *pcb;
  const ip_addr_t *addr;
  uint16_t port;
  struct pbuf *pb;
  struct netif *netif;
  err_t err;
} udp_api_call_t;

static err_t _udp_connect_api(struct tcpip_api_call_data *api_call_msg) {
  udp_api_call_t *msg = (udp_api_call_t *)api_call_msg;
  msg->err = udp_connect(msg->pcb, msg->addr, msg->port);
  return msg->err;
}

static err_t _udp_connect(struct udp_pcb *pcb, const ip_addr_t *addr, u16_t port) {
  udp_api_call_t msg;
  msg.pcb = pcb;
  msg.addr = addr;
  msg.port = port;
  tcpip_api_call(_udp_connect_api, (struct tcpip_api_call_data *)&msg);
  return msg.err;
}

static err_t _udp_disconnect_api(struct tcpip_api_call_data *api_call_msg) {
  udp_api_call_t *msg = (udp_api_call_t *)api_call_msg;
  msg->err = 0;
  udp_disconnect(msg->pcb);
  return msg->err;
}

static void _udp_disconnect(struct udp_pcb *pcb) {
  udp_api_call_t msg;
  msg.pcb = pcb;
  tcpip_api_call(_udp_disconnect_api, (struct tcpip_api_call_data *)&msg);
}

static err_t _udp_remove_api(struct tcpip_api_call_data *api_call_msg) {
  udp_api_call_t *msg = (udp_api_call_t *)api_call_msg;
  msg->err = 0;
  udp_remove(msg->pcb);
  return msg->err;
}

static void _udp_remove(struct udp_pcb *pcb) {
  udp_api_call_t msg;
  msg.pcb = pcb;
  tcpip_api_call(_udp_remove_api, (struct tcpip_api_call_data *)&msg);
}

static err_t _udp_bind_api(struct tcpip_api_call_data *api_call_msg) {
  udp_api_call_t *msg = (udp_api_call_t *)api_call_msg;
  msg->err = udp_bind(msg->pcb, msg->addr, msg->port);
  return msg->err;
}

static err_t _udp_bind(struct udp_pcb *pcb, const ip_addr_t *addr, u16_t port) {
  udp_api_call_t msg;
  msg.pcb = pcb;
  msg.addr = addr;
  msg.port = port;
  tcpip_api_call(_udp_bind_api, (struct tcpip_api_call_data *)&msg);
  return msg.err;
}

static err_t _udp_sendto_api(struct tcpip_api_call_data *api_call_msg) {
  udp_api_call_t *msg = (udp_api_call_t *)api_call_msg;
  msg->err = udp_sendto(msg->pcb, msg->pb, msg->addr, msg->port);
  return msg->err;
}

static err_t _udp_sendto(struct udp_pcb *pcb, struct pbuf *pb, const ip_addr_t *addr, u16_t port) {
  udp_api_call_t msg;
  msg.pcb = pcb;
  msg.addr = addr;
  msg.port = port;
  msg.pb = pb;
  tcpip_api_call(_udp_sendto_api, (struct tcpip_api_call_data *)&msg);
  return msg.err;
}

static err_t _udp_sendto_if_api(struct tcpip_api_call_data *api_call_msg) {
  udp_api_call_t *msg = (udp_api_call_t *)api_call_msg;
  msg->err = udp_sendto_if(msg->pcb, msg->pb, msg->addr, msg->port, msg->netif);
  return msg->err;
}

static err_t _udp_sendto_if(struct udp_pcb *pcb, struct pbuf *pb, const ip_addr_t *addr, u16_t port, struct netif *netif) {
  udp_api_call_t msg;
  msg.pcb = pcb;
  msg.addr = addr;
  msg.port = port;
  msg.pb = pb;
  msg.netif = netif;
  tcpip_api_call(_udp_sendto_if_api, (struct tcpip_api_call_data *)&msg);
  return msg.err;
}

typedef struct {
  void *arg;
  udp_pcb *pcb;
  pbuf *pb;
  const ip_addr_t *addr;
  uint16_t port;
  struct netif *netif;
} lwip_event_packet_t;

static QueueHandle_t _udp_queue;
static volatile TaskHandle_t _udp_task_handle = NULL;

static void _udp_task(void *pvParameters) {
  lwip_event_packet_t *e = NULL;
  for (;;) {
    if (xQueueReceive(_udp_queue, &e, portMAX_DELAY) == pdTRUE) {
      if (!e->pb) {
        free((void *)(e));
        continue;
      }
      AsyncUDP::_s_recv(e->arg, e->pcb, e->pb, e->addr, e->port, e->netif);
      free((void *)(e));
    }
  }
  _udp_task_handle = NULL;
  vTaskDelete(NULL);
}

static bool _udp_task_start() {
  if (!_udp_queue) {
    _udp_queue = xQueueCreate(32, sizeof(lwip_event_packet_t *));
    if (!_udp_queue) {
      return false;
    }
  }
  if (!_udp_task_handle) {
    xTaskCreateUniversal(
      _udp_task, "async_udp", 4096, NULL, CONFIG_ARDUINO_UDP_TASK_PRIORITY, (TaskHandle_t *)&_udp_task_handle, CONFIG_ARDUINO_UDP_RUNNING_CORE
    );
    if (!_udp_task_handle) {
      return false;
    }
  }
  return true;
}

static bool _udp_task_post(void *arg, udp_pcb *pcb, pbuf *pb, const ip_addr_t *addr, uint16_t port, struct netif *netif) {
  if (!_udp_task_handle || !_udp_queue) {
    return false;
  }
  lwip_event_packet_t *e = (lwip_event_packet_t *)malloc(sizeof(lwip_event_packet_t));
  if (!e) {
    return false;
  }
  e->arg = arg;
  e->pcb = pcb;
  e->pb = pb;
  e->addr = addr;
  e->port = port;
  e->netif = netif;
  if (xQueueSend(_udp_queue, &e, portMAX_DELAY) != pdPASS) {
    free((void *)(e));
    return false;
  }
  return true;
}

static void _udp_recv(void *arg, udp_pcb *pcb, pbuf *pb, const ip_addr_t *addr, uint16_t port) {
  while (pb != NULL) {
    pbuf *this_pb = pb;
    pb = pb->next;
    this_pb->next = NULL;
    if (!_udp_task_post(arg, pcb, this_pb, addr, port, ip_current_input_netif())) {
      pbuf_free(this_pb);
    }
  }
}
/*
static bool _udp_task_stop(){
    if(!_udp_task_post(NULL, NULL, NULL, NULL, 0, NULL)){
        return false;
    }
    while(_udp_task_handle){
        vTaskDelay(10);
    }

    lwip_event_packet_t * e;
    while (xQueueReceive(_udp_queue, &e, 0) == pdTRUE) {
        if(e->pb){
            pbuf_free(e->pb);
        }
        free((void*)(e));
    }
    vQueueDelete(_udp_queue);
    _udp_queue = NULL;
}
*/

#define UDP_MUTEX_LOCK()    //xSemaphoreTake(_lock, portMAX_DELAY)
#define UDP_MUTEX_UNLOCK()  //xSemaphoreGive(_lock)

AsyncUDPMessage::AsyncUDPMessage(size_t size) {
  _index = 0;
  if (size > CONFIG_TCP_MSS) {
    size = CONFIG_TCP_MSS;
  }
  _size = size;
  _buffer = (uint8_t *)malloc(size);
}

AsyncUDPMessage::~AsyncUDPMessage() {
  if (_buffer) {
    free(_buffer);
  }
}

size_t AsyncUDPMessage::write(const uint8_t *data, size_t len) {
  if (_buffer == NULL) {
    return 0;
  }
  size_t s = space();
  if (len > s) {
    len = s;
  }
  memcpy(_buffer + _index, data, len);
  _index += len;
  return len;
}

size_t AsyncUDPMessage::write(uint8_t data) {
  return write(&data, 1);
}

size_t AsyncUDPMessage::space() {
  if (_buffer == NULL) {
    return 0;
  }
  return _size - _index;
}

uint8_t *AsyncUDPMessage::data() {
  return _buffer;
}

size_t AsyncUDPMessage::length() {
  return _index;
}

void AsyncUDPMessage::flush() {
  _index = 0;
}

AsyncUDPPacket::AsyncUDPPacket(AsyncUDPPacket &packet) {
  _udp = packet._udp;
  _pb = packet._pb;
  _if = packet._if;
  _data = packet._data;
  _len = packet._len;
  _index = 0;

  memcpy(&_remoteIp, &packet._remoteIp, sizeof(ip_addr_t));
  memcpy(&_localIp, &packet._localIp, sizeof(ip_addr_t));
  _localPort = packet._localPort;
  _remotePort = packet._remotePort;
  memcpy(_remoteMac, packet._remoteMac, 6);

  pbuf_ref(_pb);
}

AsyncUDPPacket::AsyncUDPPacket(AsyncUDP *udp, pbuf *pb, const ip_addr_t *raddr, uint16_t rport, struct netif *ntif) {
  _udp = udp;
  _pb = pb;
  _if = TCPIP_ADAPTER_IF_MAX;
  _data = (uint8_t *)(pb->payload);
  _len = pb->len;
  _index = 0;

  pbuf_ref(_pb);

  //memcpy(&_remoteIp, raddr, sizeof(ip_addr_t));
  _remoteIp.type = raddr->type;
  _localIp.type = _remoteIp.type;

  eth_hdr *eth = NULL;
  udp_hdr *udphdr = (udp_hdr *)(_data - UDP_HLEN);
  _localPort = ntohs(udphdr->dest);
  _remotePort = ntohs(udphdr->src);

  if (_remoteIp.type == IPADDR_TYPE_V4) {
    eth = (eth_hdr *)(_data - UDP_HLEN - IP_HLEN - SIZEOF_ETH_HDR);
    struct ip_hdr *iphdr = (struct ip_hdr *)(_data - UDP_HLEN - IP_HLEN);
    _localIp.u_addr.ip4.addr = iphdr->dest.addr;
    _remoteIp.u_addr.ip4.addr = iphdr->src.addr;
  } else {
    eth = (eth_hdr *)(_data - UDP_HLEN - IP6_HLEN - SIZEOF_ETH_HDR);
    struct ip6_hdr *ip6hdr = (struct ip6_hdr *)(_data - UDP_HLEN - IP6_HLEN);
    memcpy(&_localIp.u_addr.ip6.addr, (uint8_t *)ip6hdr->dest.addr, 16);
    memcpy(&_remoteIp.u_addr.ip6.addr, (uint8_t *)ip6hdr->src.addr, 16);
  }
  memcpy(_remoteMac, eth->src.addr, 6);

  struct netif *netif = NULL;
  void *nif = NULL;
  int i;
  for (i = 0; i < TCPIP_ADAPTER_IF_MAX; i++) {
    tcpip_adapter_get_netif((tcpip_adapter_if_t)i, &nif);
    netif = (struct netif *)nif;
    if (netif && netif == ntif) {
      _if = (tcpip_adapter_if_t)i;
      break;
    }
  }
}

AsyncUDPPacket::~AsyncUDPPacket() {
  pbuf_free(_pb);
}

uint8_t *AsyncUDPPacket::data() {
  return _data;
}

size_t AsyncUDPPacket::length() {
  return _len;
}

int AsyncUDPPacket::available() {
  return _len - _index;
}

size_t AsyncUDPPacket::read(uint8_t *data, size_t len) {
  size_t i;
  size_t a = _len - _index;
  if (len > a) {
    len = a;
  }
  for (i = 0; i < len; i++) {
    data[i] = read();
  }
  return len;
}

int AsyncUDPPacket::read() {
  if (_index < _len) {
    return _data[_index++];
  }
  return -1;
}

int AsyncUDPPacket::peek() {
  if (_index < _len) {
    return _data[_index];
  }
  return -1;
}

void AsyncUDPPacket::flush() {
  _index = _len;
}

tcpip_adapter_if_t AsyncUDPPacket::interface() {
  return _if;
}

IPAddress AsyncUDPPacket::localIP() {
  if (_localIp.type != IPADDR_TYPE_V4) {
    return IPAddress();
  }
  return IPAddress(_localIp.u_addr.ip4.addr);
}

IPAddress AsyncUDPPacket::localIPv6() {
  if (_localIp.type != IPADDR_TYPE_V6) {
    return IPAddress(IPv6);
  }
  return IPAddress(IPv6, (const uint8_t *)_localIp.u_addr.ip6.addr, _localIp.u_addr.ip6.zone);
}

uint16_t AsyncUDPPacket::localPort() {
  return _localPort;
}

IPAddress AsyncUDPPacket::remoteIP() {
  if (_remoteIp.type != IPADDR_TYPE_V4) {
    return IPAddress();
  }
  return IPAddress(_remoteIp.u_addr.ip4.addr);
}

IPAddress AsyncUDPPacket::remoteIPv6() {
  if (_remoteIp.type != IPADDR_TYPE_V6) {
    return IPAddress(IPv6);
  }
  return IPAddress(IPv6, (const uint8_t *)_remoteIp.u_addr.ip6.addr, _remoteIp.u_addr.ip6.zone);
}

uint16_t AsyncUDPPacket::remotePort() {
  return _remotePort;
}

void AsyncUDPPacket::remoteMac(uint8_t *mac) {
  memcpy(mac, _remoteMac, 6);
}

bool AsyncUDPPacket::isIPv6() {
  return _localIp.type == IPADDR_TYPE_V6;
}

bool AsyncUDPPacket::isBroadcast() {
  if (_localIp.type == IPADDR_TYPE_V6) {
    return false;
  }
  uint32_t ip = _localIp.u_addr.ip4.addr;
  return ip == 0xFFFFFFFF || ip == 0 || (ip & 0xFF000000) == 0xFF000000;
}

bool AsyncUDPPacket::isMulticast() {
  return ip_addr_ismulticast(&(_localIp));
}

size_t AsyncUDPPacket::write(const uint8_t *data, size_t len) {
  if (!data) {
    return 0;
  }
  return _udp->writeTo(data, len, &_remoteIp, _remotePort, _if);
}

size_t AsyncUDPPacket::write(uint8_t data) {
  return write(&data, 1);
}

size_t AsyncUDPPacket::send(AsyncUDPMessage &message) {
  return write(message.data(), message.length());
}

bool AsyncUDP::_init() {
  if (_pcb) {
    return true;
  }
  _pcb = udp_new();
  if (!_pcb) {
    return false;
  }
  //_lock = xSemaphoreCreateMutex();
  udp_recv(_pcb, &_udp_recv, (void *)this);
  return true;
}

AsyncUDP::AsyncUDP() {
  _pcb = NULL;
  _connected = false;
  _lastErr = ERR_OK;
  _handler = NULL;
}

AsyncUDP::~AsyncUDP() {
  close();
  UDP_MUTEX_LOCK();
  udp_recv(_pcb, NULL, NULL);
  _udp_remove(_pcb);
  _pcb = NULL;
  UDP_MUTEX_UNLOCK();
  //vSemaphoreDelete(_lock);
}

void AsyncUDP::close() {
  UDP_MUTEX_LOCK();
  if (_pcb != NULL) {
    if (_connected) {
      _udp_disconnect(_pcb);
    }
    _connected = false;
    //todo: unjoin multicast group
  }
  UDP_MUTEX_UNLOCK();
}

bool AsyncUDP::connect(const ip_addr_t *addr, uint16_t port) {
  if (!_udp_task_start()) {
    log_e("failed to start task");
    return false;
  }
  if (!_init()) {
    return false;
  }
  close();
  UDP_MUTEX_LOCK();
  _lastErr = _udp_connect(_pcb, addr, port);
  if (_lastErr != ERR_OK) {
    UDP_MUTEX_UNLOCK();
    return false;
  }
  _connected = true;
  UDP_MUTEX_UNLOCK();
  return true;
}

bool AsyncUDP::listen(const ip_addr_t *addr, uint16_t port) {
  if (!_udp_task_start()) {
    log_e("failed to start task");
    return false;
  }
  if (!_init()) {
    return false;
  }
  close();
  if (addr) {
    IP_SET_TYPE_VAL(_pcb->local_ip, addr->type);
    IP_SET_TYPE_VAL(_pcb->remote_ip, addr->type);
  }
  UDP_MUTEX_LOCK();
  if (_udp_bind(_pcb, addr, port) != ERR_OK) {
    UDP_MUTEX_UNLOCK();
    return false;
  }
  _connected = true;
  UDP_MUTEX_UNLOCK();
  return true;
}

static esp_err_t joinMulticastGroup(const ip_addr_t *addr, bool join, tcpip_adapter_if_t tcpip_if = TCPIP_ADAPTER_IF_MAX) {
  struct netif *netif = NULL;
  if (tcpip_if < TCPIP_ADAPTER_IF_MAX) {
    void *nif = NULL;
    esp_err_t err = tcpip_adapter_get_netif(tcpip_if, &nif);
    if (err) {
      return ESP_ERR_INVALID_ARG;
    }
    netif = (struct netif *)nif;

    if (addr->type == IPADDR_TYPE_V4) {
      if (join) {
        if (igmp_joingroup_netif(netif, (const ip4_addr *)&(addr->u_addr.ip4))) {
          return ESP_ERR_INVALID_STATE;
        }
      } else {
        if (igmp_leavegroup_netif(netif, (const ip4_addr *)&(addr->u_addr.ip4))) {
          return ESP_ERR_INVALID_STATE;
        }
      }
    } else {
      if (join) {
        if (mld6_joingroup_netif(netif, &(addr->u_addr.ip6))) {
          return ESP_ERR_INVALID_STATE;
        }
      } else {
        if (mld6_leavegroup_netif(netif, &(addr->u_addr.ip6))) {
          return ESP_ERR_INVALID_STATE;
        }
      }
    }
  } else {
    if (addr->type == IPADDR_TYPE_V4) {
      if (join) {
        if (igmp_joingroup((const ip4_addr *)IP4_ADDR_ANY, (const ip4_addr *)&(addr->u_addr.ip4))) {
          return ESP_ERR_INVALID_STATE;
        }
      } else {
        if (igmp_leavegroup((const ip4_addr *)IP4_ADDR_ANY, (const ip4_addr *)&(addr->u_addr.ip4))) {
          return ESP_ERR_INVALID_STATE;
        }
      }
    } else {
      if (join) {
        if (mld6_joingroup((const ip6_addr *)IP6_ADDR_ANY, &(addr->u_addr.ip6))) {
          return ESP_ERR_INVALID_STATE;
        }
      } else {
        if (mld6_leavegroup((const ip6_addr *)IP6_ADDR_ANY, &(addr->u_addr.ip6))) {
          return ESP_ERR_INVALID_STATE;
        }
      }
    }
  }
  return ESP_OK;
}

bool AsyncUDP::listenMulticast(const ip_addr_t *addr, uint16_t port, uint8_t ttl, tcpip_adapter_if_t tcpip_if) {
  if (!ip_addr_ismulticast(addr)) {
    return false;
  }

  if (joinMulticastGroup(addr, true, tcpip_if) != ERR_OK) {
    return false;
  }

  if (!listen(NULL, port)) {
    return false;
  }

  UDP_MUTEX_LOCK();
  _pcb->mcast_ttl = ttl;
  _pcb->remote_port = port;
  ip_addr_copy(_pcb->remote_ip, *addr);
  //ip_addr_copy(_pcb->remote_ip, ip_addr_any_type);
  UDP_MUTEX_UNLOCK();

  return true;
}

size_t AsyncUDP::writeTo(const uint8_t *data, size_t len, const ip_addr_t *addr, uint16_t port, tcpip_adapter_if_t tcpip_if) {
  if (!_pcb) {
    UDP_MUTEX_LOCK();
    _pcb = udp_new();
    UDP_MUTEX_UNLOCK();
    if (_pcb == NULL) {
      return 0;
    }
  }
  if (len > CONFIG_TCP_MSS) {
    len = CONFIG_TCP_MSS;
  }
  _lastErr = ERR_OK;
  pbuf *pbt = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
  if (pbt != NULL) {
    uint8_t *dst = reinterpret_cast<uint8_t *>(pbt->payload);
    memcpy(dst, data, len);
    UDP_MUTEX_LOCK();
    if (tcpip_if < TCPIP_ADAPTER_IF_MAX) {
      void *nif = NULL;
      tcpip_adapter_get_netif((tcpip_adapter_if_t)tcpip_if, &nif);
      if (!nif) {
        _lastErr = _udp_sendto(_pcb, pbt, addr, port);
      } else {
        _lastErr = _udp_sendto_if(_pcb, pbt, addr, port, (struct netif *)nif);
      }
    } else {
      _lastErr = _udp_sendto(_pcb, pbt, addr, port);
    }
    UDP_MUTEX_UNLOCK();
    pbuf_free(pbt);
    if (_lastErr < ERR_OK) {
      return 0;
    }
    return len;
  }
  return 0;
}

void AsyncUDP::_recv(udp_pcb *upcb, pbuf *pb, const ip_addr_t *addr, uint16_t port, struct netif *netif) {
  while (pb != NULL) {
    pbuf *this_pb = pb;
    pb = pb->next;
    this_pb->next = NULL;
    if (_handler) {
      AsyncUDPPacket packet(this, this_pb, addr, port, netif);
      _handler(packet);
    }
    pbuf_free(this_pb);
  }
}

void AsyncUDP::_s_recv(void *arg, udp_pcb *upcb, pbuf *p, const ip_addr_t *addr, uint16_t port, struct netif *netif) {
  reinterpret_cast<AsyncUDP *>(arg)->_recv(upcb, p, addr, port, netif);
}

bool AsyncUDP::listen(uint16_t port) {
  return listen(IP_ANY_TYPE, port);
}

bool AsyncUDP::listen(const IPAddress addr, uint16_t port) {
  ip_addr_t laddr;
  addr.to_ip_addr_t(&laddr);
  return listen(&laddr, port);
}

bool AsyncUDP::listenMulticast(const IPAddress addr, uint16_t port, uint8_t ttl, tcpip_adapter_if_t tcpip_if) {
  ip_addr_t laddr;
  addr.to_ip_addr_t(&laddr);
  return listenMulticast(&laddr, port, ttl, tcpip_if);
}

bool AsyncUDP::connect(const IPAddress addr, uint16_t port) {
  ip_addr_t daddr;
  addr.to_ip_addr_t(&daddr);
  return connect(&daddr, port);
}

size_t AsyncUDP::writeTo(const uint8_t *data, size_t len, const IPAddress addr, uint16_t port, tcpip_adapter_if_t tcpip_if) {
  ip_addr_t daddr;
  addr.to_ip_addr_t(&daddr);
  return writeTo(data, len, &daddr, port, tcpip_if);
}

IPAddress AsyncUDP::listenIP() {
  if (!_pcb || _pcb->remote_ip.type != IPADDR_TYPE_V4) {
    return IPAddress();
  }
  return IPAddress(_pcb->remote_ip.u_addr.ip4.addr);
}

IPAddress AsyncUDP::listenIPv6() {
  if (!_pcb || _pcb->remote_ip.type != IPADDR_TYPE_V6) {
    return IPAddress(IPv6);
  }
  return IPAddress(IPv6, (const uint8_t *)_pcb->remote_ip.u_addr.ip6.addr, _pcb->remote_ip.u_addr.ip6.zone);
}

size_t AsyncUDP::write(const uint8_t *data, size_t len) {
  return writeTo(data, len, &(_pcb->remote_ip), _pcb->remote_port);
}

size_t AsyncUDP::write(uint8_t data) {
  return write(&data, 1);
}

size_t AsyncUDP::broadcastTo(uint8_t *data, size_t len, uint16_t port, tcpip_adapter_if_t tcpip_if) {
  return writeTo(data, len, IP_ADDR_BROADCAST, port, tcpip_if);
}

size_t AsyncUDP::broadcastTo(const char *data, uint16_t port, tcpip_adapter_if_t tcpip_if) {
  return broadcastTo((uint8_t *)data, strlen(data), port, tcpip_if);
}

size_t AsyncUDP::broadcast(uint8_t *data, size_t len) {
  if (_pcb->local_port != 0) {
    return broadcastTo(data, len, _pcb->local_port);
  }
  return 0;
}

size_t AsyncUDP::broadcast(const char *data) {
  return broadcast((uint8_t *)data, strlen(data));
}

size_t AsyncUDP::sendTo(AsyncUDPMessage &message, const ip_addr_t *addr, uint16_t port, tcpip_adapter_if_t tcpip_if) {
  if (!message) {
    return 0;
  }
  return writeTo(message.data(), message.length(), addr, port, tcpip_if);
}

size_t AsyncUDP::sendTo(AsyncUDPMessage &message, const IPAddress addr, uint16_t port, tcpip_adapter_if_t tcpip_if) {
  if (!message) {
    return 0;
  }
  return writeTo(message.data(), message.length(), addr, port, tcpip_if);
}

size_t AsyncUDP::send(AsyncUDPMessage &message) {
  if (!message) {
    return 0;
  }
  return writeTo(message.data(), message.length(), &(_pcb->remote_ip), _pcb->remote_port);
}

size_t AsyncUDP::broadcastTo(AsyncUDPMessage &message, uint16_t port, tcpip_adapter_if_t tcpip_if) {
  if (!message) {
    return 0;
  }
  return broadcastTo(message.data(), message.length(), port, tcpip_if);
}

size_t AsyncUDP::broadcast(AsyncUDPMessage &message) {
  if (!message) {
    return 0;
  }
  return broadcast(message.data(), message.length());
}

AsyncUDP::operator bool() {
  return _connected;
}

bool AsyncUDP::connected() {
  return _connected;
}

esp_err_t AsyncUDP::lastErr() {
  return _lastErr;
}

void AsyncUDP::onPacket(AuPacketHandlerFunctionWithArg cb, void *arg) {
  onPacket(std::bind(cb, arg, std::placeholders::_1));
}

void AsyncUDP::onPacket(AuPacketHandlerFunction cb) {
  _handler = cb;
}
