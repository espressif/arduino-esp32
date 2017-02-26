/*
  Client.h - Client class for Raspberry Pi
  Copyright (c) 2016 Hristo Gochkov  All right reserved.

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

#include "WiFiClient.h"
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <errno.h>

#undef connect
#undef write
#undef read

#define WIFI_CLIENT_SELECT_TIMEOUT_US (100000)

WiFiClientSocketHandle::~WiFiClientSocketHandle()
{
    close(sockfd);
}

WiFiClient::WiFiClient():_connected(false),next(NULL)
{
    clientSocketHandle = NULL;
}

WiFiClient::WiFiClient(int fd):_connected(true),next(NULL)
{
    clientSocketHandle = new WiFiClientSocketHandle(fd);
}

WiFiClient::~WiFiClient()
{
    stop();
}

WiFiClient & WiFiClient::operator=(const WiFiClient &other)
{
    stop();
    clientSocketHandle = other.clientSocketHandle;
    if (clientSocketHandle != NULL) {
        clientSocketHandle->ref();
    }
    _connected = other._connected;
    return *this;
}

WiFiClient::WiFiClient(const WiFiClient &other)
{
    clientSocketHandle = other.clientSocketHandle;
    if (clientSocketHandle != NULL) {
        clientSocketHandle->ref();
    }
    _connected = other._connected;
    next = other.next;
}

void WiFiClient::stop()
{
    if (clientSocketHandle && clientSocketHandle->unref() == 0) {
        delete clientSocketHandle;
    }
    clientSocketHandle = NULL;
    _connected = false;
}

int WiFiClient::connect(IPAddress ip, uint16_t port)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        log_e("socket: %d", errno);
        return 0;
    }

    //If there is an exisiting socket, unreference it
    if (clientSocketHandle && clientSocketHandle->unref() == 0) {
        delete clientSocketHandle;
    }
    clientSocketHandle = new WiFiClientSocketHandle(sockfd);

    uint32_t ip_addr = ip;
    struct sockaddr_in serveraddr;
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((const void *)(&ip_addr), (void *)&serveraddr.sin_addr.s_addr, 4);
    serveraddr.sin_port = htons(port);
    int res = lwip_connect_r(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    if (res < 0) {
        log_e("lwip_connect_r: %d", errno);
        delete clientSocketHandle;
        clientSocketHandle = NULL;
        return 0;
    }
    _connected = true;
    return 1;
}

int WiFiClient::connect(const char *host, uint16_t port)
{
    struct hostent *server;
    server = gethostbyname(host);
    if (server == NULL) {
        return 0;
    }
    IPAddress srv((const uint8_t *)(server->h_addr));
    return connect(srv, port);
}

int WiFiClient::setSocketOption(int option, char* value, size_t len)
{
    if (clientSocketHandle == NULL) {
        return -1;
    }
    int res = setsockopt(fd(), SOL_SOCKET, option, value, len);
    if(res < 0) {
        log_e("%d", errno);
    }
    return res;
}

int WiFiClient::setTimeout(uint32_t seconds)
{
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    if(setSocketOption(SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval)) < 0) {
        return -1;
    }
    return setSocketOption(SO_SNDTIMEO, (char *)&tv, sizeof(struct timeval));
}

int WiFiClient::setOption(int option, int *value)
{
    if (clientSocketHandle == NULL) {
        return -1;
    }
    int res = setsockopt(fd(), IPPROTO_TCP, option, (char *) value,
            sizeof(int));
    if (res < 0) {
        log_e("%d", errno);
    }
    return res;
}

int WiFiClient::getOption(int option, int *value)
{
    if (clientSocketHandle == NULL) {
        return -1;
    }
    size_t size = sizeof(int);
    int res = getsockopt(fd(), IPPROTO_TCP, option, (char *)value, &size);
    if(res < 0) {
        log_e("%d", errno);
    }
    return res;
}

int WiFiClient::setNoDelay(bool nodelay)
{
    int flag = nodelay;
    return setOption(TCP_NODELAY, &flag);
}

bool WiFiClient::getNoDelay()
{
    int flag = 0;
    getOption(TCP_NODELAY, &flag);
    return flag;
}

size_t WiFiClient::write(uint8_t data)
{
    return write(&data, 1);
}

int WiFiClient::read()
{
    uint8_t data = 0;
    int res = read(&data, 1);
    if(res < 0) {
        return res;
    }
    return data;
}

size_t WiFiClient::write(const uint8_t *buf, size_t size)
{
    int res =0;
    bool retry = true;

    if (!_connected || clientSocketHandle == NULL) {
        return 0;
    }

    int sockfd = fd();

    while (retry) {
        //use select to make sure the socket is ready for writing
        fd_set set;
        struct timeval tv;
        FD_ZERO(&set); /* empties the set */
        FD_SET(sockfd, &set); /* adds FD to the set */
        tv.tv_sec = 0;
        tv.tv_usec = WIFI_CLIENT_SELECT_TIMEOUT_US;

        if (select(sockfd + 1, NULL, &set, NULL, &tv) < 0) {
            return 0;
        }

        if (FD_ISSET(sockfd, &set)) {
            res = send(sockfd, (void*) buf, size, MSG_DONTWAIT);
            if (res < 0) {
                log_e("%d", errno);
                Serial.print("Write Error: ");
                Serial.println(errno);
                if (errno != EAGAIN) {
                    //if resource was busy, can try again, otherwise give up
                    stop();
                    res = 0;
                    retry = false;
                }
            } else {
                //completed successfully
                retry = false;
            }
        }
    }
    return res;
}

int WiFiClient::read(uint8_t *buf, size_t size)
{
    if (!available() || clientSocketHandle == NULL) {
        return -1;
    }
    int res = recv(fd(), buf, size, MSG_DONTWAIT);
    if(res < 0 && errno != EWOULDBLOCK) {
        log_e("%d", errno);
        stop();
    }
    return res;
}

int WiFiClient::available()
{
    if (!_connected || clientSocketHandle == NULL) {
        return 0;
    }
    int count;
    int res = ioctl(fd(), FIONREAD, &count);
    if(res < 0) {
        log_e("%d", errno);
        stop();
        return 0;
    }
    return count;
}

uint8_t WiFiClient::connected()
{
    uint8_t dummy = 0;
    read(&dummy, 0);
    return _connected;
}

IPAddress WiFiClient::remoteIP(int fd) const
{
    struct sockaddr_storage addr;
    socklen_t len = sizeof addr;
    getpeername(fd, (struct sockaddr*)&addr, &len);
    struct sockaddr_in *s = (struct sockaddr_in *)&addr;
    return IPAddress((uint32_t)(s->sin_addr.s_addr));
}

uint16_t WiFiClient::remotePort(int fd) const
{
    struct sockaddr_storage addr;
    socklen_t len = sizeof addr;
    getpeername(fd, (struct sockaddr*)&addr, &len);
    struct sockaddr_in *s = (struct sockaddr_in *)&addr;
    return ntohs(s->sin_port);
}

IPAddress WiFiClient::remoteIP() const
{
    return remoteIP(fd());
}

uint16_t WiFiClient::remotePort() const
{
    return remotePort(fd());
}

bool WiFiClient::operator==(const WiFiClient& rhs)
{
    return clientSocketHandle == rhs.clientSocketHandle && remotePort() == rhs.remotePort() && remoteIP() == rhs.remoteIP();
}
