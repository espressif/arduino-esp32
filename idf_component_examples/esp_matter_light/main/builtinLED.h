/*
    This example code is in the Public Domain (or CC0 licensed, at your option.)
    Unless required by applicable law or agreed to in writing, this
    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, either express or implied.
        This will implement the onboard WS2812b LED as a LED indicator
        It can be used to indicate some state or status of the device
        The LED can be controlled using RGB, HSV or color temperature, brightness

        In this example, the BuiltInLED class is used as the Matter light accessory
*/

#pragma once

#include <Arduino.h>

#define MAX_HUE        360
#define MAX_SATURATION 255
#define MAX_BRIGHTNESS 255
#define MAX_PROGRESS   256

typedef struct {
  union {
    struct {
      uint32_t v : 8; /*!< Brightness/Value of the LED. 0-255 */
      uint32_t s : 8; /*!< Saturation of the LED. 0-255 */
      uint32_t h : 9; /*!< Hue of the LED. 0-360 */
    };
    uint32_t value; /*!< IHSV value of the LED. */
  };
} led_indicator_color_hsv_t;

typedef struct {
  union {
    struct {
      uint32_t r : 8; /*!< Red component of the LED color. Range: 0-255. */
      uint32_t g : 8; /*!< Green component of the LED color. Range: 0-255. */
      uint32_t b : 8; /*!< Blue component of the LED color. Range: 0-255. */
    };
    uint32_t value; /*!< Combined RGB value of the LED color. */
  };
} led_indicator_color_rgb_t;

class BuiltInLED {
private:
  uint8_t pin_number;
  bool state;
  led_indicator_color_hsv_t hsv_color;

public:
  BuiltInLED();
  ~BuiltInLED();

  static led_indicator_color_hsv_t rgb2hsv(led_indicator_color_rgb_t rgb_value);
  static led_indicator_color_rgb_t hsv2rgb(led_indicator_color_hsv_t hsv);

  void begin(uint8_t pin);
  void end();

  void on();
  void off();
  void toggle();
  bool getState();

  bool write();

  void setBrightness(uint8_t brightness);
  uint8_t getBrightness();
  void setHSV(led_indicator_color_hsv_t hsv);
  led_indicator_color_hsv_t getHSV();
  void setRGB(led_indicator_color_rgb_t color);
  led_indicator_color_rgb_t getRGB();
  void setTemperature(uint32_t temperature);
};
