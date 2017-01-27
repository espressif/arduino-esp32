/* Provide SSL/TLS functions to ESP32 with Arduino IDE
 * Ported from Ameba8195 to ESP32 by Evandro Copercini - 2017 - Public domain
 */

#ifndef wifisslclient_h
#define wifisslclient_h
#include "Wifi.h"
#include "IPAddress.h"
#include "ssl_drv.h"

struct ssl_context;
class WiFiSSLClient : public Client {

public:

  WiFiSSLClient();
  WiFiSSLClient(uint8_t sock);

  uint8_t status();
  virtual int connect(IPAddress ip, uint16_t port);
  virtual int connect(const char *host, uint16_t port);
  virtual size_t write(uint8_t);
  virtual size_t write(const uint8_t *buf, size_t size);
  virtual int available();
  virtual int read();
  virtual int read(uint8_t *buf, size_t size);
  virtual int peek();
  virtual void flush();
  virtual void stop();
  virtual uint8_t connected();
  virtual operator bool();

  void setRootCA(unsigned char *rootCA);
  void setClientCertificate(unsigned char *client_ca, unsigned char *private_key);

  int connect(IPAddress ip, uint16_t port, unsigned char* rootCABuff, unsigned char* cli_cert, unsigned char* cli_key);
  int connect(const char *host, uint16_t port, unsigned char* rootCABuff, unsigned char* cli_cert, unsigned char* cli_key);

  using Print::write;

private:
  int _sock;
  bool _is_connected;
  sslclient_context* sslclient;
  SSLDrv ssldrv;

  unsigned char *_rootCABuff;
  unsigned char *_cli_cert;
  unsigned char *_cli_key;
};

#endif
