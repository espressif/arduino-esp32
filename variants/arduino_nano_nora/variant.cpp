// Enable pin remapping in this file, so pin constants are meaningful
#undef ARDUINO_CORE_BUILD

#include "Arduino.h"

#include "double_tap.h"

#include <esp_system.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

extern "C" {
    void initVariant() {
        // nothing to do
    }
}

// global, accessible from recovery sketch
bool _recovery_marker_found; // double tap detected
bool _recovery_active;       // running from factory partition

#define DELAY_US 10000
#define FADESTEP 8
static void rgb_pulse_delay(void)
{
    //                Bv   R^  G  x
    int widths[4] = { 192, 64, 0, 0 };
    int dec_led = 0;

    // initialize RGB signals from weak pinstraps
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    while (dec_led < 3) {
        widths[dec_led] -= FADESTEP;
        widths[dec_led+1] += FADESTEP;
        if (widths[dec_led] <= 0) {
            widths[dec_led] = 0;
            dec_led = dec_led+1;
            widths[dec_led] = 255;
        }

        analogWrite(LED_RED, 255-widths[1]);
        analogWrite(LED_GREEN, 255-widths[2]);
        analogWrite(LED_BLUE, 255-widths[0]);
        delayMicroseconds(DELAY_US);
    }

    // reset pins to digital HIGH before leaving
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);
}

static void NANO_ESP32_enter_bootloader(void)
{
    if (!_recovery_active) {
        // check for valid partition scheme
        const esp_partition_t *ota_part = esp_ota_get_next_update_partition(NULL);
        const esp_partition_t *fact_part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
        if (ota_part && fact_part) {
            // set tokens so the recovery FW will find them
            double_tap_mark();
            // invalidate other OTA image
            esp_partition_erase_range(ota_part, 0, 4096);
            // activate factory partition
            esp_ota_set_boot_partition(fact_part);
        }
    }

    esp_restart();
}

static void boot_double_tap_logic()
{
    const esp_partition_t *part = esp_ota_get_running_partition();
    _recovery_active = (part->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY);

    double_tap_init();

    _recovery_marker_found = double_tap_check_match();
    if (_recovery_marker_found && !_recovery_active) {
        // double tap detected in user application, reboot to factory
        NANO_ESP32_enter_bootloader();
    }

    // delay with mark set then proceed
    // - for normal startup, to detect first double tap
    // - in recovery mode, to ignore several short presses
    double_tap_mark();
    rgb_pulse_delay();
    double_tap_invalidate();
}

namespace {
    class DoubleTap {
        public:
            DoubleTap() {
                boot_double_tap_logic();
            }
    };

    DoubleTap dt __attribute__ ((init_priority (101)));
}
