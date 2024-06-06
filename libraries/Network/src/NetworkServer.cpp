/*
  Server.cpp - Server class for Raspberry Pi
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
#include "NetworkServer.h"
#include <lwip/sockets.h>
#include <lwip/netdb.h>

#undef write
#undef close

int NetworkServer::setTimeout(uint32_t seconds) {
  struct timeval tv;
  tv.tv_sec = seconds;
  tv.tv_usec = 0;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval)) < 0) {
    return -1;
  }
  return setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(struct timeval));
}

NetworkClient NetworkServer::available() {
  return accept();
}

NetworkClient NetworkServer::accept() {
  if (!_listening) {
    return NetworkClient();
  }
  int client_sock;
  if (_accepted_sockfd >= 0) {
    client_sock = _accepted_sockfd;
    _accepted_sockfd = -1;
  } else {
    struct sockaddr_in6 _client;
    int cs = sizeof(struct sockaddr_in6);
#ifdef ESP_IDF_VERSION_MAJOR
    client_sock = lwip_accept(sockfd, (struct sockaddr *)&_client, (socklen_t *)&cs);
#else
    client_sock = lwip_accept_r(sockfd, (struct sockaddr *)&_client, (socklen_t *)&cs);
#endif
  }
  if (client_sock >= 0) {
    int val = 1;
    if (setsockopt(client_sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&val, sizeof(int)) == ESP_OK) {
      val = _noDelay;
      if (setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&val, sizeof(int)) == ESP_OK) {
        return NetworkClient(client_sock);
      }
    }
  }
  return NetworkClient();
}

void NetworkServer::begin(uint16_t port) {
  begin(port, 1);
}

void NetworkServer::begin(uint16_t port, int enable) {
  if (_listening) {
    return;
  }
  if (port) {
    _port = port;
  }
  struct sockaddr_in6 server;
  sockfd = socket(AF_INET6, SOCK_STREAM, 0);
  if (sockfd < 0) {
    return;
  }
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  server.sin6_family = AF_INET6;
  if (_addr.type() == IPv4) {
    memcpy(server.sin6_addr.s6_addr + 11, (uint8_t *)&_addr[0], 4);
    server.sin6_addr.s6_addr[10] = 0xFF;
    server.sin6_addr.s6_addr[11] = 0xFF;
  } else {
    memcpy(server.sin6_addr.s6_addr, (uint8_t *)&_addr[0], 16);
  }
  memset(server.sin6_addr.s6_addr, 0x0, 16);
  server.sin6_port = htons(_port);
  if (bind(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0) {
    return;
  }
  if (listen(sockfd, _max_clients) < 0) {
    return;
  }
  fcntl(sockfd, F_SETFL, O_NONBLOCK);
  _listening = true;
  _noDelay = false;
  _accepted_sockfd = -1;
}

void NetworkServer::setNoDelay(bool nodelay) {
  _noDelay = nodelay;
}

bool NetworkServer::getNoDelay() {
  return _noDelay;
}

bool NetworkServer::hasClient() {
  if (_accepted_sockfd >= 0) {
    return true;
  }
  struct sockaddr_in6 _client;
  int cs = sizeof(struct sockaddr_in6);
#ifdef ESP_IDF_VERSION_MAJOR
  _accepted_sockfd = lwip_accept(sockfd, (struct sockaddr *)&_client, (socklen_t *)&cs);
#else
  _accepted_sockfd = lwip_accept_r(sockfd, (struct sockaddr *)&_client, (socklen_t *)&cs);
#endif
  if (_accepted_sockfd >= 0) {
    return true;
  }
  return false;
}

void NetworkServer::end() {
#ifdef ESP_IDF_VERSION_MAJOR
  lwip_close(sockfd);
#else
  lwip_close_r(sockfd);
#endif
  sockfd = -1;
  _listening = false;
}

void NetworkServer::close() {
  end();
}

void NetworkServer::stop() {
  end();
}
