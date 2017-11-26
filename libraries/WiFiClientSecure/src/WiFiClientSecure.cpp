/*
  WiFiClientSecure.cpp - Client Secure class for ESP32
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

#include "WiFiClientSecure.h"
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <errno.h>

#undef connect
#undef write
#undef read


WiFiClientSecure::WiFiClientSecure()
{
    _connected = false;

    sslclient = new sslclient_context;
    ssl_init(sslclient);
    sslclient->socket = -1;

    _CA_cert = NULL;
    _cert = NULL;
    _private_key = NULL;
	next = NULL;			
}


WiFiClientSecure::WiFiClientSecure(int sock)
{
    _connected = false;

    sslclient = new sslclient_context;
    ssl_init(sslclient);
    sslclient->socket = sock;

    if (sock >= 0) {
        _connected = true;
    }

    _CA_cert = NULL;
    _cert = NULL;
    _private_key = NULL;
    next = NULL;				
}

WiFiClientSecure::~WiFiClientSecure()
{
    stop();
	delete sslclient;				  
}

WiFiClientSecure &WiFiClientSecure::operator=(const WiFiClientSecure &other)
{
    stop();
    sslclient->socket = other.sslclient->socket;
    _connected = other._connected;
    return *this;
}

void WiFiClientSecure::stop()
{
    if (sslclient->socket >= 0) {
        close(sslclient->socket);
        sslclient->socket = -1;
        _connected = false;
    }
    stop_ssl_socket(sslclient, _CA_cert, _cert, _private_key);
}

int WiFiClientSecure::connect(IPAddress ip, uint16_t port)
{
    return connect(ip, port, _CA_cert, _cert, _private_key);
}

int WiFiClientSecure::connect(const char *host, uint16_t port)
{
    return connect(host, port, _CA_cert, _cert, _private_key);
}

int WiFiClientSecure::connect(IPAddress ip, uint16_t port, const char *_CA_cert, const char *_cert, const char *_private_key)
{
    return connect(ip.toString().c_str(), port, _CA_cert, _cert, _private_key);
}

int WiFiClientSecure::connect(const char *host, uint16_t port, const char *_CA_cert, const char *_cert, const char *_private_key)
{
    int ret = start_ssl_client(sslclient, host, port, _CA_cert, _cert, _private_key);
    if (ret < 0) {
        log_e("lwip_connect_r: %d", errno);
        stop();
        return 0;
    }
    _connected = true;
    return 1;
}

size_t WiFiClientSecure::write(uint8_t data)
{
    return write(&data, 1);
}

int WiFiClientSecure::read()
{
    uint8_t data = 0;
    int res = read(&data, 1);
    if (res < 0) {
        return res;
    }
    return data;
}

size_t WiFiClientSecure::write(const uint8_t *buf, size_t size)
{
    if (!_connected) {
        return 0;
    }
    int res = send_ssl_data(sslclient, buf, size);
    if (res < 0) {
						   
        stop();
        res = 0;
    }
    return res;
}

int WiFiClientSecure::read(uint8_t *buf, size_t size)
{
    if (!available()) {
        return -1;
    }
    int res = get_ssl_receive(sslclient, buf, size);
    if (res < 0) {
							
        stop();
    }
    return res;
}

int WiFiClientSecure::available()
{
    if (!_connected) {
        return 0;
    }
    int res = data_to_read(sslclient);
    if (res < 0 ) {
        stop();
    }	
    return res;
}

uint8_t WiFiClientSecure::connected()
{
    uint8_t dummy = 0;
    read(&dummy, 0);

    return _connected;
}

void WiFiClientSecure::setCACert (const char *rootCA)
{
    _CA_cert = rootCA;
}

void WiFiClientSecure::setCertificate (const char *client_ca)
{
    _cert = client_ca;
}

void WiFiClientSecure::setPrivateKey (const char *private_key)
{
    _private_key = private_key;
}

