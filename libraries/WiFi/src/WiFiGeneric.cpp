/*
 ESP8266WiFiGeneric.cpp - WiFi library for esp8266

 Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.
 This file is part of the esp8266 core for Arduino environment.

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

 Reworked on 28 Dec 2015 by Markus Sattler

 */

#include "WiFi.h"
#include "WiFiGeneric.h"
#if SOC_WIFI_SUPPORTED

extern "C" {
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include "lwip/ip_addr.h"
#include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/dns.h"
#include "lwip/netif.h"
#include "dhcpserver/dhcpserver.h"
#include "dhcpserver/dhcpserver_options.h"

}  //extern "C"

#include "esp32-hal.h"
#include <vector>
#include "sdkconfig.h"

ESP_EVENT_DEFINE_BASE(ARDUINO_EVENTS);

static esp_netif_t *esp_netifs[ESP_IF_MAX] = { NULL, NULL, NULL };

esp_netif_t *get_esp_interface_netif(esp_interface_t interface) {
  if (interface < ESP_IF_MAX) {
    return esp_netifs[interface];
  }
  return NULL;
}

static void _arduino_event_cb(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  arduino_event_t arduino_event;
  arduino_event.event_id = ARDUINO_EVENT_MAX;

  /*
	 * SCAN
	 * */
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
    wifi_event_sta_scan_done_t *event = (wifi_event_sta_scan_done_t *)event_data;
    log_v("SCAN Done: ID: %u, Status: %u, Results: %u", event->scan_id, event->status, event->number);
#endif
    arduino_event.event_id = ARDUINO_EVENT_WIFI_SCAN_DONE;
    memcpy(&arduino_event.event_info.wifi_scan_done, event_data, sizeof(wifi_event_sta_scan_done_t));

    /*
	 * WPS
	 * */
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_WPS_ER_SUCCESS) {
    arduino_event.event_id = ARDUINO_EVENT_WPS_ER_SUCCESS;
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_WPS_ER_FAILED) {
    arduino_event.event_id = ARDUINO_EVENT_WPS_ER_FAILED;
    memcpy(&arduino_event.event_info.wps_fail_reason, event_data, sizeof(wifi_event_sta_wps_fail_reason_t));
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_WPS_ER_TIMEOUT) {
    arduino_event.event_id = ARDUINO_EVENT_WPS_ER_TIMEOUT;
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_WPS_ER_PIN) {
    arduino_event.event_id = ARDUINO_EVENT_WPS_ER_PIN;
    memcpy(&arduino_event.event_info.wps_er_pin, event_data, sizeof(wifi_event_sta_wps_er_pin_t));
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP) {
    arduino_event.event_id = ARDUINO_EVENT_WPS_ER_PBC_OVERLAP;

    /*
	 * FTM
	 * */
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_FTM_REPORT) {
    arduino_event.event_id = ARDUINO_EVENT_WIFI_FTM_REPORT;
    memcpy(&arduino_event.event_info.wifi_ftm_report, event_data, sizeof(wifi_event_ftm_report_t));


    /*
	 * SMART CONFIG
	 * */
  } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
    log_v("SC Scan Done");
    arduino_event.event_id = ARDUINO_EVENT_SC_SCAN_DONE;
  } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
    log_v("SC Found Channel");
    arduino_event.event_id = ARDUINO_EVENT_SC_FOUND_CHANNEL;
  } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
    smartconfig_event_got_ssid_pswd_t *event = (smartconfig_event_got_ssid_pswd_t *)event_data;
    log_v("SC: SSID: %s, Password: %s", (const char *)event->ssid, (const char *)event->password);
#endif
    arduino_event.event_id = ARDUINO_EVENT_SC_GOT_SSID_PSWD;
    memcpy(&arduino_event.event_info.sc_got_ssid_pswd, event_data, sizeof(smartconfig_event_got_ssid_pswd_t));

  } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
    log_v("SC Send Ack Done");
    arduino_event.event_id = ARDUINO_EVENT_SC_SEND_ACK_DONE;

    /*
	 * Provisioning
	 * */
  } else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_INIT) {
    log_v("Provisioning Initialized!");
    arduino_event.event_id = ARDUINO_EVENT_PROV_INIT;
  } else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_DEINIT) {
    log_v("Provisioning Uninitialized!");
    arduino_event.event_id = ARDUINO_EVENT_PROV_DEINIT;
  } else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_START) {
    log_v("Provisioning Start!");
    arduino_event.event_id = ARDUINO_EVENT_PROV_START;
  } else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_END) {
    log_v("Provisioning End!");
    wifi_prov_mgr_deinit();
    arduino_event.event_id = ARDUINO_EVENT_PROV_END;
  } else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_CRED_RECV) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
    wifi_sta_config_t *event = (wifi_sta_config_t *)event_data;
    log_v("Provisioned Credentials: SSID: %s, Password: %s", (const char *)event->ssid, (const char *)event->password);
