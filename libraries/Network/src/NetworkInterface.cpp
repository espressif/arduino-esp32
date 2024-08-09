/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "NetworkInterface.h"
#include "NetworkManager.h"
#include "esp_netif.h"
#include "esp_netif_defaults.h"
#include "esp_system.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/netif.h"
#include "dhcpserver/dhcpserver.h"
#include "dhcpserver/dhcpserver_options.h"
#include "esp32-hal-log.h"

static NetworkInterface *_interfaces[ESP_NETIF_ID_MAX] = {NULL, NULL, NULL, NULL, NULL, NULL};
static esp_event_handler_instance_t _ip_ev_instance = NULL;

static NetworkInterface *getNetifByEspNetif(esp_netif_t *esp_netif) {
  for (int i = 0; i < ESP_NETIF_ID_MAX; ++i) {
    if (_interfaces[i] != NULL && _interfaces[i]->netif() == esp_netif) {
      return _interfaces[i];
    }
  }
  return NULL;
}

NetworkInterface *getNetifByID(Network_Interface_ID id) {
  if (id < ESP_NETIF_ID_MAX) {
    return _interfaces[id];
  }
  return NULL;
}

#if CONFIG_LWIP_HOOK_IP6_INPUT_CUSTOM
extern "C" int lwip_hook_ip6_input(struct pbuf *p, struct netif *inp) __attribute__((weak));
extern "C" int lwip_hook_ip6_input(struct pbuf *p, struct netif *inp) {
  if (ip6_addr_isany_val(inp->ip6_addr[0].u_addr.ip6)) {
    // We don't have an LL address -> eat this packet here, so it won't get accepted on input netif
    pbuf_free(p);
    return 1;
  }
  return 0;
}
#endif

static void _ip_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  if (event_base == IP_EVENT) {
    NetworkInterface *netif = NULL;
    if (event_id == IP_EVENT_STA_GOT_IP || event_id == IP_EVENT_ETH_GOT_IP || event_id == IP_EVENT_PPP_GOT_IP) {
      ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
      netif = getNetifByEspNetif(event->esp_netif);
    } else if (event_id == IP_EVENT_STA_LOST_IP || event_id == IP_EVENT_PPP_LOST_IP || event_id == IP_EVENT_ETH_LOST_IP) {
      ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
      netif = getNetifByEspNetif(event->esp_netif);
    } else if (event_id == IP_EVENT_GOT_IP6) {
      ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
      netif = getNetifByEspNetif(event->esp_netif);
    } else if (event_id == IP_EVENT_AP_STAIPASSIGNED) {
      ip_event_ap_staipassigned_t *event = (ip_event_ap_staipassigned_t *)event_data;
      netif = getNetifByEspNetif(event->esp_netif);
    }
    if (netif != NULL) {
      netif->_onIpEvent(event_id, event_data);
    }
  }
}

