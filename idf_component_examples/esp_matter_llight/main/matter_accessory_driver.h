#include <esp_err.h>

/** Standard max values (used for remapping attributes) */
#define STANDARD_BRIGHTNESS 255
#define STANDARD_HUE 360
#define STANDARD_SATURATION 255
#define STANDARD_TEMPERATURE_FACTOR 1000000

/** Matter max values (used for remapping attributes) */
#define MATTER_BRIGHTNESS 254
#define MATTER_HUE 254
#define MATTER_SATURATION 254
#define MATTER_TEMPERATURE_FACTOR 1000000

/** Default attribute values used during initialization */
#define DEFAULT_POWER true
#define DEFAULT_BRIGHTNESS 64
#define DEFAULT_HUE 128
#define DEFAULT_SATURATION 254

typedef void *app_driver_handle_t;

esp_err_t light_accessory_set_power(void *led,  uint8_t val);
esp_err_t light_accessory_set_brightness(void *led, uint8_t val);
esp_err_t light_accessory_set_hue(void *led, uint8_t val);
esp_err_t light_accessory_set_saturation(void *led, uint8_t val);
esp_err_t light_accessory_set_temperature(void *led, uint16_t val);
app_driver_handle_t light_accessory_init();