#endif
    arduino_event.event_id = ARDUINO_EVENT_PROV_CRED_RECV;
    memcpy(&arduino_event.event_info.prov_cred_recv, event_data, sizeof(wifi_sta_config_t));
  } else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_CRED_FAIL) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_ERROR
    wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
    log_e("Provisioning Failed: Reason : %s", (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Authentication Failed" : "AP Not Found");
#endif
    arduino_event.event_id = ARDUINO_EVENT_PROV_CRED_FAIL;
    memcpy(&arduino_event.event_info.prov_fail_reason, event_data, sizeof(wifi_prov_sta_fail_reason_t));
  } else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_CRED_SUCCESS) {
    log_v("Provisioning Success!");
    arduino_event.event_id = ARDUINO_EVENT_PROV_CRED_SUCCESS;
  }

  if (arduino_event.event_id < ARDUINO_EVENT_MAX) {
    Network.postEvent(&arduino_event);
  }
}

static bool initWiFiEvents() {
  if (esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &_arduino_event_cb, NULL, NULL)) {
    log_e("event_handler_instance_register for WIFI_EVENT Failed!");
    return false;
  }

  if (esp_event_handler_instance_register(SC_EVENT, ESP_EVENT_ANY_ID, &_arduino_event_cb, NULL, NULL)) {
    log_e("event_handler_instance_register for SC_EVENT Failed!");
    return false;
  }

  if (esp_event_handler_instance_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &_arduino_event_cb, NULL, NULL)) {
    log_e("event_handler_instance_register for WIFI_PROV_EVENT Failed!");
    return false;
  }

  return true;
}

static bool deinitWiFiEvents() {
  if (esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &_arduino_event_cb)) {
    log_e("esp_event_handler_unregister for WIFI_EVENT Failed!");
    return false;
  }

  if (esp_event_handler_unregister(SC_EVENT, ESP_EVENT_ANY_ID, &_arduino_event_cb)) {
    log_e("esp_event_handler_unregister for SC_EVENT Failed!");
    return false;
  }

  if (esp_event_handler_unregister(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &_arduino_event_cb)) {
    log_e("esp_event_handler_unregister for WIFI_PROV_EVENT Failed!");
    return false;
  }

  return true;
}

/*
 * WiFi INIT
 * */

static bool lowLevelInitDone = false;
bool WiFiGenericClass::_wifiUseStaticBuffers = false;

bool WiFiGenericClass::useStaticBuffers() {
  return _wifiUseStaticBuffers;
}

void WiFiGenericClass::useStaticBuffers(bool bufferMode) {
  if (lowLevelInitDone) {
    log_w("WiFi already started. Call WiFi.mode(WIFI_MODE_NULL) before setting Static Buffer Mode.");
  }
  _wifiUseStaticBuffers = bufferMode;
}

// Temporary fix to ensure that CDC+JTAG stay on on ESP32-C3
#if CONFIG_IDF_TARGET_ESP32C3
extern "C" void phy_bbpll_en_usb(bool en);
#endif

