/*
  WiFiClientSecure.h - Base class that provides Client SSL to ESP32
  Copyright (c) 2011 Adrian McEwen.  All right reserved.
  Additions Copyright (C) 2017 Evandro Luis Copercini.

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

#ifndef WiFiClientSecure_h
#define WiFiClientSecure_h
#include "Arduino.h"
#include "IPAddress.h"
#include <WiFi.h>
#include "ssl_client.h"

class WiFiClientSecure : public WiFiClient
{
protected:
    bool _connected;
    sslclient_context *sslclient;

    const uint8_t *_CA_cert;
    const uint8_t *_cert;
    const uint8_t *_private_key;
    size_t _CA_cert_len;
    size_t _cert_len;
    size_t _private_key_len;

public:
    WiFiClientSecure *next;
    WiFiClientSecure();
    WiFiClientSecure(int socket);
    ~WiFiClientSecure();
    int connect(IPAddress ip, uint16_t port);
    int connect(const char *host, uint16_t port);
    int connect(IPAddress ip, uint16_t port, const char *rootCABuff, const char *cli_cert, const char *cli_key);
    int connect(const char *host, uint16_t port, const char *rootCABuff, const char *cli_cert, const char *cli_key);
    int connect(IPAddress ip, uint16_t port, const uint8_t *rootCABuff, size_t rootCA_len, const uint8_t *cli_cert=0, size_t cli_cert_len=0, const uint8_t *cli_key=0, size_t cli_key_len=0);
    int connect(const char *host, uint16_t port, const uint8_t *rootCABuff, size_t rootCA_len, const uint8_t *cli_cert=0, size_t cli_cert_len=0, const uint8_t *cli_key=0, size_t cli_key_len=0);
    size_t write(uint8_t data);
    size_t write(const uint8_t *buf, size_t size);
    int available();
    int read();
    int read(uint8_t *buf, size_t size);
    int peek()
    {
        return 0;
    }
    void flush() {}
    void stop();
    uint8_t connected();

    void setCACert(const char *rootCA);
    void setCACert(const uint8_t *rootCA, size_t size);
    void setCertificate(const char *client_ca);
    void setPrivateKey (const char *private_key);

    operator bool()
    {
        return connected();
    }
    WiFiClientSecure &operator=(const WiFiClientSecure &other);
    bool operator==(const bool value)
    {
        return bool() == value;
    }
    bool operator!=(const bool value)
    {
        return bool() != value;
    }
    bool operator==(const WiFiClientSecure &);
    bool operator!=(const WiFiClientSecure &rhs)
    {
        return !this->operator==(rhs);
    };

    int socket()
    {
        return sslclient->socket = -1;
    }

    //friend class WiFiServer;
    using Print::write;
};

#endif /* _WIFICLIENT_H_ */
