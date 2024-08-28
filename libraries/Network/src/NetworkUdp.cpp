/*
  Udp.cpp - UDP class for Raspberry Pi
  Copyright (c) 2016 Hristo Gochkov  All right reserved.

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

#include "NetworkUdp.h"
#include <new>  //std::nothrow
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <errno.h>

#undef write
#undef read

NetworkUDP::NetworkUDP() : udp_server(-1), server_port(0), remote_port(0), tx_buffer(0), tx_buffer_len(0), rx_buffer(0) {}

NetworkUDP::~NetworkUDP() {
  stop();
}

uint8_t NetworkUDP::begin(IPAddress address, uint16_t port) {
  stop();

  server_port = port;

  tx_buffer = (char *)malloc(1460);
  if (!tx_buffer) {
    log_e("could not create tx buffer: %d", errno);
    return 0;
  }
  tx_buffer_len = 0;

#if LWIP_IPV6
  if ((udp_server = socket((address.type() == IPv6) ? AF_INET6 : AF_INET, SOCK_DGRAM, 0)) == -1) {
#else
  if ((udp_server = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
#endif
    log_e("could not create socket: %d", errno);
    return 0;
  }

  int yes = 1;
  if (setsockopt(udp_server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
    log_e("could not set socket option: %d", errno);
    stop();
    return 0;
  }

  struct sockaddr_storage serveraddr = {};
  size_t sock_size = 0;
#if LWIP_IPV6
  if (address.type() == IPv6) {
    struct sockaddr_in6 *tmpaddr = (struct sockaddr_in6 *)&serveraddr;
    ip_addr_t addr;
    address.to_ip_addr_t(&addr);
    memset((char *)tmpaddr, 0, sizeof(struct sockaddr_in));
    tmpaddr->sin6_family = AF_INET6;
    tmpaddr->sin6_port = htons(server_port);
    tmpaddr->sin6_scope_id = addr.u_addr.ip6.zone;
    inet6_addr_from_ip6addr(&tmpaddr->sin6_addr, ip_2_ip6(&addr));
    tmpaddr->sin6_flowinfo = 0;
    sock_size = sizeof(sockaddr_in6);
  } else
#endif
  {
    struct sockaddr_in *tmpaddr = (struct sockaddr_in *)&serveraddr;
    memset((char *)tmpaddr, 0, sizeof(struct sockaddr_in));
    tmpaddr->sin_family = AF_INET;
    tmpaddr->sin_port = htons(server_port);
    tmpaddr->sin_addr.s_addr = (in_addr_t)address;
    sock_size = sizeof(sockaddr_in);
  }
  if (bind(udp_server, (sockaddr *)&serveraddr, sock_size) == -1) {
    log_e("could not bind socket: %d", errno);
    stop();
    return 0;
  }
  fcntl(udp_server, F_SETFL, O_NONBLOCK);
  return 1;
}

uint8_t NetworkUDP::begin(uint16_t p) {
  return begin(IPAddress(), p);
}

uint8_t NetworkUDP::beginMulticast(IPAddress address, uint16_t p) {
  if (begin(IPAddress(), p)) {
    ip_addr_t addr;
    address.to_ip_addr_t(&addr);
    if (ip_addr_ismulticast(&addr)) {
#if LWIP_IPV6
      if (address.type() == IPv6) {
        struct ipv6_mreq mreq;
        bool joined = false;
        inet6_addr_from_ip6addr(&mreq.ipv6mr_multiaddr, ip_2_ip6(&addr));

        // iterate on each interface
        for (netif *intf = netif_list; intf != nullptr; intf = intf->next) {
          mreq.ipv6mr_interface = intf->num + 1;
          if (intf->name[0] != 'l' || intf->name[1] != 'o') {  // skip 'lo' local interface
            int ret = setsockopt(udp_server, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq));
            if (ret >= 0) {
              joined = true;
            }
          }
        }
        if (!joined) {
          log_e("could not join igmp: %d", errno);
          stop();
          return 0;
        }
      } else
#endif
      {
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = (in_addr_t)address;
        mreq.imr_interface.s_addr = INADDR_ANY;
        if (setsockopt(udp_server, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
          log_e("could not join igmp: %d", errno);
          stop();
          return 0;
        }
      }
      multicast_ip = address;
      return 1;
    }
  }
  return 0;
}

void NetworkUDP::stop() {
  if (tx_buffer) {
    free(tx_buffer);
    tx_buffer = NULL;
  }
  tx_buffer_len = 0;
  if (rx_buffer) {
    cbuf *b = rx_buffer;
    rx_buffer = NULL;
    delete b;
  }
  if (udp_server == -1) {
    return;
  }
  ip_addr_t addr;
  multicast_ip.to_ip_addr_t(&addr);
  if (!ip_addr_isany(&addr)) {
#if LWIP_IPV6
    if (multicast_ip.type() == IPv6) {
      struct ipv6_mreq mreq;
      inet6_addr_from_ip6addr(&mreq.ipv6mr_multiaddr, ip_2_ip6(&addr));

      // iterate on each interface
      for (netif *intf = netif_list; intf != nullptr; intf = intf->next) {
        mreq.ipv6mr_interface = intf->num + 1;
        if (intf->name[0] != 'l' || intf->name[1] != 'o') {  // skip 'lo' local interface
          setsockopt(udp_server, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &mreq, sizeof(mreq));
        }
      }
    } else
#endif
    {
      struct ip_mreq mreq;
      mreq.imr_multiaddr.s_addr = (in_addr_t)multicast_ip;
      mreq.imr_interface.s_addr = (in_addr_t)0;
      setsockopt(udp_server, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
    }
    // now common code for v4/v6
    multicast_ip = IPAddress();
  }
  close(udp_server);
  udp_server = -1;
}

int NetworkUDP::beginMulticastPacket() {
  if (!server_port || multicast_ip == IPAddress()) {
    return 0;
  }
  remote_ip = multicast_ip;
  remote_port = server_port;
  return beginPacket();
}

int NetworkUDP::beginPacket() {
  if (!remote_port) {
    return 0;
  }

  // allocate tx_buffer if is necessary
  if (!tx_buffer) {
    tx_buffer = (char *)malloc(1460);
    if (!tx_buffer) {
      log_e("could not create tx buffer: %d", errno);
      return 0;
    }
  }
  tx_buffer_len = 0;

  // check whereas socket is already open
  if (udp_server != -1) {
    return 1;
  }

  if ((udp_server = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    log_e("could not create socket: %d", errno);
    return 0;
  }

  fcntl(udp_server, F_SETFL, O_NONBLOCK);

  return 1;
}

int NetworkUDP::beginPacket(IPAddress ip, uint16_t port) {
  remote_ip = ip;
  remote_port = port;
  return beginPacket();
}

int NetworkUDP::beginPacket(const char *host, uint16_t port) {
  struct hostent *server;
  server = gethostbyname(host);
  if (server == NULL) {
    log_e("could not get host from dns: %d", errno);
    return 0;
  }
  return beginPacket(IPAddress((const uint8_t *)(server->h_addr_list[0])), port);
}

int NetworkUDP::endPacket() {
  ip_addr_t addr;
  remote_ip.to_ip_addr_t(&addr);

  if (remote_ip.type() == IPv4) {
    struct sockaddr_in recipient;
    recipient.sin_addr.s_addr = (uint32_t)remote_ip;
    recipient.sin_family = AF_INET;
    recipient.sin_port = htons(remote_port);
    int sent = sendto(udp_server, tx_buffer, tx_buffer_len, 0, (struct sockaddr *)&recipient, sizeof(recipient));
    if (sent < 0) {
      log_e("could not send data: %d", errno);
      return 0;
    }
  } else {
    struct sockaddr_in6 recipient;
    recipient.sin6_flowinfo = 0;
    recipient.sin6_addr = *(in6_addr *)(ip_addr_t *)(&addr);
    recipient.sin6_family = AF_INET6;
    recipient.sin6_port = htons(remote_port);
    recipient.sin6_scope_id = remote_ip.zone();
    int sent = sendto(udp_server, tx_buffer, tx_buffer_len, 0, (struct sockaddr *)&recipient, sizeof(recipient));
    if (sent < 0) {
      log_e("could not send data: %d", errno);
      return 0;
    }
  }
  return 1;
}

size_t NetworkUDP::write(uint8_t data) {
  if (tx_buffer_len == 1460) {
    endPacket();
    tx_buffer_len = 0;
  }
  tx_buffer[tx_buffer_len++] = data;
  return 1;
}

size_t NetworkUDP::write(const uint8_t *buffer, size_t size) {
  size_t i;
  for (i = 0; i < size; i++) {
    write(buffer[i]);
  }
  return i;
}

void NetworkUDP::flush() {
  clear();
}

int NetworkUDP::parsePacket() {
  if (rx_buffer) {
    return 0;
  }
  struct sockaddr_storage si_other_storage;  // enough storage for v4 and v6
  socklen_t slen = sizeof(sockaddr_storage);
  int len = 0;
  if (ioctl(udp_server, FIONREAD, &len) == -1) {
    log_e("could not check for data in buffer length: %d", errno);
    return 0;
  }
  if (!len) {
    return 0;
  }
  char *buf = (char *)malloc(1460);
  if (!buf) {
    return 0;
  }
  if ((len = recvfrom(udp_server, buf, 1460, MSG_DONTWAIT, (struct sockaddr *)&si_other_storage, (socklen_t *)&slen)) == -1) {
    free(buf);
    if (errno == EWOULDBLOCK) {
      return 0;
    }
    log_e("could not receive data: %d", errno);
    return 0;
  }
  if (si_other_storage.ss_family == AF_INET) {
    struct sockaddr_in &si_other = (sockaddr_in &)si_other_storage;
    remote_ip = IPAddress(si_other.sin_addr.s_addr);
    remote_port = ntohs(si_other.sin_port);
  }
#if LWIP_IPV6
  else if (si_other_storage.ss_family == AF_INET6) {
    struct sockaddr_in6 &si_other = (sockaddr_in6 &)si_other_storage;
    remote_ip = IPAddress(IPv6, (uint8_t *)&si_other.sin6_addr, si_other.sin6_scope_id);  // force IPv6
    ip_addr_t addr;
    remote_ip.to_ip_addr_t(&addr);
    /* Dual-stack: Unmap IPv4 mapped IPv6 addresses */
    if (remote_ip.type() == IPv6 && ip6_addr_isipv4mappedipv6(ip_2_ip6(&addr))) {
      unmap_ipv4_mapped_ipv6(ip_2_ip4(&addr), ip_2_ip6(&addr));
      IP_SET_TYPE_VAL(addr, IPADDR_TYPE_V4);
      remote_ip.from_ip_addr_t(&addr);
    }
    remote_port = ntohs(si_other.sin6_port);
  }
