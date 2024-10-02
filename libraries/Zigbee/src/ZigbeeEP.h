/* Common Class for Zigbee End point */

#pragma once

#include "ZigbeeCore.h"
#if SOC_IEEE802154_SUPPORTED

#include <Arduino.h>

/* Useful defines */
#define ZB_ARRAY_LENTH(arr) (sizeof(arr) / sizeof(arr[0]))
#define print_ieee_addr(addr) \
  log_i("IEEE Address: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7])
#define XYZ_TO_RGB(X, Y, Z, r, g, b)                                \
  {                                                                 \
    r = (float)(3.240479 * (X) - 1.537150 * (Y) - 0.498535 * (Z));  \
    g = (float)(-0.969256 * (X) + 1.875992 * (Y) + 0.041556 * (Z)); \
    b = (float)(0.055648 * (X) - 0.204043 * (Y) + 1.057311 * (Z));  \
    if (r > 1) {                                                    \
      r = 1;                                                        \
    }                                                               \
    if (g > 1) {                                                    \
      g = 1;                                                        \
    }                                                               \
    if (b > 1) {                                                    \
      b = 1;                                                        \
    }                                                               \
  }

#define RGB_TO_XYZ(r, g, b, X, Y, Z)                               \
  {                                                                \
    X = (float)(0.412453 * (r) + 0.357580 * (g) + 0.180423 * (b)); \
    Y = (float)(0.212671 * (r) + 0.715160 * (g) + 0.072169 * (b)); \
    Z = (float)(0.019334 * (r) + 0.119193 * (g) + 0.950227 * (b)); \
  }

typedef struct zbstring_s {
  uint8_t len;
  char data[];
} ESP_ZB_PACKED_STRUCT zbstring_t;

typedef struct zb_device_params_s {
  esp_zb_ieee_addr_t ieee_addr;
  uint8_t endpoint;
  uint16_t short_addr;
} zb_device_params_t;

typedef enum {
  SINGLE_COLOR = 0,
  RGB = 1
} zb_identify_led_type_t;

/* Zigbee End Device Class */
class ZigbeeEP {
public:
  ZigbeeEP(uint8_t endpoint = 10);
  ~ZigbeeEP();

  // Set ep config and cluster list
  void setEpConfig(esp_zb_endpoint_config_t ep_config, esp_zb_cluster_list_t *cluster_list) {
    _ep_config = ep_config;
    _cluster_list = cluster_list;
  }

  void setVersion(uint8_t version);
  uint8_t getEndpoint() {
    return _endpoint;
  }

  void printBoundDevices();
  std::list<zb_device_params_t *> getBoundDevices() const {
    return _bound_devices;
  }

  static bool isBound() {
    return _is_bound;
  }
  static void allowMultipleBinding(bool bind) {
    _allow_multiple_binding = bind;
  }

  // Manufacturer name and model implemented
  void setManufacturerAndModel(const char *name, const char *model);

  // Methods to read manufacturer and model name from selected endpoint and short address
  char *readManufacturer(uint8_t endpoint, uint16_t short_addr);
  char *readModel(uint8_t endpoint, uint16_t short_addr);

  bool epAllowMultipleBinding() {
    return _allow_multiple_binding;
  }

  // findEndpoind may be implemented by EPs to find and bind devices
  virtual void findEndpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req) {};

  //list of all handlers function calls, to be override by EPs implementation
  virtual void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {};
  virtual void zbAttributeRead(uint16_t cluster_id, const esp_zb_zcl_attribute_t *attribute) {};
  virtual void zbReadBasicCluster(const esp_zb_zcl_attribute_t *attribute);  //already implemented
  virtual void zbIdentify(const esp_zb_zcl_set_attr_value_message_t *message);

  void onIdentify(void (*callback)(uint16_t)) {
    _on_identify = callback;
  }

private:
  static bool _allow_multiple_binding;
  char *_read_manufacturer;
  char *_read_model;
  void (*_on_identify)(uint16_t time);

protected:
  static uint8_t _endpoint;
  esp_zb_ha_standard_devices_t _device_id;
  esp_zb_endpoint_config_t _ep_config;
  esp_zb_cluster_list_t *_cluster_list;
  static bool _is_bound;
  std::list<zb_device_params_t *> _bound_devices;
  SemaphoreHandle_t lock;

  friend class ZigbeeCore;
};

#endif  //SOC_IEEE802154_SUPPORTED
