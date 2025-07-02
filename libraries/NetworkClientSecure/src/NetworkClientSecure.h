/*
  NetworkClientSecure.h - Base class that provides Client SSL to ESP32
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

#ifndef NetworkClientSecure_h
#define NetworkClientSecure_h
#include "Arduino.h"
#include "IPAddress.h"
#include "Network.h"
#include "ssl_client.h"
#include <memory>

class NetworkClientSecure : public NetworkClient {
protected:
  std::shared_ptr<sslclient_context> sslclient;

  bool _use_insecure;
  bool _stillinPlainStart;
  bool _ca_cert_free;
  bool _cert_free;
  bool _private_key_free;
  const char *_CA_cert;
  const char *_cert;
  const char *_private_key;
  const char *_pskIdent;  // identity for PSK cipher suites
  const char *_psKey;     // key in hex for PSK cipher suites
  const char **_alpn_protos;
  bool _use_ca_bundle;

public:
  NetworkClientSecure *next;
  NetworkClientSecure();
  NetworkClientSecure(int socket);
  ~NetworkClientSecure();
  int connect(IPAddress ip, uint16_t port);
  int connect(IPAddress ip, uint16_t port, int32_t timeout);
  int connect(const char *host, uint16_t port);
  int connect(const char *host, uint16_t port, int32_t timeout);
  int connect(IPAddress ip, uint16_t port, const char *rootCABuff, const char *cli_cert, const char *cli_key);
  int connect(const char *host, uint16_t port, const char *rootCABuff, const char *cli_cert, const char *cli_key);
  int connect(IPAddress ip, uint16_t port, const char *pskIdent, const char *psKey);
  int connect(const char *host, uint16_t port, const char *pskIdent, const char *psKey);
  int connect(IPAddress ip, uint16_t port, const char *host, const char *CA_cert, const char *cert, const char *private_key);
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
  void setInsecure();                                             // Don't validate the chain, just accept whatever is given.  VERY INSECURE!
  void setPreSharedKey(const char *pskIdent, const char *psKey);  // psKey in Hex
  void setCACert(const char *rootCA);
  void setCertificate(const char *client_ca);
  void setPrivateKey(const char *private_key);
  bool loadCACert(Stream &stream, size_t size);
  void setCACertBundle(const uint8_t *bundle, size_t size);
  bool loadCertificate(Stream &stream, size_t size);
  bool loadPrivateKey(Stream &stream, size_t size);
  bool verify(const char *fingerprint, const char *domain_name);
  void setHandshakeTimeout(unsigned long handshake_timeout);
  void setAlpnProtocols(const char **alpn_protos);

  // Certain protocols start in plain-text; and then have the client
  // give some STARTSSL command to `upgrade' the connection to TLS
  // or SSL. Setting PlainStart to true (the default is false) enables
  // this. It is up to the application code to then call 'startTLS()'
  // at the right point to initialize the SSL or TLS upgrade.

  void setPlainStart() {
    _stillinPlainStart = true;
  };
  bool stillInPlainStart() {
    return _stillinPlainStart;
  };
  int startTLS();

  const mbedtls_x509_crt *getPeerCertificate() {
    return mbedtls_ssl_get_peer_cert(&sslclient->ssl_ctx);
  };
  bool getFingerprintSHA256(uint8_t sha256_result[32]) {
    return get_peer_fingerprint(sslclient.get(), sha256_result);
  };
  int fd() const;

  operator bool() {
    return connected();
  }

  bool operator==(const bool value) {
    return bool() == value;
  }
  bool operator!=(const bool value) {
    return bool() != value;
  }
  bool operator==(const NetworkClientSecure &);
  bool operator!=(const NetworkClientSecure &rhs) {
    return !this->operator==(rhs);
  };

  int socket() {
    return sslclient->socket = -1;
  }

private:
  char *_streamLoad(Stream &stream, size_t size);

  //friend class NetworkServer;
  using Print::write;
};

#endif /* _WIFICLIENT_H_ */
