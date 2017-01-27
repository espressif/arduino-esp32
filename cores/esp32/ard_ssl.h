/* Provide SSL/TLS functions to ESP32 with Arduino IDE
 * Ported from Ameba8195 (no license specified) to ESP32 by Evandro Copercini - 2017 - Public domain
 */
 
#ifndef ARD_SSL_H
#define ARD_SSL_H

#include "mbedtls/net.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"

typedef struct sslclient_context {
	int fd;
    mbedtls_net_context net_ctx;
    mbedtls_ssl_context ssl_ctx;
    mbedtls_ssl_config ssl_conf;

    mbedtls_ctr_drbg_context drbg_ctx;
    mbedtls_entropy_context entropy_ctx;

	mbedtls_x509_crt ca_cert;
	mbedtls_x509_crt client_cert;
	mbedtls_pk_context client_key;
} sslclient_context;


void ssl_init(sslclient_context* ssl_client);
	
int start_ssl_client(sslclient_context *ssl_client, uint32_t ipAddress, uint32_t port, unsigned char* rootCABuff, unsigned char* cli_cert, unsigned char* cli_key);

void stop_ssl_socket(sslclient_context *ssl_client);

int send_ssl_data(sslclient_context *ssl_client, const uint8_t *data, uint16_t len);

int get_ssl_receive(sslclient_context *ssl_client, uint8_t* data, int length, int flag);

int get_ssl_sock_errno(sslclient_context *ssl_client);

int get_ssl_bytes_avail(sslclient_context *ssl_client);

#endif