void NetworkInterface::_onIpEvent(int32_t event_id, void *event_data) {
  arduino_event_t arduino_event;
  arduino_event.event_id = ARDUINO_EVENT_MAX;
  if (event_id == _got_ip_event_id) {
    setStatusBits(ESP_NETIF_HAS_IP_BIT);
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    log_v(
      "%s Got %sIP: " IPSTR " MASK: " IPSTR " GW: " IPSTR, desc(), event->ip_changed ? "New " : "Same ", IP2STR(&event->ip_info.ip),
      IP2STR(&event->ip_info.netmask), IP2STR(&event->ip_info.gw)
    );
#endif
    memcpy(&arduino_event.event_info.got_ip, event_data, sizeof(ip_event_got_ip_t));
#if SOC_WIFI_SUPPORTED
    if (_interface_id == ESP_NETIF_ID_STA) {
      arduino_event.event_id = ARDUINO_EVENT_WIFI_STA_GOT_IP;
    } else
#endif
      if (_interface_id == ESP_NETIF_ID_PPP) {
      arduino_event.event_id = ARDUINO_EVENT_PPP_GOT_IP;
    } else if (_interface_id >= ESP_NETIF_ID_ETH && _interface_id < ESP_NETIF_ID_MAX) {
      arduino_event.event_id = ARDUINO_EVENT_ETH_GOT_IP;
    }
  } else if (event_id == _lost_ip_event_id) {
    clearStatusBits(ESP_NETIF_HAS_IP_BIT);
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
    log_v("%s Lost IP", desc());
#endif
#if SOC_WIFI_SUPPORTED
    if (_interface_id == ESP_NETIF_ID_STA) {
      arduino_event.event_id = ARDUINO_EVENT_WIFI_STA_LOST_IP;
    } else
#endif
      if (_interface_id == ESP_NETIF_ID_PPP) {
      arduino_event.event_id = ARDUINO_EVENT_PPP_LOST_IP;
    } else if (_interface_id >= ESP_NETIF_ID_ETH && _interface_id < ESP_NETIF_ID_MAX) {
      arduino_event.event_id = ARDUINO_EVENT_ETH_LOST_IP;
    }
  } else if (event_id == IP_EVENT_GOT_IP6) {
    ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
    esp_ip6_addr_type_t addr_type = esp_netif_ip6_get_addr_type(&event->ip6_info.ip);
    if (addr_type == ESP_IP6_ADDR_IS_GLOBAL) {
      setStatusBits(ESP_NETIF_HAS_GLOBAL_IP6_BIT);
    } else if (addr_type == ESP_IP6_ADDR_IS_LINK_LOCAL) {
      setStatusBits(ESP_NETIF_HAS_LOCAL_IP6_BIT);
    }
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
    char if_name[NETIF_NAMESIZE] = {
      0,
    };
    netif_index_to_name(event->ip6_info.ip.zone, if_name);
    static const char *addr_types[] = {"UNKNOWN", "GLOBAL", "LINK_LOCAL", "SITE_LOCAL", "UNIQUE_LOCAL", "IPV4_MAPPED_IPV6"};
    log_v(
      "IF %s Got IPv6: Interface: %d, IP Index: %d, Type: %s, Zone: %d (%s), Address: " IPV6STR, desc(), _interface_id, event->ip_index, addr_types[addr_type],
      event->ip6_info.ip.zone, if_name, IPV62STR(event->ip6_info.ip)
    );
#endif
    memcpy(&arduino_event.event_info.got_ip6, event_data, sizeof(ip_event_got_ip6_t));
#if SOC_WIFI_SUPPORTED
    if (_interface_id == ESP_NETIF_ID_STA) {
      arduino_event.event_id = ARDUINO_EVENT_WIFI_STA_GOT_IP6;
    } else if (_interface_id == ESP_NETIF_ID_AP) {
      arduino_event.event_id = ARDUINO_EVENT_WIFI_AP_GOT_IP6;
    } else
#endif
      if (_interface_id == ESP_NETIF_ID_PPP) {
      arduino_event.event_id = ARDUINO_EVENT_PPP_GOT_IP6;
    } else if (_interface_id >= ESP_NETIF_ID_ETH && _interface_id < ESP_NETIF_ID_MAX) {
      arduino_event.event_id = ARDUINO_EVENT_ETH_GOT_IP6;
    }
#if SOC_WIFI_SUPPORTED
  } else if (event_id == IP_EVENT_AP_STAIPASSIGNED && _interface_id == ESP_NETIF_ID_AP) {
    setStatusBits(ESP_NETIF_HAS_IP_BIT);
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
    ip_event_ap_staipassigned_t *event = (ip_event_ap_staipassigned_t *)event_data;
    log_v(
      "%s Assigned IP: " IPSTR " to MAC: %02X:%02X:%02X:%02X:%02X:%02X", desc(), IP2STR(&event->ip), event->mac[0], event->mac[1], event->mac[2], event->mac[3],
      event->mac[4], event->mac[5]
    );
#endif
    arduino_event.event_id = ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED;
    memcpy(&arduino_event.event_info.wifi_ap_staipassigned, event_data, sizeof(ip_event_ap_staipassigned_t));
#endif
  }

  if (arduino_event.event_id < ARDUINO_EVENT_MAX) {
    Network.postEvent(&arduino_event);
  }
}

NetworkInterface::NetworkInterface()
  : _esp_netif(NULL), _interface_event_group(NULL), _initial_bits(0), _got_ip_event_id(-1), _lost_ip_event_id(-1), _interface_id(ESP_NETIF_ID_MAX) {}

NetworkInterface::~NetworkInterface() {
  destroyNetif();
}

IPAddress NetworkInterface::calculateNetworkID(IPAddress ip, IPAddress subnet) const {
  IPAddress networkID;

  for (size_t i = 0; i < 4; i++) {
    networkID[i] = subnet[i] & ip[i];
  }

  return networkID;
}

IPAddress NetworkInterface::calculateBroadcast(IPAddress ip, IPAddress subnet) const {
  IPAddress broadcastIp;

  for (int i = 0; i < 4; i++) {
    broadcastIp[i] = ~subnet[i] | ip[i];
  }

  return broadcastIp;
}

