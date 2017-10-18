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

#define WIFI_CLIENT_MAX_WRITE_RETRY   (10)
#define WIFI_CLIENT_SELECT_TIMEOUT_US (1000000)
#define WIFI_CLIENT_FLUSH_BUFFER_SIZE (1024)

#undef connect
#undef write
#undef read

class WiFiClientSocketHandle {
private:
    int sockfd;

public:
    WiFiClientSocketHandle(int fd):sockfd(fd)
    {
    }

    ~WiFiClientSocketHandle()
    {
        close(sockfd);
    }

    int fd()
    {
        return sockfd;
    }
};

WiFiClient::WiFiClient():_connected(false),next(NULL)
{
}

WiFiClient::WiFiClient(int fd):_connected(true),next(NULL)
{
    clientSocketHandle.reset(new WiFiClientSocketHandle(fd));
}

WiFiClient::~WiFiClient()
{
    stop();
}

WiFiClient & WiFiClient::operator=(const WiFiClient &other)
{
    stop();
    clientSocketHandle = other.clientSocketHandle;
    _connected = other._connected;
    return *this;
}

void WiFiClient::stop()
{
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

    uint32_t ip_addr = ip;
    struct sockaddr_in serveraddr;
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((const void *)(&ip_addr), (void *)&serveraddr.sin_addr.s_addr, 4);
    serveraddr.sin_port = htons(port);
    int res = lwip_connect_r(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
    if (res < 0) {
        log_e("lwip_connect_r: %d", errno);
        close(sockfd);
        return 0;
    }
    clientSocketHandle.reset(new WiFiClientSocketHandle(sockfd));
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
    int res = setsockopt(fd(), SOL_SOCKET, option, value, len);
    if(res < 0) {
        log_e("%X : %d", option, errno);
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
    int res = setsockopt(fd(), IPPROTO_TCP, option, (char *) value, sizeof(int));
    if(res < 0) {
        log_e("%d", errno);
    }
    return res;
}

int WiFiClient::getOption(int option, int *value)
{
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
    int retry = WIFI_CLIENT_MAX_WRITE_RETRY;
    int socketFileDescriptor = fd();
    size_t totalBytesSent = 0;
    size_t bytesRemaining = size;

    if(!_connected || (socketFileDescriptor < 0)) {
        return 0;
    }

    while(retry) {
        //use select to make sure the socket is ready for writing
        fd_set set;
        struct timeval tv;
        FD_ZERO(&set);        // empties the set
        FD_SET(socketFileDescriptor, &set); // adds FD to the set
        tv.tv_sec = 0;
        tv.tv_usec = WIFI_CLIENT_SELECT_TIMEOUT_US;
        retry--;

        if(select(socketFileDescriptor + 1, NULL, &set, NULL, &tv) < 0) {
            return 0;
        }

        if(FD_ISSET(socketFileDescriptor, &set)) {
            res = send(socketFileDescriptor, (void*) buf, bytesRemaining, MSG_DONTWAIT);
            if(res > 0) {
                totalBytesSent += res;
                if (totalBytesSent >= size) {
                    //completed successfully
                    retry = 0;
                } else {
                    buf += res;
                    bytesRemaining -= res;
                }
            }
            else if(res < 0) {
                log_e("%d", errno);
                if(errno != EAGAIN) {
                    //if resource was busy, can try again, otherwise give up
                    stop();
                    res = 0;
                    retry = 0;
                }
            }
            else {
                // Try again
            }
        }
    }
    return totalBytesSent;
}

size_t WiFiClient::write_P(PGM_P buf, size_t size)
{
    return write(buf, size);
}

int WiFiClient::read(uint8_t *buf, size_t size)
{
    if(!available()) {
        return -1;
    }
    int res = recv(fd(), buf, size, MSG_DONTWAIT);
    if(res < 0 && errno != EWOULDBLOCK) {
        log_e("%d", errno);
        stop();
    }
    return res;
}

int WiFiClient::peek()
{
    if(!available()) {
        return -1;
    }
    uint8_t data = 0;
    int res = recv(fd(), &data, 1, MSG_PEEK);
    if(res < 0 && errno != EWOULDBLOCK) {
        log_e("%d", errno);
        stop();
    }
    return data;
}

int WiFiClient::available()
{
    if(!_connected) {
        return 0;
    }
    int count;
    int res = lwip_ioctl_r(fd(), FIONREAD, &count);
    if(res < 0) {
        log_e("%d", errno);
        stop();
        return 0;
    }
    return count;
}

// Though flushing means to send all pending data,
// seems that in Arduino it also means to clear RX
void WiFiClient::flush() {
    int res;
    size_t a = available(), toRead = 0;
    if(!a){
        return;//nothing to flush
    }
    uint8_t * buf = (uint8_t *)malloc(WIFI_CLIENT_FLUSH_BUFFER_SIZE);
    if(!buf){
        return;//memory error
    }
    while(a){
        toRead = (a>WIFI_CLIENT_FLUSH_BUFFER_SIZE)?WIFI_CLIENT_FLUSH_BUFFER_SIZE:a;
        res = recv(fd(), buf, toRead, MSG_DONTWAIT);
        if(res < 0) {
            log_e("%d", errno);
            stop();
            break;
        }
        a -= res;
    }
    free(buf);
}

uint8_t WiFiClient::connected()
{
    if (_connected) {
        uint8_t dummy;
        int res = recv(fd(), &dummy, 0, MSG_DONTWAIT);
        if (res <= 0) {
            switch (errno) {
                case ENOTCONN:
                case EPIPE:
                case ECONNRESET:
                case ECONNREFUSED:
                case ECONNABORTED:
                    _connected = false;
                    break;
                default:
                    _connected = true;
                    break;
            }
        }
        else {
            // Should never happen since requested 0 bytes
            _connected = true;
        }
    }
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

IPAddress WiFiClient::localIP(int fd) const
{
    struct sockaddr_storage addr;
    socklen_t len = sizeof addr;
    getsockname(fd, (struct sockaddr*)&addr, &len);
    struct sockaddr_in *s = (struct sockaddr_in *)&addr;
    return IPAddress((uint32_t)(s->sin_addr.s_addr));
}

uint16_t WiFiClient::localPort(int fd) const
{
    struct sockaddr_storage addr;
    socklen_t len = sizeof addr;
    getsockname(fd, (struct sockaddr*)&addr, &len);
    struct sockaddr_in *s = (struct sockaddr_in *)&addr;
    return ntohs(s->sin_port);
}

IPAddress WiFiClient::localIP() const
{
    return localIP(fd());
}

uint16_t WiFiClient::localPort() const
{
    return localPort(fd());
}

bool WiFiClient::operator==(const WiFiClient& rhs)
{
    return clientSocketHandle == rhs.clientSocketHandle && remotePort() == rhs.remotePort() && remoteIP() == rhs.remoteIP();
}

int WiFiClient::fd() const
{
    if (clientSocketHandle == NULL) {
        return -1;
    } else {
        return clientSocketHandle->fd();
    }
}

