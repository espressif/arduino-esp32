/* Provide SSL/TLS functions to ESP32 with Arduino IDE
 * by Evandro Copercini - 2017 - Public domain
 */
#include "WiFi.h"
#include "WiFiSSLClient.h"


WiFiSSLClient::WiFiSSLClient(){
    _connected = false;

	sslclient = new sslclient_context;
	ssl_init(sslclient);
	sslclient->fd = -1;

    _CA_cert = NULL;
    _cert = NULL;
    _private_key = NULL;
}

WiFiSSLClient::WiFiSSLClient(uint8_t sock) {

	sslclient = new sslclient_context;
	ssl_init(sslclient);
	sslclient->fd = -1;

    if(sock >= 0)
        _connected = true;
	
    _CA_cert = NULL;
    _cert = NULL;
    _private_key = NULL;
}

uint8_t WiFiSSLClient::connected() {
  	if (sslclient->fd < 0) {
		_connected = false;
    	return 0;
  	}
	else {
		if(_connected)
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

	if(!_connected)
		return 0;
  	if (sslclient->fd >= 0)
  	{	
      	ret = availData(sslclient);
		if (ret > 0) {
            return 1;
        } else {
            printf("SSLclient: availData ERROR");
			_connected = false;
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

    ret = getData(sslclient, b);
    if (ret > 0) {
  		return b[0];
	} else {
        printf("SSLclient: read ERROR");
            _connected = false;
	}
	return -1;
}

int WiFiSSLClient::read(uint8_t* buf, size_t size) {
  	uint16_t _size = size;
	int ret;
    int err;

	ret = getDataBuf(sslclient, buf, _size);
  	if (ret <= 0){
        printf("SSLclient: read ERROR");
            _connected = false;
  	}
  	return ret;
}

void WiFiSSLClient::stop() {

  	if (sslclient->fd < 0)
    	return;

  	stopClient(sslclient);
	_connected = false;
	
  	sslclient->fd = -1;
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

  	if (!sendData(sslclient, buf, size))
  	{
  	    setWriteError();
		_connected = false;
      	return 0;
  	}
	
  	return size;
}

WiFiSSLClient::operator bool() {
  	return sslclient->fd >= 0;
}

int WiFiSSLClient::connect(IPAddress ip, uint16_t port) {
    connect(ip, port, _CA_cert, _cert, _private_key);
}

int WiFiSSLClient::connect(const char *host, uint16_t port) {
    connect(host, port, _CA_cert, _cert, _private_key);
}


int WiFiSSLClient::connect(const char* host, uint16_t port, unsigned char* CA_cert, unsigned char* cert, unsigned char* private_key) {
	IPAddress remote_addr;
	
	if (WiFi.hostByName(host, remote_addr))
	{
		return connect(remote_addr, port, CA_cert, cert, private_key);
	}
	return 0;
}

int WiFiSSLClient::connect(IPAddress ip, uint16_t port, unsigned char* CA_cert, unsigned char* cert, unsigned char* private_key) {
	int ret = 0;
	
	ret = startClient(sslclient, ip, port, CA_cert, cert, private_key);

    if (ret < 0) {
        _connected = false;
        return 0;
    } else {
        _connected = true;
    }

    return 1;
}

int WiFiSSLClient::peek() {
	uint8_t b;

	if (!available())
		return -1;

	getData(sslclient, &b, 1);

	return b;
}
void WiFiSSLClient::flush() {
	while (available())
		read();
}

void WiFiSSLClient::setCACert(unsigned char *rootCA) {
    _CA_cert = rootCA;
}

void WiFiSSLClient::setCertificate (unsigned char *client_ca){
    _cert = client_ca;
}
	
void WiFiSSLClient::setPrivateKey (unsigned char *private_key) {
    _private_key = private_key;
}


uint16_t WiFiSSLClient::availData(sslclient_context *ssl_client)
{
	int ret;

	if (ssl_client->fd < 0)		
		return 0;
	
	if(_available) {
		return 1;
	} else {
	    return getData(ssl_client, c, 1);
	}
}

bool WiFiSSLClient::getData(sslclient_context *ssl_client, uint8_t *data, uint8_t peek)
{
	int ret = 0;
	int flag = 0;

    if (_available) {
        /* we already has data to read */
        data[0] = c[0];
        if (peek) {
        } else {
            /* It's not peek and the data has been taken */
            _available = false;
        }
        return true;
    }

    if (peek) {
        flag |= 1;
    }

	ret = get_ssl_receive(ssl_client, c, 1, flag);

	if (ret == 1) {
        data[0] = c[0];
        if (peek) {
            _available = true;
        } else {
            _available = false;
        }
		return true;
	}
 
	return false;
}

int WiFiSSLClient::getDataBuf(sslclient_context *ssl_client, uint8_t *_data, uint16_t _dataLen)
{
	int ret;

    if (_available) {
        /* there is one byte cached */
        _data[0] = c[0];
        _available = false;
        _dataLen--;
        if (_dataLen > 0) {
            ret = get_ssl_receive(ssl_client, _data, _dataLen, 0);
            if (ret > 0) {
                ret++;
                return ret;
            } else {
                return 1;
            }
        } else {
            return 1;
        }
    } else {
        ret = get_ssl_receive(ssl_client, _data, _dataLen, 0);
    }

	return ret;
}

void WiFiSSLClient::stopClient(sslclient_context *ssl_client)
{
	stop_ssl_socket(ssl_client);
    _available = false;
}

bool WiFiSSLClient::sendData(sslclient_context *ssl_client, const uint8_t *data, uint16_t len)
{
    int ret;

    if (ssl_client->fd < 0)
        return false;        	

    ret = send_ssl_data(ssl_client, data, len);

    if (ret == 0) {  
        return false;
    }

    return true;
}


int WiFiSSLClient::startClient(sslclient_context *ssl_client, uint32_t ipAddress, uint32_t port, unsigned char* CA_cert, unsigned char* cert, unsigned char* private_key)
{
    int ret;

    ret = start_ssl_client(ssl_client, ipAddress, port, CA_cert, cert, private_key);

    return ret;
}