uint8_t NetworkInterface::calculateSubnetCIDR(IPAddress subnetMask) const {
  uint8_t CIDR = 0;

  for (uint8_t i = 0; i < 4; i++) {
    if (subnetMask[i] == 0x80) {  // 128
      CIDR += 1;
    } else if (subnetMask[i] == 0xC0) {  // 192
      CIDR += 2;
    } else if (subnetMask[i] == 0xE0) {  // 224
      CIDR += 3;
    } else if (subnetMask[i] == 0xF0) {  // 242
      CIDR += 4;
    } else if (subnetMask[i] == 0xF8) {  // 248
      CIDR += 5;
    } else if (subnetMask[i] == 0xFC) {  // 252
      CIDR += 6;
    } else if (subnetMask[i] == 0xFE) {  // 254
      CIDR += 7;
    } else if (subnetMask[i] == 0xFF) {  // 255
      CIDR += 8;
    }
  }

  return CIDR;
}

int NetworkInterface::setStatusBits(int bits) {
  if (!_interface_event_group) {
    _initial_bits |= bits;
    return _initial_bits;
  }
  return xEventGroupSetBits(_interface_event_group, bits);
}

int NetworkInterface::clearStatusBits(int bits) {
  if (!_interface_event_group) {
    _initial_bits &= ~bits;
    return _initial_bits;
  }
  return xEventGroupClearBits(_interface_event_group, bits);
}

int NetworkInterface::getStatusBits() const {
  if (!_interface_event_group) {
    return _initial_bits;
  }
  return xEventGroupGetBits(_interface_event_group);
}

int NetworkInterface::waitStatusBits(int bits, uint32_t timeout_ms) const {
  if (!_interface_event_group) {
    return 0;
  }
  bits = xEventGroupWaitBits(
           _interface_event_group,  // The event group being tested.
           bits,                    // The bits within the event group to wait for.
           pdFALSE,                 // bits should be cleared before returning.
           pdTRUE,                  // Don't wait for all bits, any bit will do.
           timeout_ms / portTICK_PERIOD_MS
         )
         & bits;  // Wait a maximum of timeout_ms for any bit to be set.
  return bits;
}

void NetworkInterface::destroyNetif() {
  if (_ip_ev_instance != NULL) {
    bool do_not_unreg_ev_handler = false;
    for (int i = 0; i < ESP_NETIF_ID_MAX; ++i) {
      if (_interfaces[i] != NULL && _interfaces[i] != this) {
        do_not_unreg_ev_handler = true;
        break;
      }
    }
    if (!do_not_unreg_ev_handler) {
      if (esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &_ip_event_cb) == ESP_OK) {
        _ip_ev_instance = NULL;
        log_v("Unregistered event handler");
      } else {
        log_e("Failed to unregister event handler");
      }
    }
  }
  if (_esp_netif != NULL) {
    esp_netif_destroy(_esp_netif);
    _esp_netif = NULL;
  }
  if (_interface_event_group != NULL) {
    vEventGroupDelete(_interface_event_group);
    _interface_event_group = NULL;
    _initial_bits = 0;
  }
  for (int i = 0; i < ESP_NETIF_ID_MAX; ++i) {
    if (_interfaces[i] != NULL && _interfaces[i] == this) {
      _interfaces[i] = NULL;
      break;
    }
  }
}

bool NetworkInterface::initNetif(Network_Interface_ID interface_id) {
  if (_esp_netif == NULL || interface_id >= ESP_NETIF_ID_MAX) {
    return false;
  }
  _interface_id = interface_id;
  _got_ip_event_id = esp_netif_get_event_id(_esp_netif, ESP_NETIF_IP_EVENT_GOT_IP);
  _lost_ip_event_id = esp_netif_get_event_id(_esp_netif, ESP_NETIF_IP_EVENT_LOST_IP);
  _interfaces[_interface_id] = this;

  if (_interface_event_group == NULL) {
    _interface_event_group = xEventGroupCreate();
    if (!_interface_event_group) {
      log_e("Interface Event Group Create Failed!");
      return false;
    }
    setStatusBits(_initial_bits);
  }

  if (_ip_ev_instance == NULL && esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &_ip_event_cb, NULL, &_ip_ev_instance)) {
    log_e("event_handler_instance_register for IP_EVENT Failed!");
    return false;
  }
  return true;
}

bool NetworkInterface::started() const {
  return (getStatusBits() & ESP_NETIF_STARTED_BIT) != 0;
}

bool NetworkInterface::connected() const {
  return (getStatusBits() & ESP_NETIF_CONNECTED_BIT) != 0;
}

bool NetworkInterface::hasIP() const {
  return (getStatusBits() & (ESP_NETIF_HAS_IP_BIT | ESP_NETIF_HAS_STATIC_IP_BIT)) != 0;
}

