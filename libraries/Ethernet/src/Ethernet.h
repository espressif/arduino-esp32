#ifndef ethernet_h
#define ethernet_h

//#include <inttypes.h>
#include "IPAddress.h"
#include "WiFi.h"
#include "esp_eth.h"
#include "eth_phy/phy.h"
#include "eth_phy/phy_lan8720.h"

#define ETH_PHY_CONF  phy_lan8720_default_ethernet_config
#define ETH_PHY_ADDR  PHY0
//#define PIN_PHY_POWER 17
#define PIN_SMI_MDC   23
#define PIN_SMI_MDIO  18

class EthernetClass {
private:
  
public:
  // Initialise the ethernet controller and gain every configuration through DHCP.
  // Returns 0 if the DHCP configuration failed, and 1 if it succeeded
  int begin();
  int begin(IPAddress local_ip, IPAddress subnet, IPAddress gateway);
  int config(IPAddress local_ip, IPAddress subnet, IPAddress gateway);
 
  IPAddress localIP();
  IPAddress subnetMask();
  IPAddress gatewayIP();
  
  bool setHostname(const char * hostname);
  bool isFullDuplex();
  uint8_t getLinkSpeed();
  String getMacAddress();
  
  friend class EthernetClient;
  friend class EthernetServer;
};

class ESP32Ethernet : public EthernetClass {
};

#endif