bool wifiLowLevelInit(bool persistent) {
  if (!lowLevelInitDone) {
    lowLevelInitDone = true;
    if (!Network.begin()) {
      lowLevelInitDone = false;
      return lowLevelInitDone;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    if (!WiFiGenericClass::useStaticBuffers()) {
      cfg.static_tx_buf_num = 0;
      cfg.dynamic_tx_buf_num = 32;
      cfg.tx_buf_type = 1;
      cfg.cache_tx_buf_num = 4;  // can't be zero!
      cfg.static_rx_buf_num = 4;
      cfg.dynamic_rx_buf_num = 32;
    }

    esp_err_t err = esp_wifi_init(&cfg);
    if (err) {
      log_e("esp_wifi_init %d", err);
      lowLevelInitDone = false;
      return lowLevelInitDone;
    }
// Temporary fix to ensure that CDC+JTAG stay on on ESP32-C3
#if CONFIG_IDF_TARGET_ESP32C3
    phy_bbpll_en_usb(true);
#endif
    if (!persistent) {
      lowLevelInitDone = esp_wifi_set_storage(WIFI_STORAGE_RAM) == ESP_OK;
    }
    if (lowLevelInitDone) {
      initWiFiEvents();
      if (esp_netifs[ESP_IF_WIFI_AP] == NULL) {
        esp_netifs[ESP_IF_WIFI_AP] = esp_netif_create_default_wifi_ap();
      }
      if (esp_netifs[ESP_IF_WIFI_STA] == NULL) {
        esp_netifs[ESP_IF_WIFI_STA] = esp_netif_create_default_wifi_sta();
      }

      arduino_event_t arduino_event;
      arduino_event.event_id = ARDUINO_EVENT_WIFI_READY;
      Network.postEvent(&arduino_event);
    }
  }
  return lowLevelInitDone;
}

static bool wifiLowLevelDeinit() {
  if (lowLevelInitDone) {
    lowLevelInitDone = false;
    deinitWiFiEvents();
    if (esp_netifs[ESP_IF_WIFI_AP] != NULL) {
      esp_netif_destroy_default_wifi(esp_netifs[ESP_IF_WIFI_AP]);
      esp_netifs[ESP_IF_WIFI_AP] = NULL;
    }
    if (esp_netifs[ESP_IF_WIFI_STA] != NULL) {
      esp_netif_destroy_default_wifi(esp_netifs[ESP_IF_WIFI_STA]);
      esp_netifs[ESP_IF_WIFI_STA] = NULL;
    }
    lowLevelInitDone = !(esp_wifi_deinit() == ESP_OK);
    if (!lowLevelInitDone) {
      arduino_event_t arduino_event;
      arduino_event.event_id = ARDUINO_EVENT_WIFI_OFF;
      Network.postEvent(&arduino_event);
    }
  }
  return !lowLevelInitDone;
}

static bool _esp_wifi_started = false;

static bool espWiFiStart() {
  if (_esp_wifi_started) {
    return true;
  }
  _esp_wifi_started = true;
  esp_err_t err = esp_wifi_start();
  if (err != ESP_OK) {
    _esp_wifi_started = false;
    log_e("esp_wifi_start %d", err);
    return _esp_wifi_started;
  }
  return _esp_wifi_started;
}

static bool espWiFiStop() {
  esp_err_t err;
  if (!_esp_wifi_started) {
    return true;
  }
  _esp_wifi_started = false;
  err = esp_wifi_stop();
  if (err) {
    log_e("Could not stop WiFi! %d", err);
    _esp_wifi_started = true;
    return false;
  }
  return wifiLowLevelDeinit();
}

// -----------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------- Generic WiFi function -----------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

bool WiFiGenericClass::_persistent = true;
bool WiFiGenericClass::_long_range = false;
wifi_mode_t WiFiGenericClass::_forceSleepLastMode = WIFI_MODE_NULL;
#if CONFIG_IDF_TARGET_ESP32S2
wifi_ps_type_t WiFiGenericClass::_sleepEnabled = WIFI_PS_NONE;
#else
wifi_ps_type_t WiFiGenericClass::_sleepEnabled = WIFI_PS_MIN_MODEM;
#endif

WiFiGenericClass::WiFiGenericClass() {
}

const char *WiFiGenericClass::disconnectReasonName(wifi_err_reason_t reason) {
  return WiFi.STA.disconnectReasonName(reason);
}

const char *WiFiGenericClass::eventName(arduino_event_id_t id) {
  return Network.eventName(id);
}

const char *WiFiGenericClass::getHostname() {
  return NetworkManager::getHostname();
}

bool WiFiGenericClass::setHostname(const char *hostname) {
  return NetworkManager::setHostname(hostname);
}

/**
 * callback for WiFi events
 * @param arg
 */
void WiFiGenericClass::_eventCallback(arduino_event_t *event) {
  if (!event) return;  //Null would crash this function

  // log_d("Arduino Event: %d - %s", event->event_id, WiFi.eventName(event->event_id));
  if (event->event_id == ARDUINO_EVENT_WIFI_SCAN_DONE) {
    WiFiScanClass::_scanDone();
  } else if (event->event_id == ARDUINO_EVENT_SC_GOT_SSID_PSWD) {
    WiFi.begin(
      (const char *)event->event_info.sc_got_ssid_pswd.ssid,
      (const char *)event->event_info.sc_got_ssid_pswd.password,
      0,
      ((event->event_info.sc_got_ssid_pswd.bssid_set == true) ? event->event_info.sc_got_ssid_pswd.bssid : NULL));
  } else if (event->event_id == ARDUINO_EVENT_SC_SEND_ACK_DONE) {
    esp_smartconfig_stop();
    WiFiSTAClass::_smartConfigDone = true;
  }
}

/**
 * Return the current channel associated with the network
 * @return channel (1-13)
 */
int32_t WiFiGenericClass::channel(void) {
  uint8_t primaryChan = 0;
  wifi_second_chan_t secondChan = WIFI_SECOND_CHAN_NONE;
  if (!lowLevelInitDone) {
    return primaryChan;
  }
  esp_wifi_get_channel(&primaryChan, &secondChan);
  return primaryChan;
}

/**
 * Set the WiFi channel configuration
 * @param primary primary channel. Depending on the region, not all channels may be available.
 * @param secondary secondary channel (WIFI_SECOND_CHAN_NONE, WIFI_SECOND_CHAN_ABOVE, WIFI_SECOND_CHAN_BELOW)
 * @return 0 on success, otherwise error
 */
int WiFiGenericClass::setChannel(uint8_t primary, wifi_second_chan_t secondary) {
  wifi_country_t country;
  esp_err_t ret;

  ret = esp_wifi_get_country(&country);
  if (ret != ESP_OK) {
    log_e("Failed to get country info");
    return ret;
  }

  uint8_t min_chan = country.schan;
  uint8_t max_chan = min_chan + country.nchan - 1;

  if (primary < min_chan || primary > max_chan) {
    log_e("Invalid primary channel: %d. Valid range is %d-%d for country %s", primary, min_chan, max_chan, country.cc);
    return ESP_ERR_INVALID_ARG;
  }

  ret = esp_wifi_set_channel(primary, secondary);
  if (ret != ESP_OK) {
    log_e("Failed to set channel");
    return ret;
  }

  return ESP_OK;
}

/**
 * store WiFi config in SDK flash area
 * @param persistent
 */
void WiFiGenericClass::persistent(bool persistent) {
  _persistent = persistent;
}


/**
 * enable WiFi long range mode
 * @param enable
 */
void WiFiGenericClass::enableLongRange(bool enable) {
  _long_range = enable;
}


/**
 * set new mode
 * @param m WiFiMode_t
 */
bool WiFiGenericClass::mode(wifi_mode_t m) {
  wifi_mode_t cm = getMode();
  if (cm == m) {
    return true;
  }
  if (!cm && m) {
    // Turn ON WiFi
    if (!wifiLowLevelInit(_persistent)) {
      return false;
    }
    Network.onSysEvent(_eventCallback);
  }

  if (((m & WIFI_MODE_STA) != 0) && ((cm & WIFI_MODE_STA) == 0)) {
    // we are enabling STA interface
    WiFi.STA.onEnable();
  }
  if (((m & WIFI_MODE_AP) != 0) && ((cm & WIFI_MODE_AP) == 0)) {
    // we are enabling AP interface
    WiFi.AP.onEnable();
  }

  if (cm && !m) {
    // Turn OFF WiFi
    if (!espWiFiStop()) {
      return false;
    }
    if ((cm & WIFI_MODE_STA) != 0) {
      // we are disabling STA interface
      WiFi.STA.onDisable();
    }
    if ((cm & WIFI_MODE_AP) != 0) {
      // we are disabling AP interface
      WiFi.AP.onDisable();
    }
    Network.removeEvent(_eventCallback);
    return true;
  }

  esp_err_t err;
  if (((m & WIFI_MODE_STA) != 0) && ((cm & WIFI_MODE_STA) == 0)) {
    err = esp_netif_set_hostname(esp_netifs[ESP_IF_WIFI_STA], NetworkManager::getHostname());
    if (err) {
      log_e("Could not set hostname! %d", err);
      return false;
    }
  }
  err = esp_wifi_set_mode(m);
  if (err) {
    log_e("Could not set mode! %d", err);
    return false;
  }

  if (((m & WIFI_MODE_STA) == 0) && ((cm & WIFI_MODE_STA) != 0)) {
    // we are disabling STA interface (but AP is ON)
    WiFi.STA.onDisable();
  }
  if (((m & WIFI_MODE_AP) == 0) && ((cm & WIFI_MODE_AP) != 0)) {
    // we are disabling AP interface (but STA is ON)
    WiFi.AP.onDisable();
  }

  if (_long_range) {
    if (m & WIFI_MODE_STA) {
      err = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
      if (err != ESP_OK) {
        log_e("Could not enable long range on STA! %d", err);
        return false;
      }
    }
    if (m & WIFI_MODE_AP) {
      err = esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_LR);
      if (err != ESP_OK) {
        log_e("Could not enable long range on AP! %d", err);
        return false;
      }
    }
  }
  if (!espWiFiStart()) {
    return false;
  }

#ifdef BOARD_HAS_DUAL_ANTENNA
  if (!setDualAntennaConfig(ANT1, ANT2, WIFI_RX_ANT_AUTO, WIFI_TX_ANT_AUTO)) {
    log_e("Dual Antenna Config failed!");
    return false;
  }
#endif

  return true;
}