bool NetworkInterface::hasLinkLocalIPv6() const {
  return (getStatusBits() & ESP_NETIF_HAS_LOCAL_IP6_BIT) != 0;
}

bool NetworkInterface::hasGlobalIPv6() const {
  return (getStatusBits() & ESP_NETIF_HAS_GLOBAL_IP6_BIT) != 0;
}

bool NetworkInterface::enableIPv6(bool en) {
  if (en) {
    setStatusBits(ESP_NETIF_WANT_IP6_BIT);
    if (_esp_netif != NULL && connected()) {
      // If we are already connected, try to enable IPv6 immediately
      esp_err_t err = esp_netif_create_ip6_linklocal(_esp_netif);
      if (err != ESP_OK) {
        log_e("Failed to enable IPv6 Link Local on %s: [%d] %s", desc(), err, esp_err_to_name(err));
      } else {
        log_v("Enabled IPv6 Link Local on %s", desc());
      }
    }
  } else {
    clearStatusBits(ESP_NETIF_WANT_IP6_BIT);
  }
  return true;
}

bool NetworkInterface::dnsIP(uint8_t dns_no, IPAddress ip) {
  if (_esp_netif == NULL || dns_no > 2) {
    return false;
  }
  esp_netif_flags_t flags = esp_netif_get_flags(_esp_netif);
  if (flags & ESP_NETIF_DHCP_SERVER && dns_no > 0) {
    log_e("Server interfaces can have only one DNS server.");
    return false;
  }
  esp_netif_dns_info_t d;
  // ToDo: can this work with IPv6 addresses?
  d.ip.type = IPADDR_TYPE_V4;
  if (static_cast<uint32_t>(ip) != 0) {
    d.ip.u_addr.ip4.addr = static_cast<uint32_t>(ip);
  } else {
    d.ip.u_addr.ip4.addr = 0;
  }
  if (esp_netif_set_dns_info(_esp_netif, (esp_netif_dns_type_t)dns_no, &d) != ESP_OK) {
    return false;
  }
  return true;
}

