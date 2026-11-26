// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once
#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <math.h>
#include <Matter.h>
#include <MatterEndPoint.h>

/**
 * @brief Matter illuminance sensor endpoint (read-only reporting, 1 lx to 3.576 Mlx).
 *
 * Compatible with Home Assistant, Apple Home, Amazon Alexa, and Google Home.
 */
class MatterLightSensor : public MatterEndPoint {
public:
  /**
   * @brief Create a new Matter light sensor endpoint.
   */
  MatterLightSensor();
  /**
   * @brief Destroy the light sensor endpoint object.
   */
  ~MatterLightSensor();
  /**
   * @brief Initialize the Matter light sensor endpoint.
   * @param illuminance Initial illuminance (1 lx to 3.576 Mlx, default 1.00).
   * @return true if successful, false otherwise.
   */
  bool begin(double illuminance = 1.00) {
    if (illuminance < 1.0 || illuminance > 3576000.0) {
      log_e("Light illuminance value out of range [1..3576000].");
      return false;
    }
    // compute the raw value in a wider type, round, and clamp to the max valid Matter value (0xFFFE) before casting
    long rawValue = lround(10000.0 * log10(illuminance) + 1);
    if (rawValue > 65534) {
      rawValue = 65534;
    }
    return begin(static_cast<uint16_t>(rawValue));
  }
  /**
   * @brief Stop processing Matter light sensor events.
   */
  void end();

  /**
   * @brief Set the reported illuminance value.
   * @param illuminance Illuminance to report (1 lx to 3.576 Mlx).
   * @return true if successful, false otherwise.
   */
  bool setIlluminance(double illuminance) {
    if (illuminance < 1.0 || illuminance > 3576000.0) {
      log_e("Light illuminance value out of range [1..3576000].");
      return false;
    }
    // compute the raw value in a wider type, round, and clamp to the max valid Matter value (0xFFFE) before casting
    long rawValue = lround(10000.0 * log10(illuminance) + 1);
    if (rawValue > 65534) {
      rawValue = 65534;
    }
    return setRawIlluminance(static_cast<uint16_t>(rawValue));
  }
  /**
   * @brief Get the current reported illuminance value.
   * @return Illuminance in lux.
   */
  double getIlluminance() {
    return (double)pow(10, (rawIlluminance - 1) / 10000.0);
  }
  /**
   * @brief Assignment — set the reported illuminance.
   * @param illuminance Illuminance in lux (1 lx to 3.576 Mlx).
   */
  void operator=(double illuminance) {
    setIlluminance(illuminance);
  }
  /**
   * @brief Conversion — current reported illuminance.
   * @return Illuminance in lux.
   */
  operator double() {
    return (double)getIlluminance();
  }

  /**
   * @brief Called by the Matter internal event processor when an attribute changes.
   *
   * Can be overridden by the application if necessary.
   *
   * @param endpoint_id Endpoint ID where the attribute changed.
   * @param cluster_id Cluster ID of the changed attribute.
   * @param attribute_id Attribute ID that changed.
   * @param val Pointer to the new attribute value.
   * @return true if the change was handled successfully.
   */
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

protected:
  bool started = false;
  // implementation stores illuminance as a Matter-style logarithmic value
  uint16_t rawIlluminance = 0;
  // internal function to set the raw illuminance value (Matter Cluster)
  bool begin(uint16_t _rawIlluminance);
  bool setRawIlluminance(uint16_t _rawIlluminance);
  void onStackStarted() override;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
