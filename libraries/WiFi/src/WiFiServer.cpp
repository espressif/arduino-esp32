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
#include "WiFiServer.h"
#include <lwip/sockets.h>
#include <lwip/netdb.h>

#undef write
#undef close

#if !CONFIG_DISABLE_HAL_LOCKS
#define WIFISERVER_MUTEX_LOCK()    do {} while (xSemaphoreTake(_lock, portMAX_DELAY) != pdPASS)
#define WIFISERVER_MUTEX_UNLOCK()  xSemaphoreGive(_lock)
#else
#define WIFISERVER_MUTEX_LOCK()
#define WIFISERVER_MUTEX_UNLOCK()
#endif

WiFiServer::WiFiServer(uint16_t port, uint8_t max_clients) :
    sockfd(-1), _accepted_sockfd(-1), _addr(), _port(port), _max_clients(max_clients), _listening(false), _noDelay(false)
#if !CONFIG_DISABLE_HAL_LOCKS
        , _lock(NULL)
#endif
{
  log_v("WiFiServer::WiFiServer(port=%d, ...)", port);
#if !CONFIG_DISABLE_HAL_LOCKS
  if (_lock == NULL) {
    _lock = xSemaphoreCreateMutex();
    if (_lock == NULL) {
      log_e("xSemaphoreCreateMutex failed");
      return;
    }
  }
#endif
}

WiFiServer::WiFiServer(const IPAddress &addr, uint16_t port, uint8_t max_clients) :
    sockfd(-1), _accepted_sockfd(-1), _addr(addr), _port(port), _max_clients(max_clients), _listening(false), _noDelay(false)
#if !CONFIG_DISABLE_HAL_LOCKS
        , _lock(NULL)
#endif
{
  log_v("WiFiServer::WiFiServer(addr=%s, port=%d, ...)", addr.toString().c_str(), port);
#if !CONFIG_DISABLE_HAL_LOCKS
  if (_lock == NULL) {
    _lock = xSemaphoreCreateMutex();
    if (_lock == NULL) {
      log_e("xSemaphoreCreateMutex failed");
      return;
    }
  }
#endif
}

WiFiServer::~WiFiServer() {
  end();
#if !CONFIG_DISABLE_HAL_LOCKS
  if (_lock != NULL) {
    vSemaphoreDelete(_lock);
  }
#endif
}

int WiFiServer::setTimeout(uint32_t seconds){
  struct timeval tv;
  tv.tv_sec = seconds;
  tv.tv_usec = 0;
  if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval)) < 0)
    return -1;
  return setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(struct timeval));
}

size_t WiFiServer::write(const uint8_t *data, size_t len){
  WIFISERVER_MUTEX_LOCK();
  static uint32_t lastCheck;
  uint32_t m = millis();
  if (m - lastCheck > 100) {
    lastCheck = m;
    acceptClients();
  }
  size_t ret = 0;
  if (len > 0) {
    for (uint8_t i = 0; i < SERVER_MAX_MONITORED_CLIENTS; i++) {
      if (connectedClients[i].connected()) {
        ret += connectedClients[i].write(data, len);
      }
    }
  }
  WIFISERVER_MUTEX_UNLOCK();
  return ret;
}

void WiFiServer::stopAll(){}

// https://www.arduino.cc/en/Reference/WiFiServerAvailable
WiFiClient WiFiServer::available() {
  WIFISERVER_MUTEX_LOCK();

  acceptClients();

  WiFiClient ret;

  // find next client with data available
  for (uint8_t i = 0; i < SERVER_MAX_MONITORED_CLIENTS; i++) {
    if (index == SERVER_MAX_MONITORED_CLIENTS) {
      index = 0;
    }
    WiFiClient& client = connectedClients[index];
    index++;
    if (client.available()) {
      ret = client;
      break;
    }
  }
  WIFISERVER_MUTEX_UNLOCK();
  return ret;
}

