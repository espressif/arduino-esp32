/*
  NetworkClientSecure.cpp - Client Secure class for ESP32
  Copyright (c) 2016 Hristo Gochkov  All right reserved.
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

#include "NetworkClientSecure.h"
#include "esp_crt_bundle.h"
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <errno.h>

#undef connect
#undef write
#undef read

NetworkClientSecure::NetworkClientSecure() {
  _connected = false;
  _timeout = 30000;  // Same default as ssl_client

  sslclient.reset(new sslclient_context, [](struct sslclient_context *sslclient) {
    stop_ssl_socket(sslclient);
    delete sslclient;
  });
  ssl_init(sslclient.get());
  sslclient->socket = -1;
  sslclient->handshake_timeout = 120000;
  _use_insecure = false;
  _CA_cert = NULL;
  _cert = NULL;
  _private_key = NULL;
  _pskIdent = NULL;
  _psKey = NULL;
  next = NULL;
  _alpn_protos = NULL;
  _use_ca_bundle = false;
}

NetworkClientSecure::NetworkClientSecure(int sock) {
  _connected = false;
  _timeout = 30000;  // Same default as ssl_client
  _lastReadTimeout = 0;
  _lastWriteTimeout = 0;

  sslclient.reset(new sslclient_context, [](struct sslclient_context *sslclient) {
    stop_ssl_socket(sslclient);
    delete sslclient;
  });
  ssl_init(sslclient.get());
  sslclient->socket = sock;
  sslclient->handshake_timeout = 120000;

  if (sock >= 0) {
    _connected = true;
  }

  _CA_cert = NULL;
  _cert = NULL;
  _private_key = NULL;
  _pskIdent = NULL;
  _psKey = NULL;
  next = NULL;
  _alpn_protos = NULL;
}

NetworkClientSecure::~NetworkClientSecure() {}

void NetworkClientSecure::stop() {
  stop_ssl_socket(sslclient.get());

  _connected = false;
  sslclient->peek_buf = -1;
  _lastReadTimeout = 0;
  _lastWriteTimeout = 0;
}

int NetworkClientSecure::connect(IPAddress ip, uint16_t port) {
  if (_pskIdent && _psKey) {
    return connect(ip, port, _pskIdent, _psKey);
  }
  return connect(ip, port, _CA_cert, _cert, _private_key);
}

int NetworkClientSecure::connect(IPAddress ip, uint16_t port, int32_t timeout) {
  _timeout = timeout;
  return connect(ip, port);
}

int NetworkClientSecure::connect(const char *host, uint16_t port) {
  if (_pskIdent && _psKey) {
    return connect(host, port, _pskIdent, _psKey);
  }
  return connect(host, port, _CA_cert, _cert, _private_key);
}

int NetworkClientSecure::connect(const char *host, uint16_t port, int32_t timeout) {
  _timeout = timeout;
  return connect(host, port);
}

int NetworkClientSecure::connect(IPAddress ip, uint16_t port, const char *CA_cert, const char *cert, const char *private_key) {
  return connect(ip, port, NULL, CA_cert, cert, private_key);
}

int NetworkClientSecure::connect(const char *host, uint16_t port, const char *CA_cert, const char *cert, const char *private_key) {
  IPAddress address;
  if (!Network.hostByName(host, address)) {
    return 0;
  }

  return connect(address, port, host, CA_cert, cert, private_key);
}

int NetworkClientSecure::connect(IPAddress ip, uint16_t port, const char *host, const char *CA_cert, const char *cert, const char *private_key) {
  int ret = start_ssl_client(sslclient.get(), ip, port, host, _timeout, CA_cert, _use_ca_bundle, cert, private_key, NULL, NULL, _use_insecure, _alpn_protos);

  if (ret >= 0 && !_stillinPlainStart) {
    ret = ssl_starttls_handshake(sslclient.get());
  } else {
    log_i("Actual TLS start postponed.");
  }

  sslclient->last_error = ret;

  if (ret < 0) {
    log_e("start_ssl_client: connect failed: %d", ret);
    stop();
    return 0;
  }
  _connected = true;
  return 1;
}

int NetworkClientSecure::startTLS() {
  int ret = 1;
  if (_stillinPlainStart) {
    log_i("startTLS: starting TLS/SSL on this dplain connection");
    ret = ssl_starttls_handshake(sslclient.get());
    if (ret < 0) {
      log_e("startTLS: %d", ret);
      stop();
      return 0;
    };
    _stillinPlainStart = false;
  } else {
    log_i("startTLS: ignoring StartTLS - as we should be secure already");
  }
  return 1;
}

int NetworkClientSecure::connect(IPAddress ip, uint16_t port, const char *pskIdent, const char *psKey) {
  return connect(ip.toString().c_str(), port, pskIdent, psKey);
}

int NetworkClientSecure::connect(const char *host, uint16_t port, const char *pskIdent, const char *psKey) {
  log_v("start_ssl_client with PSK");

  IPAddress address;
  if (!Network.hostByName(host, address)) {
    return 0;
  }

  int ret = start_ssl_client(sslclient.get(), address, port, host, _timeout, NULL, false, NULL, NULL, pskIdent, psKey, _use_insecure, _alpn_protos);
  sslclient->last_error = ret;
  if (ret < 0) {
    log_e("start_ssl_client: connect failed %d", ret);
    stop();
    return 0;
  }
  _connected = true;
  return 1;
}

int NetworkClientSecure::peek() {
  if (sslclient->peek_buf >= 0) {
    return sslclient->peek_buf;
  }
  sslclient->peek_buf = timedRead();
  return sslclient->peek_buf;
}

size_t NetworkClientSecure::write(uint8_t data) {
  return write(&data, 1);
}

int NetworkClientSecure::read() {
  uint8_t data = -1;
  int res = read(&data, 1);
  return res < 0 ? res : data;
}

size_t NetworkClientSecure::write(const uint8_t *buf, size_t size) {
  if (!_connected) {
    return 0;
  }

  if (_stillinPlainStart) {
    return send_net_data(sslclient.get(), buf, size);
  }

  if (_lastWriteTimeout != _timeout) {
    struct timeval timeout_tv;
    timeout_tv.tv_sec = _timeout / 1000;
    timeout_tv.tv_usec = (_timeout % 1000) * 1000;
    if (setSocketOption(SO_SNDTIMEO, (char *)&timeout_tv, sizeof(struct timeval)) >= 0) {
      _lastWriteTimeout = _timeout;
    }
  }
  int res = send_ssl_data(sslclient.get(), buf, size);
  if (res < 0) {
    log_e("Closing connection on failed write");
    stop();
    res = 0;
  }
  return res;
}

int NetworkClientSecure::read(uint8_t *buf, size_t size) {
  if (_stillinPlainStart) {
    return get_net_receive(sslclient.get(), buf, size);
  }

  if (_lastReadTimeout != _timeout) {
    if (fd() >= 0) {
      struct timeval timeout_tv;
      timeout_tv.tv_sec = _timeout / 1000;
      timeout_tv.tv_usec = (_timeout % 1000) * 1000;
      if (setSocketOption(SO_RCVTIMEO, (char *)&timeout_tv, sizeof(struct timeval)) >= 0) {
        _lastReadTimeout = _timeout;
      }
    }
  }

  int peeked = 0, res = -1;
  int avail = available();
  if ((!buf && size) || avail <= 0) {
    return -1;
  }
  if (!size) {
    return 0;
  }
  if (sslclient->peek_buf >= 0) {
    buf[0] = sslclient->peek_buf;
    sslclient->peek_buf = -1;
    size--;
    avail--;
    if (!size || !avail) {
      return 1;
    }
    buf++;
    peeked = 1;
  }
  res = get_ssl_receive(sslclient.get(), buf, size);

  if (res < 0) {
    log_e("Closing connection on failed read");
    stop();
    return peeked ? peeked : res;
  }
  return res + peeked;
}

int NetworkClientSecure::available() {
  if (_stillinPlainStart) {
    return peek_net_receive(sslclient.get(), 0);
  }

  int peeked = (sslclient->peek_buf >= 0), res = -1;
  if (!_connected) {
    return peeked;
  }
  res = data_to_read(sslclient.get());

  if (res < 0 && !_stillinPlainStart) {
    log_e("Closing connection on failed available check");
    stop();
    return peeked ? peeked : res;
  }
  return res + peeked;
}

uint8_t NetworkClientSecure::connected() {
  uint8_t dummy = 0;
  read(&dummy, 0);

  return _connected;
}

void NetworkClientSecure::setInsecure() {
  _CA_cert = NULL;
  _cert = NULL;
  _private_key = NULL;
  _pskIdent = NULL;
  _psKey = NULL;
  _use_insecure = true;
}

void NetworkClientSecure::setCACert(const char *rootCA) {
  _CA_cert = rootCA;
  _use_insecure = false;
}

void NetworkClientSecure::setCACertBundle(const uint8_t *bundle) {
  if (bundle != NULL) {
    esp_crt_bundle_set(bundle, sizeof(bundle));
    _use_ca_bundle = true;
  } else {
    esp_crt_bundle_detach(NULL);
    _use_ca_bundle = false;
  }
}

void NetworkClientSecure::setCertificate(const char *client_ca) {
  _cert = client_ca;
}

void NetworkClientSecure::setPrivateKey(const char *private_key) {
  _private_key = private_key;
}

void NetworkClientSecure::setPreSharedKey(const char *pskIdent, const char *psKey) {
  _pskIdent = pskIdent;
  _psKey = psKey;
}

bool NetworkClientSecure::verify(const char *fp, const char *domain_name) {
  if (!sslclient) {
    return false;
  }

  return verify_ssl_fingerprint(sslclient.get(), fp, domain_name);
}

char *NetworkClientSecure::_streamLoad(Stream &stream, size_t size) {
  char *dest = (char *)malloc(size + 1);
  if (!dest) {
    return nullptr;
  }
  if (size != stream.readBytes(dest, size)) {
    free(dest);
    dest = nullptr;
    return nullptr;
  }
  dest[size] = '\0';
  return dest;
}

bool NetworkClientSecure::loadCACert(Stream &stream, size_t size) {
  if (_CA_cert != NULL) {
    free(const_cast<char *>(_CA_cert));
  }
  char *dest = _streamLoad(stream, size);
  bool ret = false;
  if (dest) {
    setCACert(dest);
    ret = true;
  }
  return ret;
}

bool NetworkClientSecure::loadCertificate(Stream &stream, size_t size) {
  if (_cert != NULL) {
    free(const_cast<char *>(_cert));
  }
  char *dest = _streamLoad(stream, size);
  bool ret = false;
  if (dest) {
    setCertificate(dest);
    ret = true;
  }
  return ret;
}

bool NetworkClientSecure::loadPrivateKey(Stream &stream, size_t size) {
  if (_private_key != NULL) {
    free(const_cast<char *>(_private_key));
  }
  char *dest = _streamLoad(stream, size);
  bool ret = false;
  if (dest) {
    setPrivateKey(dest);
    ret = true;
  }
  return ret;
}

int NetworkClientSecure::lastError(char *buf, const size_t size) {
  int lastError = sslclient->last_error;
  mbedtls_strerror(lastError, buf, size);
  return lastError;
}

void NetworkClientSecure::setHandshakeTimeout(unsigned long handshake_timeout) {
  sslclient->handshake_timeout = handshake_timeout * 1000;
}

void NetworkClientSecure::setAlpnProtocols(const char **alpn_protos) {
  _alpn_protos = alpn_protos;
}

int NetworkClientSecure::fd() const {
  return sslclient->socket;
}
