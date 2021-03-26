//
#ifndef __ESPNBNS_h__
#define __ESPNBNS_h__

#include <WiFi.h>
#include "AsyncUDP.h"

class NetBIOS
{
protected:
    AsyncUDP _udp;
    String _name;
    void _onPacket(AsyncUDPPacket& packet);

public:
    NetBIOS();
    ~NetBIOS();
    bool begin(const char *name);
    void end();
};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_NETBIOS)
extern NetBIOS NBNS;
#endif

#endif
