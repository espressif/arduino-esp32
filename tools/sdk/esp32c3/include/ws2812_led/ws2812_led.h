/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once
#include <esp_err.h>

/** Initialize the WS2812 RGB LED
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t ws2812_led_init(void);

/** Set RGB value for the WS2812 LED
 *
 * @param[in] red Intensity of Red color (0-100)
 * @param[in] green Intensity of Green color (0-100)
 * @param[in] blue Intensity of Green color (0-100)
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t ws2812_led_set_rgb(uint32_t red, uint32_t green, uint32_t blue);

/** Set HSV value for the WS2812 LED
 *
 * @param[in] hue Value of hue in arc degrees (0-360)
 * @param[in] saturation Saturation in percentage (0-100)
 * @param[in] value Value (also called Intensity) in percentage (0-100)
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t ws2812_led_set_hsv(uint32_t hue, uint32_t saturation, uint32_t value);

/** Clear (turn off) the WS2812 LED
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t ws2812_led_clear(void);
