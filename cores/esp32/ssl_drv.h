/* Provide SSL/TLS functions to ESP32 with Arduino IDE
 * Ported from Ameba8195 (no license specified) to ESP32 by Evandro Copercini - 2017 - Public domain
 */
 
#ifndef SSL_DRV_H
#define SSL_DRV_H
#include <inttypes.h>

#define	DATA_LENTH		128
#ifdef __cplusplus
extern "C" {
#include "ard_ssl.h"
}
#endif

class SSLDrv
{
public:
	int startClient(sslclient_context *ssl_client, uint32_t ipAddress, uint32_t port, unsigned char* rootCABuff, unsigned char* cli_cert, unsigned char* cli_key);
	void stopClient(sslclient_context *ssl_client);
	bool getData(sslclient_context *ssl_client, uint8_t *data, uint8_t peek=0);
	int getDataBuf(sslclient_context *ssl_client, uint8_t *_data, uint16_t _dataLen);
	bool sendData(sslclient_context *ssl_client, const uint8_t *data, uint16_t len);
	uint16_t availData(sslclient_context *ssl_client);
	sslclient_context *init(void);
    int getLastErrno(sslclient_context *ssl_client);

private:    	
	bool _available;
	uint8_t c[1];
};

#endif