bool NetworkInterface::config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1, IPAddress dns2, IPAddress dns3) {
  if (_esp_netif == NULL) {
    return false;
  }
  esp_err_t err = ESP_OK;
  esp_netif_ip_info_t info;
  esp_netif_dns_info_t d1;
  esp_netif_dns_info_t d2;
  esp_netif_dns_info_t d3;
  d1.ip.type = IPADDR_TYPE_V4;
  d2.ip.type = IPADDR_TYPE_V4;
  d3.ip.type = IPADDR_TYPE_V4;

  if (static_cast<uint32_t>(local_ip) != 0) {
    info.ip.addr = static_cast<uint32_t>(local_ip);
    info.gw.addr = static_cast<uint32_t>(gateway);
    info.netmask.addr = static_cast<uint32_t>(subnet);
    d1.ip.u_addr.ip4.addr = static_cast<uint32_t>(dns1);
    d2.ip.u_addr.ip4.addr = static_cast<uint32_t>(dns2);
    d3.ip.u_addr.ip4.addr = static_cast<uint32_t>(dns3);
  } else {
    info.ip.addr = 0;
    info.gw.addr = 0;
    info.netmask.addr = 0;
    d1.ip.u_addr.ip4.addr = 0;
    d2.ip.u_addr.ip4.addr = 0;
    d3.ip.u_addr.ip4.addr = 0;
  }

  esp_netif_flags_t flags = esp_netif_get_flags(_esp_netif);
  if (flags & ESP_NETIF_DHCP_SERVER) {

    // Set DNS Server
    if (d2.ip.u_addr.ip4.addr != 0) {
      err = esp_netif_set_dns_info(_esp_netif, ESP_NETIF_DNS_MAIN, &d2);
      if (err) {
        log_e("Netif Set DNS Info Failed! 0x%04x: %s", err, esp_err_to_name(err));
        return false;
      }
    }

    // Stop DHCPS
    err = esp_netif_dhcps_stop(_esp_netif);
    if (err && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
      log_e("DHCPS Stop Failed! 0x%04x: %s", err, esp_err_to_name(err));
      return false;
    }

    // Set IPv4, Netmask, Gateway
    err = esp_netif_set_ip_info(_esp_netif, &info);
    if (err) {
      log_e("Netif Set IP Failed! 0x%04x: %s", err, esp_err_to_name(err));
      return false;
    }

    dhcps_lease_t lease;
    lease.enable = true;
    uint8_t CIDR = calculateSubnetCIDR(subnet);
    log_v(
      "SoftAP: %s | Gateway: %s | DHCP Start: %s | Netmask: %s", local_ip.toString().c_str(), gateway.toString().c_str(), dns1.toString().c_str(),
      subnet.toString().c_str()
    );
    // netmask must have room for at least 12 IP addresses (AP + GW + 10 DHCP Leasing addresses)
    // netmask also must be limited to the last 8 bits of IPv4, otherwise this function won't work
    // IDF NETIF checks netmask for the 3rd byte: https://github.com/espressif/esp-idf/blob/master/components/esp_netif/lwip/esp_netif_lwip.c#L1857-L1862
    if (CIDR > 28 || CIDR < 24) {
      log_e("Bad netmask. It must be from /24 to /28 (255.255.255. 0<->240)");
      return false;  //  ESP_FAIL if initializing failed
    }
#define _byte_swap32(num) (((num >> 24) & 0xff) | ((num << 8) & 0xff0000) | ((num >> 8) & 0xff00) | ((num << 24) & 0xff000000))
    // The code below is ready for any netmask, not limited to 255.255.255.0
    uint32_t netmask = _byte_swap32(info.netmask.addr);
    uint32_t ap_ipaddr = _byte_swap32(info.ip.addr);
    uint32_t dhcp_ipaddr = _byte_swap32(static_cast<uint32_t>(dns1));
    dhcp_ipaddr = dhcp_ipaddr == 0 ? ap_ipaddr + 1 : dhcp_ipaddr;
    uint32_t leaseStartMax = ~netmask - 10;
    // there will be 10 addresses for DHCP to lease
    lease.start_ip.addr = dhcp_ipaddr;
    lease.end_ip.addr = lease.start_ip.addr + 10;
    // Check if local_ip is in the same subnet as the dhcp leasing range initial address
    if ((ap_ipaddr & netmask) != (dhcp_ipaddr & netmask)) {
      log_e(
        "The AP IP address (%s) and the DHCP start address (%s) must be in the same subnet", local_ip.toString().c_str(),
        IPAddress(_byte_swap32(dhcp_ipaddr)).toString().c_str()
      );
      return false;  //  ESP_FAIL if initializing failed
    }
    // prevents DHCP lease range to overflow subnet range
    if ((dhcp_ipaddr & ~netmask) >= leaseStartMax) {
      // make first DHCP lease addr stay in the beginning of the netmask range
      lease.start_ip.addr = (dhcp_ipaddr & netmask) + 1;
      lease.end_ip.addr = lease.start_ip.addr + 10;
      log_w("DHCP Lease out of range - Changing DHCP leasing start to %s", IPAddress(_byte_swap32(lease.start_ip.addr)).toString().c_str());
    }
    // Check if local_ip is within DHCP range
    if (ap_ipaddr >= lease.start_ip.addr && ap_ipaddr <= lease.end_ip.addr) {
      log_e(
        "The AP IP address (%s) can't be within the DHCP range (%s -- %s)", local_ip.toString().c_str(),
        IPAddress(_byte_swap32(lease.start_ip.addr)).toString().c_str(), IPAddress(_byte_swap32(lease.end_ip.addr)).toString().c_str()
      );
      return false;  //  ESP_FAIL if initializing failed
    }
    // Check if gateway is within DHCP range
    uint32_t gw_ipaddr = _byte_swap32(info.gw.addr);
    bool gw_in_same_subnet = (gw_ipaddr & netmask) == (ap_ipaddr & netmask);
    if (gw_in_same_subnet && gw_ipaddr >= lease.start_ip.addr && gw_ipaddr <= lease.end_ip.addr) {
      log_e(
        "The GatewayP address (%s) can't be within the DHCP range (%s -- %s)", gateway.toString().c_str(),
        IPAddress(_byte_swap32(lease.start_ip.addr)).toString().c_str(), IPAddress(_byte_swap32(lease.end_ip.addr)).toString().c_str()
      );
      return false;  //  ESP_FAIL if initializing failed
    }
    // all done, just revert back byte order of DHCP lease range
    lease.start_ip.addr = _byte_swap32(lease.start_ip.addr);
    lease.end_ip.addr = _byte_swap32(lease.end_ip.addr);
    log_v("DHCP Server Range: %s to %s", IPAddress(lease.start_ip.addr).toString().c_str(), IPAddress(lease.end_ip.addr).toString().c_str());
    err = esp_netif_dhcps_option(_esp_netif, ESP_NETIF_OP_SET, ESP_NETIF_REQUESTED_IP_ADDRESS, (void *)&lease, sizeof(dhcps_lease_t));
    if (err) {
      log_e("DHCPS Set Lease Failed! 0x%04x: %s", err, esp_err_to_name(err));
      return false;
    }

    // Offer DNS to DHCP clients
    if (d2.ip.u_addr.ip4.addr != 0) {
      dhcps_offer_t dhcps_dns_value = OFFER_DNS;
      err = esp_netif_dhcps_option(_esp_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_dns_value, sizeof(dhcps_dns_value));
      if (err) {
        log_e("Netif Set DHCP Option Failed! 0x%04x: %s", err, esp_err_to_name(err));
        return false;
      }
    }

    // Start DHCPS
    err = esp_netif_dhcps_start(_esp_netif);
    if (err) {
      log_e("DHCPS Start Failed! 0x%04x: %s", err, esp_err_to_name(err));
      return false;
    }
  } else {
    // Stop DHCPC
    err = esp_netif_dhcpc_stop(_esp_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
      log_e("DHCP could not be stopped! Error: 0x%04x: %s", err, esp_err_to_name(err));
      return false;
    }

    clearStatusBits(ESP_NETIF_HAS_IP_BIT | ESP_NETIF_HAS_STATIC_IP_BIT);

    // Set IPv4, Netmask, Gateway
    err = esp_netif_set_ip_info(_esp_netif, &info);
    if (err != ERR_OK) {
      log_e("ETH IP could not be configured! Error: 0x%04x: %s", err, esp_err_to_name(err));
      return false;
    }

    // Set DNS Servers
    esp_netif_set_dns_info(_esp_netif, ESP_NETIF_DNS_MAIN, &d1);
    esp_netif_set_dns_info(_esp_netif, ESP_NETIF_DNS_BACKUP, &d2);
    esp_netif_set_dns_info(_esp_netif, ESP_NETIF_DNS_FALLBACK, &d3);

    // Start DHCPC if static IP was set
    if (info.ip.addr == 0) {
      err = esp_netif_dhcpc_start(_esp_netif);
      if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        log_w("DHCP could not be started! Error: 0x%04x: %s", err, esp_err_to_name(err));
        return false;
      }
    } else {
      setStatusBits(ESP_NETIF_HAS_STATIC_IP_BIT);
    }
  }

  return true;
}

