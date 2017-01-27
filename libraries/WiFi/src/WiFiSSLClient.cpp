/* Provide SSL/TLS functions to ESP32 with Arduino IDE
 * Ported from Ameba8195 (no license specified) to ESP32 by Evandro Copercini - 2017 - Public domain
 */
#include "WiFi.h"
#include "WiFiSSLClient.h"

WiFiSSLClient::WiFiSSLClient(){
    _is_connected = false;
	_sock = -1;

	sslclient = new sslclient_context;
	ssl_init(sslclient);
	sslclient->fd = -1;

    _rootCABuff = NULL;
    _cli_cert = NULL;
    _cli_key = NULL;
}

WiFiSSLClient::WiFiSSLClient(uint8_t sock) {
    _sock = sock;

	sslclient = new sslclient_context;
	ssl_init(sslclient);
	sslclient->fd = -1;

    if(sock >= 0)
        _is_connected = true;
    _rootCABuff = NULL;
    _cli_cert = NULL;
    _cli_key = NULL;
}

uint8_t WiFiSSLClient::connected() {
  	if (sslclient->fd < 0) {
		_is_connected = false;
    	return 0;
  	}
	else {
		if(_is_connected)
			return 1;
		else{
			stop();
			return 0;
		}
	}
}

int WiFiSSLClient::available() {
	int ret = 0;
    int err;

	if(!_is_connected)
		return 0;
  	if (sslclient->fd >= 0)
  	{	
      	ret = ssldrv.availData(sslclient);
		if (ret > 0) {
            return 1;
        } else {
            err = ssldrv.getLastErrno(sslclient);
            if (err != EAGAIN) {
			    _is_connected = false;
            }
		}
		return 0;
  	}
}

int WiFiSSLClient::read() {
    int ret;
    int err;
  	uint8_t b[1];
	
  	if (!available())
    	return -1;

    ret = ssldrv.getData(sslclient, b);
    if (ret > 0) {
  		return b[0];
	} else {
        err = ssldrv.getLastErrno(sslclient);
        if (err != EAGAIN) {
            _is_connected = false;
        }
	}
	return -1;
}

int WiFiSSLClient::read(uint8_t* buf, size_t size) {
  	uint16_t _size = size;
	int ret;
    int err;

	ret = ssldrv.getDataBuf(sslclient, buf, _size);
  	if (ret <= 0){
        err = ssldrv.getLastErrno(sslclient);
        if (err != EAGAIN) {
            _is_connected = false;
        }
  	}
  	return ret;
}

void WiFiSSLClient::stop() {

  	if (sslclient->fd < 0)
    	return;

  	ssldrv.stopClient(sslclient);
	_is_connected = false;
	
  	sslclient->fd = -1;
	_sock = -1;
}

size_t WiFiSSLClient::write(uint8_t b) {
	  return write(&b, 1);
}

size_t WiFiSSLClient::write(const uint8_t *buf, size_t size) {
  	if (sslclient->fd < 0)
  	{
  	    setWriteError();
	  	return 0;
  	}
  	if (size == 0)
  	{
  	    setWriteError();
      	return 0;
  	}

  	if (!ssldrv.sendData(sslclient, buf, size))
  	{
  	    setWriteError();
		_is_connected = false;
      	return 0;
  	}
	
  	return size;
}

WiFiSSLClient::operator bool() {
  	return sslclient->fd >= 0;
}

int WiFiSSLClient::connect(IPAddress ip, uint16_t port) {
    //
}

int WiFiSSLClient::connect(const char *host, uint16_t port) {
    connect(host, port, _rootCABuff, _cli_cert, _cli_key);
}


int WiFiSSLClient::connect(const char* host, uint16_t port, unsigned char* rootCABuff, unsigned char* cli_cert, unsigned char* cli_key) {
	IPAddress remote_addr;
	
	if (WiFi.hostByName(host, remote_addr))
	{
		return connect(remote_addr, port, rootCABuff, cli_cert, cli_key);
	}
	return 0;
}

int WiFiSSLClient::connect(IPAddress ip, uint16_t port, unsigned char* rootCABuff, unsigned char* cli_cert, unsigned char* cli_key) {
	int ret = 0;
	
	ret = ssldrv.startClient(sslclient, ip, port, rootCABuff, cli_cert, cli_key);

    if (ret < 0) {
        _is_connected = false;
        return 0;
    } else {
        _is_connected = true;
    }

    return 1;
}

int WiFiSSLClient::peek() {
	uint8_t b;

	if (!available())
		return -1;

	ssldrv.getData(sslclient, &b, 1);

	return b;
}
void WiFiSSLClient::flush() {
	while (available())
		read();
}

void WiFiSSLClient::setRootCA(unsigned char *rootCA) {
    _rootCABuff = rootCA;
}

void WiFiSSLClient::setClientCertificate(unsigned char *client_ca, unsigned char *private_key) {
    _cli_cert = client_ca;
    _cli_key = private_key;
}

