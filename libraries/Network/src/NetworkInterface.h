/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_netif_types.h"
#include "esp_event.h"
#include "Arduino.h"
#include "Printable.h"

typedef enum {
  ESP_NETIF_ID_STA,
  ESP_NETIF_ID_AP,
  ESP_NETIF_ID_PPP,
  ESP_NETIF_ID_ETH0,
  ESP_NETIF_ID_ETH1,
  ESP_NETIF_ID_ETH2,
  ESP_NETIF_ID_MAX
} Network_Interface_ID;

static const int ESP_NETIF_STARTED_BIT = BIT0;
static const int ESP_NETIF_CONNECTED_BIT = BIT1;
static const int ESP_NETIF_HAS_IP_BIT = BIT2;
static const int ESP_NETIF_HAS_LOCAL_IP6_BIT = BIT3;
static const int ESP_NETIF_HAS_GLOBAL_IP6_BIT = BIT4;
static const int ESP_NETIF_WANT_IP6_BIT = BIT5;
static const int ESP_NETIF_HAS_STATIC_IP_BIT = BIT6;

#define ESP_NETIF_ID_ETH ESP_NETIF_ID_ETH0

class NetworkInterface : public Printable {
public:
  NetworkInterface();
  virtual ~NetworkInterface();

  // For server interfaces (WiFi AP), dns1 is the DHCP lease range start and dns2 is the DNS. dns3 is not used
  bool config(
    IPAddress local_ip = (uint32_t)0x00000000, IPAddress gateway = (uint32_t)0x00000000, IPAddress subnet = (uint32_t)0x00000000,
    IPAddress dns1 = (uint32_t)0x00000000, IPAddress dns2 = (uint32_t)0x00000000, IPAddress dns3 = (uint32_t)0x00000000
  );
  bool dnsIP(uint8_t dns_no, IPAddress ip);

  const char *getHostname() const;
  bool setHostname(const char *hostname) const;

  bool started() const;
  bool connected() const;
  bool hasIP() const;
  bool hasLinkLocalIPv6() const;
  bool hasGlobalIPv6() const;
  bool enableIPv6(bool en = true);

  bool linkUp() const;
  const char *ifkey() const;
  const char *desc() const;
  String impl_name() const;
  int impl_index() const;
  int route_prio() const;
  bool setDefault();
  bool isDefault() const;

  uint8_t *macAddress(uint8_t *mac) const;
  String macAddress() const;
  IPAddress localIP() const;
  IPAddress subnetMask() const;
  IPAddress gatewayIP() const;
  IPAddress dnsIP(uint8_t dns_no = 0) const;
  IPAddress broadcastIP() const;
  IPAddress networkID() const;
  uint8_t subnetCIDR() const;
  IPAddress linkLocalIPv6() const;
  IPAddress globalIPv6() const;

  size_t printTo(Print &out) const;

  esp_netif_t *netif() {
    return _esp_netif;
  }
  int getStatusBits() const;
  int waitStatusBits(int bits, uint32_t timeout_ms) const;

protected:
  esp_netif_t *_esp_netif;
  EventGroupHandle_t _interface_event_group;
  int _initial_bits;
  int32_t _got_ip_event_id;
  int32_t _lost_ip_event_id;
  Network_Interface_ID _interface_id;

  bool initNetif(Network_Interface_ID interface_id);
  void destroyNetif();
  int setStatusBits(int bits);
  int clearStatusBits(int bits);

  // virtual void getMac(uint8_t* mac) = 0;
  virtual size_t printDriverInfo(Print &out) const = 0;

public:
  void _onIpEvent(int32_t event_id, void *event_data);

private:
  IPAddress calculateNetworkID(IPAddress ip, IPAddress subnet) const;
  IPAddress calculateBroadcast(IPAddress ip, IPAddress subnet) const;
  uint8_t calculateSubnetCIDR(IPAddress subnetMask) const;
};