const char *NetworkInterface::getHostname() const {
  if (_esp_netif == NULL) {
    return "";
  }
  const char *hostname;
  if (esp_netif_get_hostname(_esp_netif, &hostname)) {
    return NULL;
  }
  return hostname;
}

bool NetworkInterface::setHostname(const char *hostname) const {
  if (_esp_netif == NULL) {
    return false;
  }
  return esp_netif_set_hostname(_esp_netif, hostname) == 0;
}

bool NetworkInterface::linkUp() const {
  if (_esp_netif == NULL) {
    return false;
  }
  return esp_netif_is_netif_up(_esp_netif);
}

const char *NetworkInterface::ifkey(void) const {
  if (_esp_netif == NULL) {
    return "";
  }
  return esp_netif_get_ifkey(_esp_netif);
}

const char *NetworkInterface::desc(void) const {
  if (_esp_netif == NULL) {
    return "";
  }
  return esp_netif_get_desc(_esp_netif);
}

String NetworkInterface::impl_name(void) const {
  if (_esp_netif == NULL) {
    return String("");
  }
  char netif_name[8];
  esp_err_t err = esp_netif_get_netif_impl_name(_esp_netif, netif_name);
  if (err != ESP_OK) {
    log_e("Failed to get netif impl_name: 0x%04x %s", err, esp_err_to_name(err));
    return String("");
  }
  return String(netif_name);
}

int NetworkInterface::impl_index() const {
  if (_esp_netif == NULL) {
    return -1;
  }
  return esp_netif_get_netif_impl_index(_esp_netif);
}

int NetworkInterface::route_prio() const {
  if (_esp_netif == NULL) {
    return -1;
  }
  return esp_netif_get_route_prio(_esp_netif);
}

bool NetworkInterface::setDefault() {
  if (_esp_netif == NULL) {
    return false;
  }
  esp_err_t err = esp_netif_set_default_netif(_esp_netif);
  if (err != ESP_OK) {
    log_e("Failed to set default netif: 0x%04x %s", err, esp_err_to_name(err));
    return false;
  }
  return true;
}

bool NetworkInterface::isDefault() const {
  if (_esp_netif == NULL) {
    return false;
  }
  return esp_netif_get_default_netif() == _esp_netif;
}

