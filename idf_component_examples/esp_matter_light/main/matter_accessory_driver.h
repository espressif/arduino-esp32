#include <esp_err.h>
#include <sdkconfig.h>

// set your board WS2812b pin here (e.g. 48 is the default pin for the ESP32-S3 devkit)
#ifndef CONFIG_WS2812_PIN
#define WS2812_PIN 48  // ESP32-S3 DevKitC built-in LED
#else
#define WS2812_PIN CONFIG_WS2812_PIN  // From sdkconfig.defaults.<soc>
#endif

#ifndef RGB_BUILTIN
#define RGB_BUILTIN WS2812_PIN
#endif

// Set your board button pin here (e.g. 0 is the default pin for the ESP32-S3 devkit)
#ifndef CONFIG_BUTTON_PIN
#define BUTTON_PIN 0  // ESP32-S3 DevKitC built-in button
#else
#define BUTTON_PIN CONFIG_BUTTON_PIN  // From sdkconfig.defaults.<soc>
#endif

/** Standard max values (used for remapping attributes) */
#define STANDARD_BRIGHTNESS         255
#define STANDARD_HUE                360
#define STANDARD_SATURATION         255
#define STANDARD_TEMPERATURE_FACTOR 1000000

/** Matter max values (used for remapping attributes) */
#define MATTER_BRIGHTNESS         254
#define MATTER_HUE                254
#define MATTER_SATURATION         254
#define MATTER_TEMPERATURE_FACTOR 1000000

/** Default attribute values used during initialization */
#define DEFAULT_POWER      true
#define DEFAULT_BRIGHTNESS 64
#define DEFAULT_HUE        128
#define DEFAULT_SATURATION 254

typedef void *app_driver_handle_t;

esp_err_t light_accessory_set_power(void *led, uint8_t val);
esp_err_t light_accessory_set_brightness(void *led, uint8_t val);
esp_err_t light_accessory_set_hue(void *led, uint8_t val);
esp_err_t light_accessory_set_saturation(void *led, uint8_t val);
esp_err_t light_accessory_set_temperature(void *led, uint16_t val);
app_driver_handle_t light_accessory_init();
