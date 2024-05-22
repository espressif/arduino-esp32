/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "sdkconfig.h"
#ifdef CONFIG_ESP_INSIGHTS_ENABLED
#include "Arduino.h"

#ifdef __cplusplus

class ESPInsightsMetricsClass {
private:
  bool initialized;

public:
  ESPInsightsMetricsClass() : initialized(false) {}

  bool addBool(const char *tag, const char *key, const char *label, const char *path);
  bool addInt(const char *tag, const char *key, const char *label, const char *path);
  bool addUint(const char *tag, const char *key, const char *label, const char *path);
  bool addFloat(const char *tag, const char *key, const char *label, const char *path);
  bool addString(const char *tag, const char *key, const char *label, const char *path);
  bool addIPv4(const char *tag, const char *key, const char *label, const char *path);
  bool addMAC(const char *tag, const char *key, const char *label, const char *path);

  bool setBool(const char *key, bool b);
  bool setInt(const char *key, int32_t i);
  bool setUint(const char *key, uint32_t u);
  bool setFloat(const char *key, float f);
  bool setString(const char *key, const char *str);
  bool setIPv4(const char *key, uint32_t ip);
  bool setMAC(const char *key, uint8_t *mac);

  bool remove(const char *key);
  bool removeAll();

  void setHeapPeriod(uint32_t seconds);
  void setWiFiPeriod(uint32_t seconds);

  bool dumpHeap();
  bool dumpWiFi();

  //internal use
  void setInitialized(bool init) {
    initialized = init;
  }
};

class ESPInsightsVariablesClass {
private:
  bool initialized;

public:
  ESPInsightsVariablesClass() : initialized(false) {}

  bool addBool(const char *tag, const char *key, const char *label, const char *path);
  bool addInt(const char *tag, const char *key, const char *label, const char *path);
  bool addUint(const char *tag, const char *key, const char *label, const char *path);
  bool addFloat(const char *tag, const char *key, const char *label, const char *path);
  bool addString(const char *tag, const char *key, const char *label, const char *path);
  bool addIPv4(const char *tag, const char *key, const char *label, const char *path);
  bool addMAC(const char *tag, const char *key, const char *label, const char *path);

  bool setBool(const char *key, bool b);
  bool setInt(const char *key, int32_t i);
  bool setUint(const char *key, uint32_t u);
  bool setFloat(const char *key, float f);
  bool setString(const char *key, const char *str);
  bool setIPv4(const char *key, uint32_t ip);
  bool setMAC(const char *key, uint8_t *mac);

  bool remove(const char *key);
  bool removeAll();

  //internal use
  void setInitialized(bool init) {
    initialized = init;
  }
};

class ESPInsightsClass {
private:
  bool initialized;

public:
  ESPInsightsMetricsClass metrics;
  ESPInsightsVariablesClass variables;

  ESPInsightsClass();
  ~ESPInsightsClass();

  bool begin(const char *auth_key, const char *node_id = NULL, uint32_t log_type = 0xFFFFFFFF, bool alloc_ext_ram = false, bool use_default_transport = true);
  void end();
  bool send();
  const char *nodeID();

  void dumpTasksStatus();

  bool event(const char *tag, const char *format, ...);
};

extern ESPInsightsClass Insights;

extern "C" {
#endif

#include "esp_err.h"
#include "esp_log.h"
esp_err_t esp_diag_log_event(const char *tag, const char *format, ...) __attribute__((format(printf, 2, 3)));
#define insightsEvent(tag, format, ...) \
  { esp_diag_log_event(tag, "EV (%" PRIu32 ") %s: " format, esp_log_timestamp(), tag, ##__VA_ARGS__); }

#ifdef __cplusplus
}
#endif

#endif