uint8_t *NetworkInterface::macAddress(uint8_t *mac) const {
  if (!mac || _esp_netif == NULL || _interface_id == ESP_NETIF_ID_PPP) {
    return NULL;
  }
  esp_err_t err = esp_netif_get_mac(_esp_netif, mac);
  if (err != ESP_OK) {
    log_e("Failed to get netif mac: 0x%04x %s", err, esp_err_to_name(err));
    return NULL;
  }
  return mac;
}

String NetworkInterface::macAddress(void) const {
  uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
  char macStr[18] = {0};
  macAddress(mac);
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

IPAddress NetworkInterface::localIP() const {
  if (_esp_netif == NULL) {
    return IPAddress();
  }
  esp_netif_ip_info_t ip;
  if (esp_netif_get_ip_info(_esp_netif, &ip)) {
    return IPAddress();
  }
  return IPAddress(ip.ip.addr);
}

IPAddress NetworkInterface::subnetMask() const {
  if (_esp_netif == NULL) {
    return IPAddress();
  }
  esp_netif_ip_info_t ip;
  if (esp_netif_get_ip_info(_esp_netif, &ip)) {
    return IPAddress();
  }
  return IPAddress(ip.netmask.addr);
}

IPAddress NetworkInterface::gatewayIP() const {
  if (_esp_netif == NULL) {
    return IPAddress();
  }
  esp_netif_ip_info_t ip;
  if (esp_netif_get_ip_info(_esp_netif, &ip)) {
    return IPAddress();
  }
  return IPAddress(ip.gw.addr);
}

IPAddress NetworkInterface::dnsIP(uint8_t dns_no) const {
  if (_esp_netif == NULL) {
    return IPAddress();
  }
  esp_netif_dns_info_t d;
  if (esp_netif_get_dns_info(_esp_netif, dns_no ? ESP_NETIF_DNS_BACKUP : ESP_NETIF_DNS_MAIN, &d) != ESP_OK) {
    return IPAddress();
  }
  if (d.ip.type == ESP_IPADDR_TYPE_V6) {
    // IPv6 from 4x uint32_t; byte order based on IPV62STR() in esp_netif_ip_addr.h
    // log_v("DNS got IPv6: " IPV6STR, IPV62STR(d.ip.u_addr.ip6));
    uint32_t addr0 esp_netif_htonl(d.ip.u_addr.ip6.addr[0]);
    uint32_t addr1 esp_netif_htonl(d.ip.u_addr.ip6.addr[1]);
    uint32_t addr2 esp_netif_htonl(d.ip.u_addr.ip6.addr[2]);
    uint32_t addr3 esp_netif_htonl(d.ip.u_addr.ip6.addr[3]);
    return IPAddress(
      (uint8_t)(addr0 >> 24) & 0xFF, (uint8_t)(addr0 >> 16) & 0xFF, (uint8_t)(addr0 >> 8) & 0xFF, (uint8_t)addr0 & 0xFF, (uint8_t)(addr1 >> 24) & 0xFF,
      (uint8_t)(addr1 >> 16) & 0xFF, (uint8_t)(addr1 >> 8) & 0xFF, (uint8_t)addr1 & 0xFF, (uint8_t)(addr2 >> 24) & 0xFF, (uint8_t)(addr2 >> 16) & 0xFF,
      (uint8_t)(addr2 >> 8) & 0xFF, (uint8_t)addr2 & 0xFF, (uint8_t)(addr3 >> 24) & 0xFF, (uint8_t)(addr3 >> 16) & 0xFF, (uint8_t)(addr3 >> 8) & 0xFF,
      (uint8_t)addr3 & 0xFF, d.ip.u_addr.ip6.zone
    );
  }
  // IPv4 from single uint32_t
  // log_v("DNS IPv4: " IPSTR, IP2STR(&d.ip.u_addr.ip4));
  return IPAddress(d.ip.u_addr.ip4.addr);
}

IPAddress NetworkInterface::broadcastIP() const {
  if (_esp_netif == NULL) {
    return IPAddress();
  }
  esp_netif_ip_info_t ip;
  if (esp_netif_get_ip_info(_esp_netif, &ip)) {
    return IPAddress();
  }
  return calculateBroadcast(IPAddress(ip.gw.addr), IPAddress(ip.netmask.addr));
}

IPAddress NetworkInterface::networkID() const {
  if (_esp_netif == NULL) {
    return IPAddress();
  }
  esp_netif_ip_info_t ip;
  if (esp_netif_get_ip_info(_esp_netif, &ip)) {
    return IPAddress();
  }
  return calculateNetworkID(IPAddress(ip.gw.addr), IPAddress(ip.netmask.addr));
}

uint8_t NetworkInterface::subnetCIDR() const {
  if (_esp_netif == NULL) {
    return (uint8_t)0;
  }
  esp_netif_ip_info_t ip;
  if (esp_netif_get_ip_info(_esp_netif, &ip)) {
    return (uint8_t)0;
  }
  return calculateSubnetCIDR(IPAddress(ip.netmask.addr));
}

IPAddress NetworkInterface::linkLocalIPv6() const {
  if (_esp_netif == NULL) {
    return IPAddress(IPv6);
  }
  static esp_ip6_addr_t addr;
  if (esp_netif_get_ip6_linklocal(_esp_netif, &addr)) {
    return IPAddress(IPv6);
  }
  return IPAddress(IPv6, (const uint8_t *)addr.addr, addr.zone);
}

IPAddress NetworkInterface::globalIPv6() const {
  if (_esp_netif == NULL) {
    return IPAddress(IPv6);
  }
  static esp_ip6_addr_t addr;
  if (esp_netif_get_ip6_global(_esp_netif, &addr)) {
    return IPAddress(IPv6);
  }
  return IPAddress(IPv6, (const uint8_t *)addr.addr, addr.zone);
}

size_t NetworkInterface::printTo(Print &out) const {
  size_t bytes = 0;
  if (_esp_netif == NULL) {
    return bytes;
  }
  if (isDefault()) {
    bytes += out.print("*");
  }
  const char *dscr = esp_netif_get_desc(_esp_netif);
  if (dscr != NULL) {
    bytes += out.print(dscr);
  }
  bytes += out.print(":");
  if (esp_netif_is_netif_up(_esp_netif)) {
    bytes += out.print(" <UP");
  } else {
    bytes += out.print(" <DOWN");
  }
  bytes += printDriverInfo(out);
  bytes += out.print(">");

  bytes += out.print(" (");
  esp_netif_flags_t flags = esp_netif_get_flags(_esp_netif);
  if (flags & ESP_NETIF_DHCP_CLIENT) {
    bytes += out.print("DHCPC");
    if (getStatusBits() & ESP_NETIF_HAS_STATIC_IP_BIT) {
      bytes += out.print("_OFF");
    }
  }
  if (flags & ESP_NETIF_DHCP_SERVER) {
    bytes += out.print("DHCPS");
  }
  if (flags & ESP_NETIF_FLAG_AUTOUP) {
    bytes += out.print(",AUTOUP");
  }
  if (flags & ESP_NETIF_FLAG_GARP) {
    bytes += out.print(",GARP");
  }
  if (flags & ESP_NETIF_FLAG_EVENT_IP_MODIFIED) {
    bytes += out.print(",IP_MOD");
  }
  if (flags & ESP_NETIF_FLAG_IS_PPP) {
    bytes += out.print(",PPP");
  }
  if (flags & ESP_NETIF_FLAG_IS_BRIDGE) {
    bytes += out.print(",BRIDGE");
  }
  if (flags & ESP_NETIF_FLAG_MLDV6_REPORT) {
    bytes += out.print(",V6_REP");
  }
  bytes += out.println(")");

  bytes += out.print("      ");
  bytes += out.print("ether ");
  bytes += out.print(macAddress());
  bytes += out.println();

  bytes += out.print("      ");
  bytes += out.print("inet ");
  bytes += out.print(localIP());
  bytes += out.print(" netmask ");
  bytes += out.print(subnetMask());
  bytes += out.print(" broadcast ");
  bytes += out.print(broadcastIP());
  bytes += out.println();

  bytes += out.print("      ");
  bytes += out.print("gateway ");
  bytes += out.print(gatewayIP());
  bytes += out.print(" dns ");
  bytes += out.print(dnsIP());
  bytes += out.println();

  static const char *types[] = {"UNKNOWN", "GLOBAL", "LINK_LOCAL", "SITE_LOCAL", "UNIQUE_LOCAL", "IPV4_MAPPED_IPV6"};
  esp_ip6_addr_t if_ip6[CONFIG_LWIP_IPV6_NUM_ADDRESSES];
  int v6addrs = esp_netif_get_all_ip6(_esp_netif, if_ip6);
  for (int i = 0; i < v6addrs; ++i) {
    bytes += out.print("      ");
    bytes += out.print("inet6 ");
    bytes += IPAddress(IPv6, (const uint8_t *)if_ip6[i].addr, if_ip6[i].zone).printTo(out, true);
    bytes += out.print(" type ");
    bytes += out.print(types[esp_netif_ip6_get_addr_type(&if_ip6[i])]);
    bytes += out.println();
  }

  return bytes;
}
