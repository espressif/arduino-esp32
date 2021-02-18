// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef WiFiClientSecure_h
#define WiFiClientSecure_h
#include "Arduino.h"
#include "IPAddress.h"
#include <WiFi.h>
#include "ssl_client.h"

class WiFiClientSecure : public WiFiClient
{
protected:
    sslclient_context *sslclient;
 
    int _lastError = 0;
	int _peek = -1;
    int _timeout = 0;
    bool _use_insecure;
    const char *_CA_cert;
    const char *_cert;
    const char *_private_key;
    const char *_pskIdent; // identity for PSK cipher suites
    const char *_psKey; // key in hex for PSK cipher suites

public:
    WiFiClientSecure *next;
    WiFiClientSecure();
    WiFiClientSecure(int socket);
    ~WiFiClientSecure();
    int connect(IPAddress ip, uint16_t port);
    int connect(IPAddress ip, uint16_t port, int32_t timeout);
    int connect(const char *host, uint16_t port);
    int connect(const char *host, uint16_t port, int32_t timeout);
    int connect(IPAddress ip, uint16_t port, const char *rootCABuff, const char *cli_cert, const char *cli_key);
    int connect(const char *host, uint16_t port, const char *rootCABuff, const char *cli_cert, const char *cli_key);
    int connect(IPAddress ip, uint16_t port, const char *pskIdent, const char *psKey);
    int connect(const char *host, uint16_t port, const char *pskIdent, const char *psKey);
	int peek();
    size_t write(uint8_t data);
    size_t write(const uint8_t *buf, size_t size);
    int available();
    int read();
    int read(uint8_t *buf, size_t size);
    void flush() {}
    void stop();
    uint8_t connected();
    int lastError(char *buf, const size_t size);
    void setInsecure(); // Don't validate the chain, just accept whatever is given.  VERY INSECURE!
    void setPreSharedKey(const char *pskIdent, const char *psKey); // psKey in Hex
    void setCACert(const char *rootCA);
    void setCertificate(const char *client_ca);
    void setPrivateKey (const char *private_key);
    bool loadCACert(Stream& stream, size_t size);
    bool loadCertificate(Stream& stream, size_t size);
    bool loadPrivateKey(Stream& stream, size_t size);
    bool verify(const char* fingerprint, const char* domain_name);
    void setHandshakeTimeout(unsigned long handshake_timeout);

    int setTimeout(uint32_t seconds){ return 0; }

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

private:
    char *_streamLoad(Stream& stream, size_t size);

    //friend class WiFiServer;
    using Print::write;
};

#endif /* _WIFICLIENT_H_ */
