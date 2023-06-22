// Enable pin remapping in this file, so pin constants are meaningful
#undef ARDUINO_CORE_BUILD

#include "Arduino.h"

extern "C" {
    void initVariant() {
    }
}

// defines an "Update" object accessed only by this translation unit
// (also, the object requires MD5Builder internally)
namespace {
// ignore '{anonymous}::MD5Builder::...() defined but not used' warnings
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "../../libraries/Update/src/Updater.cpp"
#include "../../cores/esp32/MD5Builder.cpp"
#pragma GCC diagnostic pop
}

#include <esp32-hal-tinyusb.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#define ALT_COUNT   1

uint16_t load_dfu_ota_descriptor(uint8_t * dst, uint8_t * itf)
{
#define DFU_ATTRS (DFU_ATTR_CAN_DOWNLOAD | DFU_ATTR_CAN_UPLOAD | DFU_ATTR_MANIFESTATION_TOLERANT)

    uint8_t str_index = tinyusb_add_string_descriptor("Arduino DFU");
    uint8_t descriptor[TUD_DFU_DESC_LEN(ALT_COUNT)] = {
        // Interface number, string index, attributes, detach timeout, transfer size */
        TUD_DFU_DESCRIPTOR(*itf, ALT_COUNT, str_index, DFU_ATTRS, 100, CFG_TUD_DFU_XFER_BUFSIZE),
    };
    *itf+=1;
    memcpy(dst, descriptor, TUD_DFU_DESC_LEN(ALT_COUNT));
    return TUD_DFU_DESC_LEN(ALT_COUNT);
}

extern "C" {


//--------------------------------------------------------------------+
// DFU callbacks
// Note: alt is used as the partition number, in order to support multiple partitions like FLASH, EEPROM, etc.
//--------------------------------------------------------------------+

// Invoked right before tud_dfu_download_cb() (state=DFU_DNBUSY) or tud_dfu_manifest_cb() (state=DFU_MANIFEST)
// Application return timeout in milliseconds (bwPollTimeout) for the next download/manifest operation.
// During this period, USB host won't try to communicate with us.
uint32_t tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state)
{
    if ( state == DFU_DNBUSY )
    {
        // longest delay for Flash writing
        return 10;
    }
    else if (state == DFU_MANIFEST)
    {
        // time for esp32_ota_set_boot_partition to check final image
        return 100;
    }

    return 0;
}

// Invoked when received DFU_DNLOAD (wLength>0) following by DFU_GETSTATUS (state=DFU_DNBUSY) requests
// This callback could be returned before flashing op is complete (async).
// Once finished flashing, application must call tud_dfu_finish_flashing()
void tud_dfu_download_cb(uint8_t alt, uint16_t block_num, uint8_t const* data, uint16_t length)
{
    if (!Update.isRunning())
    {
        // this is the first data block, start update if possible
        if (!Update.begin())
        {
            tud_dfu_finish_flashing(DFU_STATUS_ERR_TARGET);
            return;
        }
    }

    // write a block of data to Flash
    // XXX: Update API is needlessly non-const
    size_t written = Update.write(const_cast<uint8_t*>(data), length);
    tud_dfu_finish_flashing((written == length) ? DFU_STATUS_OK : DFU_STATUS_ERR_WRITE);
}

// Invoked when download process is complete, received DFU_DNLOAD (wLength=0) following by DFU_GETSTATUS (state=Manifest)
// Application can do checksum, or actual flashing if buffered entire image previously.
// Once finished flashing, application must call tud_dfu_finish_flashing()
void tud_dfu_manifest_cb(uint8_t alt)
{
    (void) alt;
    bool ok = Update.end(true);

    // flashing op for manifest is complete
    tud_dfu_finish_flashing(ok? DFU_STATUS_OK : DFU_STATUS_ERR_VERIFY);
}

// Invoked when received DFU_UPLOAD request
// Application must populate data with up to length bytes and
// Return the number of written bytes
uint16_t tud_dfu_upload_cb(uint8_t alt, uint16_t block_num, uint8_t* data, uint16_t length)
{
    (void) alt;
    (void) block_num;
    (void) data;
    (void) length;

    // not implemented
    return 0;
}

// Invoked when the Host has terminated a download or upload transfer
void tud_dfu_abort_cb(uint8_t alt)
{
    (void) alt;
    // ignore
}

// Invoked when a DFU_DETACH request is received
void tud_dfu_detach_cb(void)
{
    // done, reboot
    esp_restart();
}

}

static const uint32_t magic_tokens[] = {
    0xf01681de, 0xbd729b29, 0xd359be7a,
};

#ifndef count_of
#define count_of(a) (sizeof(a)/sizeof((a)[0]))
#endif

extern inline void boot_double_tap_mark(volatile uint32_t *magic_location)
{
    for (int i = 0; i < count_of(magic_tokens); i++) {
        magic_location[i] = magic_tokens[i];
    }
    esp_spiram_writeback_cache();
}

extern inline void boot_double_tap_invalidate(volatile uint32_t *magic_location)
{
    for (int i = 0; i < count_of(magic_tokens); i++) {
        magic_location[i] = 0;
    }
    esp_spiram_writeback_cache();
}

extern inline bool boot_double_tap_match(volatile uint32_t *magic_location)
{
    for (int i = 0; i < count_of(magic_tokens); i++) {
        if (magic_location[i] != magic_tokens[i])
            return false;
    }
    return true;
}

#define DELAY_US 10000
#define FADESTEP 8
static void boot_rgb_pulse_delay()
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
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
}

// global, accessible from recovery sketch
bool _recovery_marker_found;
bool _recovery_active;

static void boot_double_tap_check()
{
    // allocation here is deterministic and repeatable
    // but make sure address is not clobbered by esp_spiram_test
    uint32_t psram_addr = (uint32_t) heap_caps_malloc(32+4+sizeof(magic_tokens), MALLOC_CAP_SPIRAM);
    psram_addr = (psram_addr + 0x1f) & ~0x1f; // align to next multiple of 32 bytes
    psram_addr += 4; // avoid esp_spiram_test clobbering
    volatile uint32_t *magic_location = (volatile uint32_t*) psram_addr;
    _recovery_marker_found = boot_double_tap_match(magic_location);

    const esp_partition_t *part;
    part = esp_ota_get_running_partition();
    _recovery_active = (part->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY);

    if (_recovery_marker_found && !_recovery_active) {
        // double tap detected in user application
        // do not clear tokens: they will be read by recovery FW
        // invalidate other OTA image
        part = esp_ota_get_next_update_partition(NULL);
        esp_partition_erase_range(part, 0, 4096);
        // activate factory partition
        part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
        esp_ota_set_boot_partition(part);
        esp_restart();
    } else {
        // delay with mark set then proceed
        // - for normal startup, to detect first double tap
        // - in recovery mode, to ignore several short presses
        boot_double_tap_mark(magic_location);
        boot_rgb_pulse_delay();
        boot_double_tap_invalidate(magic_location);
    }
}

namespace {
    class DoubleTap {
        public:
            DoubleTap() {
                boot_double_tap_check();
            }
    };

    DoubleTap dt __attribute__ ((init_priority (101)));
}