/**
 * get WiFi mode
 * @return WiFiMode
 */
wifi_mode_t WiFiGenericClass::getMode() {
  if (!lowLevelInitDone || !_esp_wifi_started) {
    return WIFI_MODE_NULL;
  }
  wifi_mode_t mode;
  if (esp_wifi_get_mode(&mode) != ESP_OK) {
    log_w("WiFi not started");
    return WIFI_MODE_NULL;
  }
  return mode;
}

/**
 * control STA mode
 * @param enable bool
 * @return ok
 */
bool WiFiGenericClass::enableSTA(bool enable) {

  wifi_mode_t currentMode = getMode();
  bool isEnabled = ((currentMode & WIFI_MODE_STA) != 0);

  if (isEnabled != enable) {
    if (enable) {
      return mode((wifi_mode_t)(currentMode | WIFI_MODE_STA));
    }
    return mode((wifi_mode_t)(currentMode & (~WIFI_MODE_STA)));
  }
  return true;
}

/**
 * control AP mode
 * @param enable bool
 * @return ok
 */
bool WiFiGenericClass::enableAP(bool enable) {

  wifi_mode_t currentMode = getMode();
  bool isEnabled = ((currentMode & WIFI_MODE_AP) != 0);

  if (isEnabled != enable) {
    if (enable) {
      return mode((wifi_mode_t)(currentMode | WIFI_MODE_AP));
    }
    return mode((wifi_mode_t)(currentMode & (~WIFI_MODE_AP)));
  }
  return true;
}