WiFiClient WiFiServer::accept(){
  if(!_listening)
    return WiFiClient();
  int client_sock;
  if (_accepted_sockfd >= 0) {
    client_sock = _accepted_sockfd;
    _accepted_sockfd = -1;
  }
  else {
  struct sockaddr_in6 _client;
  int cs = sizeof(struct sockaddr_in6);
#ifdef ESP_IDF_VERSION_MAJOR
    client_sock = lwip_accept(sockfd, (struct sockaddr *)&_client, (socklen_t*)&cs);
#else
    client_sock = lwip_accept_r(sockfd, (struct sockaddr *)&_client, (socklen_t*)&cs);
#endif
  }
  if(client_sock >= 0){
    int val = 1;
    if(setsockopt(client_sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&val, sizeof(int)) == ESP_OK) {
      val = _noDelay;
      if(setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, (char*)&val, sizeof(int)) == ESP_OK)
        return WiFiClient(client_sock);
    }
  }
  return WiFiClient();
}

void WiFiServer::begin(uint16_t port){
    begin(port, 1);
}

void WiFiServer::begin(uint16_t port, int enable){
  if(_listening)
    return;
  if(port){
      _port = port;
  }
  struct sockaddr_in6 server;
  sockfd = socket(AF_INET6 , SOCK_STREAM, 0);
  if (sockfd < 0)
    return;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  server.sin6_family = AF_INET6;
  if (_addr.type() == IPv4) {
    memcpy(server.sin6_addr.s6_addr+11, (uint8_t*)&_addr[0], 4);
    server.sin6_addr.s6_addr[10] = 0xFF;
    server.sin6_addr.s6_addr[11] = 0xFF;
  } else {
    memcpy(server.sin6_addr.s6_addr, (uint8_t*)&_addr[0], 16);
  }
  memset(server.sin6_addr.s6_addr, 0x0, 16);
  server.sin6_port = htons(_port);
  if(bind(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
    return;
  if(listen(sockfd , _max_clients) < 0)
    return;
  fcntl(sockfd, F_SETFL, O_NONBLOCK);
  _listening = true;
  _noDelay = false;
  _accepted_sockfd = -1;
}

void WiFiServer::setNoDelay(bool nodelay) {
    _noDelay = nodelay;
}

bool WiFiServer::getNoDelay() {
    return _noDelay;
}

bool WiFiServer::hasClient() {
    if (_accepted_sockfd >= 0) {
      return true;
    }
    struct sockaddr_in6 _client;
    int cs = sizeof(struct sockaddr_in6);
#ifdef ESP_IDF_VERSION_MAJOR
    _accepted_sockfd = lwip_accept(sockfd, (struct sockaddr *)&_client, (socklen_t*)&cs);
#else
    _accepted_sockfd = lwip_accept_r(sockfd, (struct sockaddr *)&_client, (socklen_t*)&cs);
#endif
    if (_accepted_sockfd >= 0) {
      return true;
    }
    return false;
}

void WiFiServer::end(){
#ifdef ESP_IDF_VERSION_MAJOR
  lwip_close(sockfd);
#else
  lwip_close_r(sockfd);
#endif
  sockfd = -1;
  _listening = false;

  WIFISERVER_MUTEX_LOCK();
  for (uint8_t i = 0; i < SERVER_MAX_MONITORED_CLIENTS; i++) {
    if (connectedClients[i]) {
      connectedClients[i].stop();
    }
  }
  WIFISERVER_MUTEX_UNLOCK();
}

void WiFiServer::close(){
  end();
}

void WiFiServer::stop(){
  end();
}

void WiFiServer::acceptClients() {
  for (uint8_t i = 0; i < SERVER_MAX_MONITORED_CLIENTS; i++) {
    WiFiClient& client = connectedClients[i];
    if (!client.connected() && !client.available()) {
      client = accept();
    }
  }
}