#endif  // LWIP_IPV6=1
  else {
    remote_ip = ip_addr_any.u_addr.ip4.addr;
    remote_port = 0;
  }
  if (len > 0) {
    rx_buffer = new (std::nothrow) cbuf(len);
    rx_buffer->write(buf, len);
  }
  free(buf);
  return len;
}

int NetworkUDP::available() {
  if (!rx_buffer) {
    return 0;
  }
  return rx_buffer->available();
}

int NetworkUDP::read() {
  if (!rx_buffer) {
    return -1;
  }
  int out = rx_buffer->read();
  if (!rx_buffer->available()) {
    cbuf *b = rx_buffer;
    rx_buffer = 0;
    delete b;
  }
  return out;
}

int NetworkUDP::read(unsigned char *buffer, size_t len) {
  return read((char *)buffer, len);
}

int NetworkUDP::read(char *buffer, size_t len) {
  if (!rx_buffer) {
    return 0;
  }
  int out = rx_buffer->read(buffer, len);
  if (!rx_buffer->available()) {
    cbuf *b = rx_buffer;
    rx_buffer = 0;
    delete b;
  }
  return out;
}

int NetworkUDP::peek() {
  if (!rx_buffer) {
    return -1;
  }
  return rx_buffer->peek();
}

void NetworkUDP::clear() {
  if (!rx_buffer) {
    return;
  }
  cbuf *b = rx_buffer;
  rx_buffer = 0;
  delete b;
}

IPAddress NetworkUDP::remoteIP() {
  return remote_ip;
}

uint16_t NetworkUDP::remotePort() {
  return remote_port;
}