/**
 * control modem sleep when only in STA mode
 * @param enable bool
 * @return ok
 */
bool WiFiGenericClass::setSleep(bool enabled) {
  return setSleep(enabled ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE);
}

/**
 * control modem sleep when only in STA mode
 * @param mode wifi_ps_type_t
 * @return ok
 */
bool WiFiGenericClass::setSleep(wifi_ps_type_t sleepType) {
  if (sleepType != _sleepEnabled) {
    _sleepEnabled = sleepType;
    if (WiFi.STA.started()) {
      if (esp_wifi_set_ps(_sleepEnabled) != ESP_OK) {
        log_e("esp_wifi_set_ps failed!");
        return false;
      }
    }
    return true;
  }
  return false;
}

/**
 * get modem sleep enabled
 * @return true if modem sleep is enabled
 */
wifi_ps_type_t WiFiGenericClass::getSleep() {
  return _sleepEnabled;
}

/**
 * control wifi tx power
 * @param power enum maximum wifi tx power
 * @return ok
 */
bool WiFiGenericClass::setTxPower(wifi_power_t power) {
  if (!WiFi.STA.started() && !WiFi.AP.started()) {
    log_w("Neither AP or STA has been started");
    return false;
  }
  return esp_wifi_set_max_tx_power(power) == ESP_OK;
}

