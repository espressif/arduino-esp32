/*
  Client.h - Base class that provides Client
  Copyright (c) 2011 Adrian McEwen.  All right reserved.

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

#pragma once


#include "Arduino.h"
#include "Client.h"
#include <memory>

class NetworkClientSocketHandle;
class NetworkClientRxBuffer;

class ESPLwIPClient : public Client {
public:
  virtual int connect(IPAddress ip, uint16_t port, int32_t timeout) = 0;
  virtual int connect(const char *host, uint16_t port, int32_t timeout) = 0;
  virtual void setConnectionTimeout(uint32_t milliseconds) = 0;
};

class NetworkClient : public ESPLwIPClient {
protected:
  std::shared_ptr<NetworkClientSocketHandle> clientSocketHandle;
  std::shared_ptr<NetworkClientRxBuffer> _rxBuffer;
  bool _connected;
  bool _sse;
  int _timeout;
  int _lastWriteTimeout;
  int _lastReadTimeout;

public:
  NetworkClient *next;
  NetworkClient();
  NetworkClient(int fd);
  ~NetworkClient();
  int connect(IPAddress ip, uint16_t port);
  int connect(IPAddress ip, uint16_t port, int32_t timeout_ms);
  int connect(const char *host, uint16_t port);
  int connect(const char *host, uint16_t port, int32_t timeout_ms);
  size_t write(uint8_t data);
  size_t write(const uint8_t *buf, size_t size);
  size_t write_P(PGM_P buf, size_t size);
  size_t write(Stream &stream);
  void flush();  // Print::flush tx
  int available();
  int read();
  int read(uint8_t *buf, size_t size);
  int peek();
  void clear();  // clear rx
  void stop();
  uint8_t connected();
  void setSSE(bool sse);
  bool isSSE();

  operator bool() {
    return connected();
  }
  bool operator==(const bool value) {
    return bool() == value;
  }
  bool operator!=(const bool value) {
    return bool() != value;
  }
  bool operator==(const NetworkClient &);
  bool operator!=(const NetworkClient &rhs) {
    return !this->operator==(rhs);
  };

  virtual int fd() const;

  int setSocketOption(int option, char *value, size_t len);
  int setSocketOption(int level, int option, const void *value, size_t len);
  int getSocketOption(int level, int option, const void *value, size_t size);
  int setOption(int option, int *value);
  int getOption(int option, int *value);
  void setConnectionTimeout(uint32_t milliseconds);
  int setNoDelay(bool nodelay);
  bool getNoDelay();

  IPAddress remoteIP() const;
  IPAddress remoteIP(int fd) const;
  uint16_t remotePort() const;
  uint16_t remotePort(int fd) const;
  IPAddress localIP() const;
  IPAddress localIP(int fd) const;
  uint16_t localPort() const;
  uint16_t localPort(int fd) const;

  //friend class NetworkServer;
  using Print::write;
};
