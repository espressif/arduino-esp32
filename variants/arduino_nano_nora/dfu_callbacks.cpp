#include "Arduino.h"

#include <esp32-hal-tinyusb.h>
#include <esp_system.h>

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

#define ALT_COUNT   1

//--------------------------------------------------------------------+
// DFU callbacks
// Note: alt is used as the partition number, in order to support multiple partitions like FLASH, EEPROM, etc.
//--------------------------------------------------------------------+

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