wifi_power_t WiFiGenericClass::getTxPower() {
  int8_t power;
  if (!WiFi.STA.started() && !WiFi.AP.started()) {
    log_w("Neither AP or STA has been started");
    return WIFI_POWER_19_5dBm;
  }
  if (esp_wifi_get_max_tx_power(&power)) {
    return WIFI_POWER_19_5dBm;
  }
  return (wifi_power_t)power;
}

/**
 * Initiate FTM Session.
 * @param frm_count Number of FTM frames requested in terms of 4 or 8 bursts (allowed values - 0(No pref), 16, 24, 32, 64)
 * @param burst_period Requested time period between consecutive FTM bursts in 100's of milliseconds (allowed values - 0(No pref), 2 - 255)
 * @param channel Primary channel of the FTM Responder
 * @param mac MAC address of the FTM Responder
 * @return true on success
 */
bool WiFiGenericClass::initiateFTM(uint8_t frm_count, uint16_t burst_period, uint8_t channel, const uint8_t *mac) {
  wifi_ftm_initiator_cfg_t ftmi_cfg = {
    .resp_mac = { 0, 0, 0, 0, 0, 0 },
    .channel = channel,
    .frm_count = frm_count,
    .burst_period = burst_period,
    .use_get_report_api = true
  };
  if (mac != NULL) {
    memcpy(ftmi_cfg.resp_mac, mac, 6);
  }
  // Request FTM session with the Responder
  if (ESP_OK != esp_wifi_ftm_initiate_session(&ftmi_cfg)) {
    log_e("Failed to initiate FTM session");
    return false;
  }
  return true;
}

/**
 * Configure Dual antenna.
 * @param gpio_ant1 Configure the GPIO number for the antenna 1 connected to the RF switch (default GPIO2 on ESP32-WROOM-DA)
 * @param gpio_ant2 Configure the GPIO number for the antenna 2 connected to the RF switch (default GPIO25 on ESP32-WROOM-DA)
 * @param rx_mode Set the RX antenna mode. See wifi_rx_ant_t for the options.
 * @param tx_mode Set the TX antenna mode. See wifi_tx_ant_t for the options.
 * @return true on success
 */
bool WiFiGenericClass::setDualAntennaConfig(uint8_t gpio_ant1, uint8_t gpio_ant2, wifi_rx_ant_t rx_mode, wifi_tx_ant_t tx_mode) {

  wifi_ant_gpio_config_t wifi_ant_io;

  if (ESP_OK != esp_wifi_get_ant_gpio(&wifi_ant_io)) {
    log_e("Failed to get antenna configuration");
    return false;
  }

  wifi_ant_io.gpio_cfg[0].gpio_num = gpio_ant1;
  wifi_ant_io.gpio_cfg[0].gpio_select = 1;
  wifi_ant_io.gpio_cfg[1].gpio_num = gpio_ant2;
  wifi_ant_io.gpio_cfg[1].gpio_select = 1;

  if (ESP_OK != esp_wifi_set_ant_gpio(&wifi_ant_io)) {
    log_e("Failed to set antenna GPIO configuration");
    return false;
  }

  // Set antenna default configuration
  wifi_ant_config_t ant_config = {
    .rx_ant_mode = WIFI_ANT_MODE_AUTO,
    .rx_ant_default = WIFI_ANT_MAX,  // Ignored in AUTO mode
    .tx_ant_mode = WIFI_ANT_MODE_AUTO,
    .enabled_ant0 = 1,
    .enabled_ant1 = 2,
  };

  switch (rx_mode) {
    case WIFI_RX_ANT0:
      ant_config.rx_ant_mode = WIFI_ANT_MODE_ANT0;
      break;
    case WIFI_RX_ANT1:
      ant_config.rx_ant_mode = WIFI_ANT_MODE_ANT1;
      break;
    case WIFI_RX_ANT_AUTO:
      log_i("TX Antenna will be automatically selected");
      ant_config.rx_ant_default = WIFI_ANT_ANT0;
      ant_config.rx_ant_mode = WIFI_ANT_MODE_AUTO;
      // Force TX for AUTO if RX is AUTO
      ant_config.tx_ant_mode = WIFI_ANT_MODE_AUTO;
      goto set_ant;
      break;
    default:
      log_e("Invalid default antenna! Falling back to AUTO");
      ant_config.rx_ant_mode = WIFI_ANT_MODE_AUTO;
      break;
  }

  switch (tx_mode) {
    case WIFI_TX_ANT0:
      ant_config.tx_ant_mode = WIFI_ANT_MODE_ANT0;
      break;
    case WIFI_TX_ANT1:
      ant_config.tx_ant_mode = WIFI_ANT_MODE_ANT1;
      break;
    case WIFI_TX_ANT_AUTO:
      log_i("RX Antenna will be automatically selected");
      ant_config.rx_ant_default = WIFI_ANT_ANT0;
      ant_config.tx_ant_mode = WIFI_ANT_MODE_AUTO;
      // Force RX for AUTO if RX is AUTO
      ant_config.rx_ant_mode = WIFI_ANT_MODE_AUTO;
      break;
    default:
      log_e("Invalid default antenna! Falling back to AUTO");
      ant_config.rx_ant_default = WIFI_ANT_ANT0;
      ant_config.tx_ant_mode = WIFI_ANT_MODE_AUTO;
      break;
  }

set_ant:
  if (ESP_OK != esp_wifi_set_ant(&ant_config)) {
    log_e("Failed to set antenna configuration");
    return false;
  }

  return true;
}

