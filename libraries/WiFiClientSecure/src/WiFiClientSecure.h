/* Provide SSL/TLS functions to ESP32 with Arduino IDE
 * by Evandro Copercini - 2017 - Apache 2.0 License
 */

#ifndef WiFiClientSecure_h
#define WiFiClientSecure_h
//#include "Wifi.h"
#include "IPAddress.h"

#include "ssl_client.h"

struct  ssl_context;

class WiFiClientSecure : public WiFiClient {

public:

	WiFiClientSecure();
	WiFiClientSecure(uint8_t sock);

	virtual int connect(IPAddress ip, uint16_t port);
	virtual int connect(const char *host, uint16_t port);
	virtual size_t write(uint8_t data );
	virtual size_t write(const uint8_t *buf, size_t size);
	virtual int available();
	virtual int read();
	virtual int read(uint8_t *buf, size_t size);
	virtual int peek();
	virtual void flush();
	virtual void stop();
	virtual uint8_t connected();
	virtual operator bool();

	void setCACert(unsigned char *rootCA);
	void setCertificate(unsigned char *client_ca);
	void setPrivateKey (unsigned char *private_key);

	int connect(IPAddress ip, uint16_t port, unsigned char* rootCABuff, unsigned char* cli_cert, unsigned char* cli_key);
	int connect(const char *host, uint16_t port, unsigned char* rootCABuff, unsigned char* cli_cert, unsigned char* cli_key);

	int startClient(sslclient_context *ssl_client, uint32_t ipAddress, uint32_t port, unsigned char* rootCABuff, unsigned char* cli_cert, unsigned char* cli_key);
	void stopClient(sslclient_context *ssl_client, unsigned char* rootCABuff, unsigned char* _cert, unsigned char* _private_key);
	bool getData(sslclient_context *ssl_client, uint8_t *data, uint8_t peek=0);
	int getDataBuf(sslclient_context *ssl_client, uint8_t *_data, uint16_t _dataLen);
	bool sendData(sslclient_context *ssl_client, const uint8_t *data, uint16_t len);
	uint16_t availData(sslclient_context *ssl_client);

	using Print::write;

private:
	bool _connected;
	sslclient_context* sslclient;

	unsigned char *_CA_cert;
	unsigned char *_cert;
	unsigned char *_private_key;

	bool _available;
	uint8_t c[1];

};
#endif