// -----------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------ Generic Network function ---------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

/*
 * Deprecated Methods
*/
int WiFiGenericClass::hostByName(const char *aHostname, IPAddress &aResult) {
  return Network.hostByName(aHostname, aResult);
}

IPAddress WiFiGenericClass::calculateNetworkID(IPAddress ip, IPAddress subnet) {
  IPAddress networkID;

  for (size_t i = 0; i < 4; i++)
    networkID[i] = subnet[i] & ip[i];

  return networkID;
}

IPAddress WiFiGenericClass::calculateBroadcast(IPAddress ip, IPAddress subnet) {
  IPAddress broadcastIp;

  for (int i = 0; i < 4; i++)
    broadcastIp[i] = ~subnet[i] | ip[i];

  return broadcastIp;
}

uint8_t WiFiGenericClass::calculateSubnetCIDR(IPAddress subnetMask) {
  uint8_t CIDR = 0;

  for (uint8_t i = 0; i < 4; i++) {
    if (subnetMask[i] == 0x80)  // 128
      CIDR += 1;
    else if (subnetMask[i] == 0xC0)  // 192
      CIDR += 2;
    else if (subnetMask[i] == 0xE0)  // 224
      CIDR += 3;
    else if (subnetMask[i] == 0xF0)  // 242
      CIDR += 4;
    else if (subnetMask[i] == 0xF8)  // 248
      CIDR += 5;
    else if (subnetMask[i] == 0xFC)  // 252
      CIDR += 6;
    else if (subnetMask[i] == 0xFE)  // 254
      CIDR += 7;
    else if (subnetMask[i] == 0xFF)  // 255
      CIDR += 8;
  }

  return CIDR;
}

wifi_event_id_t WiFiGenericClass::onEvent(WiFiEventCb cbEvent, arduino_event_id_t event) {
  return Network.onEvent(cbEvent, event);
}

wifi_event_id_t WiFiGenericClass::onEvent(WiFiEventFuncCb cbEvent, arduino_event_id_t event) {
  return Network.onEvent(cbEvent, event);
}

wifi_event_id_t WiFiGenericClass::onEvent(WiFiEventSysCb cbEvent, arduino_event_id_t event) {
  return Network.onEvent(cbEvent, event);
}

void WiFiGenericClass::removeEvent(WiFiEventCb cbEvent, arduino_event_id_t event) {
  Network.removeEvent(cbEvent, event);
}

void WiFiGenericClass::removeEvent(WiFiEventSysCb cbEvent, arduino_event_id_t event) {
  Network.removeEvent(cbEvent, event);
}

void WiFiGenericClass::removeEvent(wifi_event_id_t id) {
  Network.removeEvent(id);
}

int WiFiGenericClass::setStatusBits(int bits) {
  return Network.setStatusBits(bits);
}

int WiFiGenericClass::clearStatusBits(int bits) {
  return Network.clearStatusBits(bits);
}

int WiFiGenericClass::getStatusBits() {
  return Network.getStatusBits();
}

int WiFiGenericClass::waitStatusBits(int bits, uint32_t timeout_ms) {
  return Network.waitStatusBits(bits, timeout_ms);
}

#endif /* SOC_WIFI_SUPPORTED */
