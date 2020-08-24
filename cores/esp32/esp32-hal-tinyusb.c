
#include "sdkconfig.h"
#if CONFIG_USB_ENABLED
#include <stdlib.h>

#include "esp_log.h"

#include "soc/soc.h"
#include "soc/efuse_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/usb_struct.h"
#include "soc/usb_reg.h"
#include "soc/usb_wrap_reg.h"
#include "soc/usb_wrap_struct.h"
#include "soc/periph_defs.h"
#include "soc/timer_group_struct.h"
#include "soc/system_reg.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/periph_ctrl.h"

#include "esp_efuse.h"
#include "esp_efuse_table.h"

#include "tinyusb.h"
#include "esp32-hal.h"

#include "esp32-hal-tinyusb.h"
#include "esp32s2/rom/usb/usb_persist.h"

typedef char tusb_str_t[127];

static bool WEBUSB_ENABLED                = false;

static tusb_str_t WEBUSB_URL              = "";
static tusb_str_t USB_DEVICE_PRODUCT      = "";
static tusb_str_t USB_DEVICE_MANUFACTURER = "";
static tusb_str_t USB_DEVICE_SERIAL       = "";

static uint8_t USB_DEVICE_ATTRIBUTES     = 0;
static uint16_t USB_DEVICE_POWER         = 0;

/*
 * Device Descriptor
 * */
static tusb_desc_device_t tinyusb_device_descriptor = {
        .bLength = sizeof(tusb_desc_device_t),
        .bDescriptorType = TUSB_DESC_DEVICE,
        .bcdUSB = 0,
        .bDeviceClass = 0,
        .bDeviceSubClass = 0,
        .bDeviceProtocol = 0,
        .bMaxPacketSize0 = CFG_TUD_ENDOINT0_SIZE,

        .idVendor = 0,
        .idProduct = 0,
        .bcdDevice = 0,

        .iManufacturer = 0x01,
        .iProduct = 0x02,
        .iSerialNumber = 0x03,

        .bNumConfigurations = 0x01
};

/*
 * String Descriptors
 * */
#define MAX_STRING_DESCRIPTORS 20
static uint32_t tinyusb_string_descriptor_len = 4;
static char * tinyusb_string_descriptor[MAX_STRING_DESCRIPTORS] = {
        // array of pointer to string descriptors
        "\x09\x04",   // 0: is supported language is English (0x0409)
        USB_DEVICE_MANUFACTURER,// 1: Manufacturer
        USB_DEVICE_PRODUCT,     // 2: Product
        USB_DEVICE_SERIAL,      // 3: Serials, should use chip ID
};


/* Microsoft OS 2.0 registry property descriptor
Per MS requirements https://msdn.microsoft.com/en-us/library/windows/hardware/hh450799(v=vs.85).aspx
device should create DeviceInterfaceGUIDs. It can be done by driver and
in case of real PnP solution device should expose MS "Microsoft OS 2.0
registry property descriptor". Such descriptor can insert any record
into Windows registry per device/configuration/interface. In our case it
will insert "DeviceInterfaceGUIDs" multistring property.

GUID is freshly generated and should be OK to use.

https://developers.google.com/web/fundamentals/native-hardware/build-for-webusb/
(Section Microsoft OS compatibility descriptors)
 */

#define MS_OS_20_DESC_LEN  0xB2

static uint8_t const tinyusb_ms_os_20_descriptor[] =
{
        // Set header: length, type, windows version, total length
        U16_TO_U8S_LE(0x000A), U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR), U32_TO_U8S_LE(0x06030000), U16_TO_U8S_LE(MS_OS_20_DESC_LEN),

        // Configuration subset header: length, type, configuration index, reserved, configuration total length
        U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_CONFIGURATION), 0, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN-0x0A),

        // Function Subset header: length, type, first interface, reserved, subset length
        U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_FUNCTION), 0, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN-0x0A-0x08),

        // MS OS 2.0 Compatible ID descriptor: length, type, compatible ID, sub compatible ID
        U16_TO_U8S_LE(0x0014), U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID), 'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // sub-compatible

        // MS OS 2.0 Registry property descriptor: length, type
        U16_TO_U8S_LE(MS_OS_20_DESC_LEN-0x0A-0x08-0x08-0x14), U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),
        U16_TO_U8S_LE(0x0007), U16_TO_U8S_LE(0x002A), // wPropertyDataType, wPropertyNameLength and PropertyName "DeviceInterfaceGUIDs\0" in UTF-16
        'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00,
        'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00,
        U16_TO_U8S_LE(0x0050), // wPropertyDataLength
        //bPropertyData: “{975F44D9-0D08-43FD-8B3E-127CA8AFFF9D}”.
        '{', 0x00, '9', 0x00, '7', 0x00, '5', 0x00, 'F', 0x00, '4', 0x00, '4', 0x00, 'D', 0x00, '9', 0x00, '-', 0x00,
        '0', 0x00, 'D', 0x00, '0', 0x00, '8', 0x00, '-', 0x00, '4', 0x00, '3', 0x00, 'F', 0x00, 'D', 0x00, '-', 0x00,
        '8', 0x00, 'B', 0x00, '3', 0x00, 'E', 0x00, '-', 0x00, '1', 0x00, '2', 0x00, '7', 0x00, 'C', 0x00, 'A', 0x00,
        '8', 0x00, 'A', 0x00, 'F', 0x00, 'F', 0x00, 'F', 0x00, '9', 0x00, 'D', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00
};

TU_VERIFY_STATIC(sizeof(tinyusb_ms_os_20_descriptor) == MS_OS_20_DESC_LEN, "Incorrect size");


/*
 * BOS Descriptor (required for webUSB)
 * */
#define BOS_TOTAL_LEN      (TUD_BOS_DESC_LEN + TUD_BOS_WEBUSB_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)

enum {
    VENDOR_REQUEST_WEBUSB = 1,
    VENDOR_REQUEST_MICROSOFT = 2
};

static uint8_t const tinyusb_bos_descriptor[] = {
        // total length, number of device caps
        TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 2),

        // Vendor Code, iLandingPage
        TUD_BOS_WEBUSB_DESCRIPTOR(VENDOR_REQUEST_WEBUSB, 1),

        // Microsoft OS 2.0 descriptor
        TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, VENDOR_REQUEST_MICROSOFT)
};

/*
 * URL Descriptor (required for webUSB)
 * */
typedef struct TU_ATTR_PACKED {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bScheme;
  char    url[127];
} tinyusb_desc_webusb_url_t;

static tinyusb_desc_webusb_url_t tinyusb_url_descriptor = {
        .bLength         = 3,
        .bDescriptorType = 3, // WEBUSB URL type
        .bScheme         = 1, // URL Scheme Prefix: 0: "http://", 1: "https://", 255: ""
        .url             = ""
};

/*
 * Configuration Descriptor
 * */

static tinyusb_descriptor_cb_t tinyusb_loaded_interfaces_callbacks[USB_INTERFACE_MAX];
static uint32_t tinyusb_loaded_interfaces_mask = 0;
static uint8_t tinyusb_loaded_interfaces_num = 0;
static uint16_t tinyusb_config_descriptor_len = 0;
static uint8_t * tinyusb_config_descriptor = NULL;

/*
 * Endpoint Usage Tracking
 * */
typedef union {
        struct {
                uint32_t in:16;
                uint32_t out:16;
        };
        uint32_t val;
} tinyusb_endpoints_usage_t;

static tinyusb_endpoints_usage_t tinyusb_endpoints;


/*
 * TinyUSB Callbacks
 * */

/**
 * @brief Invoked when received GET CONFIGURATION DESCRIPTOR.
 */
uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    //log_d("%u", index);
    return tinyusb_config_descriptor;
}

/**
 * @brief Invoked when received GET DEVICE DESCRIPTOR.
 */
uint8_t const *tud_descriptor_device_cb(void)
{
    //log_d("");
    return (uint8_t const *)&tinyusb_device_descriptor;
}

/**
 * @brief Invoked when received GET STRING DESCRIPTOR request.
 */
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    //log_d("%u (0x%x)", index, langid);
    static uint16_t _desc_str[127];
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], tinyusb_string_descriptor[0], 2);
        chr_count = 1;
    } else {
        // Convert ASCII string into UTF-16
        if (index >= tinyusb_string_descriptor_len) {
            return NULL;
        }
        const char *str = tinyusb_string_descriptor[index];
        // Cap at max char
        chr_count = strlen(str);
        if (chr_count > 126) {
            chr_count = 126;
        }
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // first byte is len, second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

    return _desc_str;
}

/**
 * @brief Invoked when received GET BOS DESCRIPTOR request.
 */
uint8_t const * tud_descriptor_bos_cb(void)
{
    //log_d("");
    return tinyusb_bos_descriptor;
}

__attribute__ ((weak)) bool tinyusb_vendor_control_request_cb(uint8_t rhport, tusb_control_request_t const * request){ return false; }
__attribute__ ((weak)) bool tinyusb_vendor_control_complete_cb(uint8_t rhport, tusb_control_request_t const * request){ return true; }

/**
 * @brief Handle WebUSB and Vendor requests.
 */
bool tud_vendor_control_request_cb(uint8_t rhport, tusb_control_request_t const * request)
{
    if(WEBUSB_ENABLED && (request->bRequest == VENDOR_REQUEST_WEBUSB
            || (request->bRequest == VENDOR_REQUEST_MICROSOFT && request->wIndex == 7))){
        if(request->bRequest == VENDOR_REQUEST_WEBUSB){
            // match vendor request in BOS descriptor
            // Get landing page url
            tinyusb_url_descriptor.bLength = 3 + strlen(WEBUSB_URL);
            snprintf(tinyusb_url_descriptor.url, 127, "%s", WEBUSB_URL);
            return tud_control_xfer(rhport, request, (void*) &tinyusb_url_descriptor, tinyusb_url_descriptor.bLength);
        }
        // Get Microsoft OS 2.0 compatible descriptor
        uint16_t total_len;
        memcpy(&total_len, tinyusb_ms_os_20_descriptor + 8, 2);
        return tud_control_xfer(rhport, request, (void*) tinyusb_ms_os_20_descriptor, total_len);
    }
    return tinyusb_vendor_control_request_cb(rhport, request);
}

bool tud_vendor_control_complete_cb(uint8_t rhport, tusb_control_request_t const * request)
{
    if(!WEBUSB_ENABLED || !(request->bRequest == VENDOR_REQUEST_WEBUSB
            || (request->bRequest == VENDOR_REQUEST_MICROSOFT && request->wIndex == 7))){
        return tinyusb_vendor_control_complete_cb(rhport, request);
    }
    return true;
}

/*
 * Required Callbacks
 * */
#if CFG_TUD_HID
__attribute__ ((weak)) const uint8_t * tud_hid_descriptor_report_cb(void){return NULL;}
__attribute__ ((weak)) uint16_t tud_hid_get_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen){return 0;}
__attribute__ ((weak)) void tud_hid_set_report_cb(uint8_t report_id, hid_report_type_t report_type, const uint8_t * buffer, uint16_t bufsize){}
#endif
#if CFG_TUD_MSC
__attribute__ ((weak)) bool tud_msc_test_unit_ready_cb(uint8_t lun){return false;}
__attribute__ ((weak)) void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]){}
__attribute__ ((weak)) void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size){}
__attribute__ ((weak)) int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize){return -1;}
__attribute__ ((weak)) int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize){return -1;}
__attribute__ ((weak)) int32_t tud_msc_scsi_cb (uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize){return -1;}
#endif


/*
 * Private API
 * */
static bool usb_persist_enabled = false;
static restart_type_t usb_persist_mode = RESTART_NO_PERSIST;

static bool tinyusb_reserve_in_endpoint(uint8_t endpoint){
    if(endpoint > 6 || (tinyusb_endpoints.in & BIT(endpoint)) != 0){
        return false;
    }
    tinyusb_endpoints.in |= BIT(endpoint);
    return true;
}

static bool tinyusb_reserve_out_endpoint(uint8_t endpoint){
    if(endpoint > 6 || (tinyusb_endpoints.out & BIT(endpoint)) != 0){
        return false;
    }
    tinyusb_endpoints.out |= BIT(endpoint);
    return true;
}

static bool tinyusb_has_available_fifos(void){
    uint8_t max_endpoints = 4, active_endpoints = 0;
    if (tinyusb_loaded_interfaces_mask & BIT(USB_INTERFACE_CDC)) {
        max_endpoints = 5; //CDC endpoint 0x85 is actually not linked to FIFO and not used
    }
    for(uint8_t i=1; i<7; i++){
        if((tinyusb_endpoints.in & BIT(i)) != 0){
            active_endpoints++;
        }
    }

    return active_endpoints < max_endpoints;
}

static uint16_t tinyusb_load_descriptor(tinyusb_interface_t interface, uint8_t * dst, uint8_t * itf)
{
    if(tinyusb_loaded_interfaces_callbacks[interface]){
        return tinyusb_loaded_interfaces_callbacks[interface](dst, itf);
    }
    return 0;
}

static bool tinyusb_load_enabled_interfaces(){
    tinyusb_config_descriptor_len += TUD_CONFIG_DESC_LEN;
    tinyusb_config_descriptor = (uint8_t *)malloc(tinyusb_config_descriptor_len);
    if (tinyusb_config_descriptor == NULL) {
        log_e("Descriptor Malloc Failed");
        return false;
    }
    uint8_t * dst = tinyusb_config_descriptor + TUD_CONFIG_DESC_LEN;

    for(int i=0; i<USB_INTERFACE_MAX; i++){
        if (tinyusb_loaded_interfaces_mask & (1U << i)) {
            uint16_t len = tinyusb_load_descriptor((tinyusb_interface_t)i, dst, &tinyusb_loaded_interfaces_num);
            if (!len) {
                log_e("Descriptor Load Failed");
                return false;
            } else {
                if(i == USB_INTERFACE_CDC){
                    if(!tinyusb_reserve_out_endpoint(3) ||!tinyusb_reserve_in_endpoint(4) || !tinyusb_reserve_in_endpoint(5)){
                        log_e("CDC Reserve Endpoints Failed");
                        return false;
                    }
                }
                dst += len;
            }
        }
    }
    uint8_t str_index = tinyusb_add_string_descriptor("TinyUSB Device");
    uint8_t descriptor[TUD_CONFIG_DESC_LEN] = {
            //num configs, interface count, string index, total length, attribute, power in mA
            TUD_CONFIG_DESCRIPTOR(1, tinyusb_loaded_interfaces_num, str_index, tinyusb_config_descriptor_len, USB_DEVICE_ATTRIBUTES, USB_DEVICE_POWER)
    };
    memcpy(tinyusb_config_descriptor, descriptor, TUD_CONFIG_DESC_LEN);
    if ((tinyusb_loaded_interfaces_mask == (BIT(USB_INTERFACE_CDC) | BIT(USB_INTERFACE_DFU))) || (tinyusb_loaded_interfaces_mask == BIT(USB_INTERFACE_CDC))) {
        usb_persist_enabled = true;
        log_d("USB Persist enabled");
    }
    log_d("Load Done: if_num: %u, descr_len: %u, if_mask: 0x%x", tinyusb_loaded_interfaces_num, tinyusb_config_descriptor_len, tinyusb_loaded_interfaces_mask);
    return true;
}

static inline char nibble_to_hex_char(uint8_t b)
{
    if (b < 0xa) {
        return '0' + b;
    } else {
        return 'a' + b - 0xa;
    }
}

static void set_usb_serial_num(void)
{
    /* Get the MAC address */
    const uint32_t mac0 = REG_GET_FIELD(EFUSE_RD_MAC_SPI_SYS_0_REG, EFUSE_MAC_0);
    const uint32_t mac1 = REG_GET_FIELD(EFUSE_RD_MAC_SPI_SYS_1_REG, EFUSE_MAC_1);
    uint8_t mac_bytes[6];
    memcpy(mac_bytes, &mac0, 4);
    memcpy(mac_bytes + 4, &mac1, 2);

    /* Convert to UTF16 string */
    uint8_t* srl = (uint8_t*)USB_DEVICE_SERIAL;
    for (int i = 0; i < 6; ++i) {
        uint8_t b = mac_bytes[5 - i]; /* printing from the MSB */
        if (i) {
            *srl++ = ':';
        }
        *srl++ = nibble_to_hex_char(b >> 4);
        *srl++ = nibble_to_hex_char(b & 0xf);
    }
    *srl++ = '\0';
}

static void tinyusb_apply_device_config(tinyusb_device_config_t *config){
    if(config->product_name){
        snprintf(USB_DEVICE_PRODUCT, 126, "%s", config->product_name);
    }
    
    if(config->manufacturer_name){
        snprintf(USB_DEVICE_MANUFACTURER, 126, "%s", config->manufacturer_name);
    }
    
    if(config->serial_number && config->serial_number[0]){
        snprintf(USB_DEVICE_SERIAL, 126, "%s", config->serial_number);
    } else {
        set_usb_serial_num();
    }
    
    if(config->webusb_url){
        snprintf(WEBUSB_URL, 126, "%s", config->webusb_url);
    }

    WEBUSB_ENABLED            = config->webusb_enabled;
    USB_DEVICE_ATTRIBUTES     = config->usb_attributes;
    USB_DEVICE_POWER          = config->usb_power_ma;
    
    tinyusb_device_descriptor.bcdUSB = config->usb_version;
    tinyusb_device_descriptor.idVendor = config->vid;
    tinyusb_device_descriptor.idProduct = config->pid;
    tinyusb_device_descriptor.bcdDevice = config->fw_version;
    tinyusb_device_descriptor.bDeviceClass = config->usb_class;
    tinyusb_device_descriptor.bDeviceSubClass = config->usb_subclass;
    tinyusb_device_descriptor.bDeviceProtocol = config->usb_protocol;
}

static void IRAM_ATTR usb_persist_shutdown_handler(void)
{
    if(usb_persist_mode != RESTART_NO_PERSIST){
        if (usb_persist_enabled) {
            REG_SET_BIT(RTC_CNTL_USB_CONF_REG, RTC_CNTL_IO_MUX_RESET_DISABLE);
            REG_SET_BIT(RTC_CNTL_USB_CONF_REG, RTC_CNTL_USB_RESET_DISABLE);
        }
        if (usb_persist_mode == RESTART_BOOTLOADER) {
            //USB CDC Download
            if (usb_persist_enabled) {
                USB_WRAP.date.val = USBDC_PERSIST_ENA;
            }
            REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
            periph_module_disable(PERIPH_TIMG1_MODULE);
        } else if (usb_persist_mode == RESTART_BOOTLOADER_DFU) {
            //DFU Download
            USB_WRAP.date.val = USBDC_BOOT_DFU;
            REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
            periph_module_disable(PERIPH_TIMG0_MODULE);
            periph_module_disable(PERIPH_TIMG1_MODULE);
        } else if (usb_persist_enabled) {
            //USB Persist reboot
            USB_WRAP.date.val = USBDC_PERSIST_ENA;
        }
        SET_PERI_REG_MASK(RTC_CNTL_OPTIONS0_REG, RTC_CNTL_SW_PROCPU_RST);
    }
}

// USB Device Driver task
// This top level thread processes all usb events and invokes callbacks
static void usb_device_task(void *param) {
    (void)param;
    while(1) tud_task(); // RTOS forever loop
}

/*
 * PUBLIC API
 * */

esp_err_t tinyusb_enable_interface(tinyusb_interface_t interface, uint16_t descriptor_len, tinyusb_descriptor_cb_t cb)
{
    if((interface >= USB_INTERFACE_MAX) || (tinyusb_loaded_interfaces_mask & (1U << interface))){
        log_e("Interface %u not enabled", interface);
        return ESP_FAIL;
    }
    tinyusb_loaded_interfaces_mask |= (1U << interface);
    tinyusb_config_descriptor_len += descriptor_len;
    tinyusb_loaded_interfaces_callbacks[interface] = cb;
    log_d("Interface %u enabled", interface);
    return ESP_OK;
}

esp_err_t tinyusb_init(tinyusb_device_config_t *config) {
    static bool initialized = false;
    if(initialized){
        return ESP_OK;
    }
    initialized = true;
    
    tinyusb_endpoints.val = 0;
    tinyusb_apply_device_config(config);
    if (!tinyusb_load_enabled_interfaces()) {
        initialized = false;
        return ESP_FAIL;
    }

    bool usb_did_persist = (USB_WRAP.date.val == USBDC_PERSIST_ENA);

    if(usb_did_persist && usb_persist_enabled){
        // Enable USB/IO_MUX peripheral reset, if coming from persistent reboot
        REG_CLR_BIT(RTC_CNTL_USB_CONF_REG, RTC_CNTL_IO_MUX_RESET_DISABLE);
        REG_CLR_BIT(RTC_CNTL_USB_CONF_REG, RTC_CNTL_USB_RESET_DISABLE);
    } else {
        // Reset USB module
        periph_module_reset(PERIPH_USB_MODULE);
        periph_module_enable(PERIPH_USB_MODULE);
    }

    if (usb_persist_enabled && esp_register_shutdown_handler(usb_persist_shutdown_handler) != ESP_OK) {
        initialized = false;
        return ESP_FAIL;
    }

    tinyusb_config_t tusb_cfg = {
            .external_phy = false // In the most cases you need to use a `false` value
    };
    esp_err_t err = tinyusb_driver_install(&tusb_cfg);
    if (err != ESP_OK) {
        initialized = false;
        return err;
    }
    xTaskCreate(usb_device_task, "usbd", 4096, NULL, configMAX_PRIORITIES - 1, NULL);
    return err;
}

void usb_persist_restart(restart_type_t mode)
{
    if (usb_persist_enabled && mode < RESTART_TYPE_MAX) {
        usb_persist_mode = mode;
        esp_restart();
    } else {
        log_e("Persistence is not enabled");
    }
}

uint8_t tinyusb_add_string_descriptor(const char * str){
    if(str == NULL || tinyusb_string_descriptor_len >= MAX_STRING_DESCRIPTORS){
        return 0;
    }
    uint8_t index = tinyusb_string_descriptor_len;
    tinyusb_string_descriptor[tinyusb_string_descriptor_len++] = (char*)str;
    return index;
}

uint8_t tinyusb_get_free_duplex_endpoint(void){
    if(!tinyusb_has_available_fifos()){
        log_e("No available IN endpoints");
        return 0;
    }
    for(uint8_t i=1; i<7; i++){
        if((tinyusb_endpoints.in & BIT(i)) == 0 && (tinyusb_endpoints.out & BIT(i)) == 0){
            tinyusb_endpoints.in |= BIT(i);
            tinyusb_endpoints.out |= BIT(i);
            return i;
        }
    }
    log_e("No available duplex endpoints");
    return 0;
}

uint8_t tinyusb_get_free_in_endpoint(void){
    if(!tinyusb_has_available_fifos()){
        log_e("No available IN endpoints");
        return 0;
    }
    for(uint8_t i=1; i<7; i++){
        if((tinyusb_endpoints.in & BIT(i)) == 0 && (tinyusb_endpoints.out & BIT(i)) != 0){
            tinyusb_endpoints.in |= BIT(i);
            return i;
        }
    }
    for(uint8_t i=1; i<7; i++){
        if((tinyusb_endpoints.in & BIT(i)) == 0){
            tinyusb_endpoints.in |= BIT(i);
            return i;
        }
    }
    return 0;
}

uint8_t tinyusb_get_free_out_endpoint(void){
    for(uint8_t i=1; i<7; i++){
        if((tinyusb_endpoints.out & BIT(i)) == 0 && (tinyusb_endpoints.in & BIT(i)) != 0){
            tinyusb_endpoints.out |= BIT(i);
            return i;
        }
    }
    for(uint8_t i=1; i<7; i++){
        if((tinyusb_endpoints.out & BIT(i)) == 0){
            tinyusb_endpoints.out |= BIT(i);
            return i;
        }
    }
    return 0;
}

/*
void usb_dw_reg_dump(void)
{
#define USB_PRINT_REG(r) printf("USB0." #r " = 0x%x;\n", USB0.r)
#define USB_PRINT_IREG(i, r) printf("USB0.in_ep_reg[%u]." #r " = 0x%x;\n", i, USB0.in_ep_reg[i].r)
#define USB_PRINT_OREG(i, r) printf("USB0.out_ep_reg[%u]." #r " = 0x%x;\n", i, USB0.out_ep_reg[i].r)
    uint8_t i;
    USB_PRINT_REG(gotgctl);
    USB_PRINT_REG(gotgint);
    USB_PRINT_REG(gahbcfg);
    USB_PRINT_REG(gusbcfg);
    USB_PRINT_REG(grstctl);
    USB_PRINT_REG(gintsts);
    USB_PRINT_REG(gintmsk);
    USB_PRINT_REG(grxstsr);
    USB_PRINT_REG(grxstsp);
    USB_PRINT_REG(grxfsiz);
    USB_PRINT_REG(gnptxsts);
    USB_PRINT_REG(gpvndctl);
    USB_PRINT_REG(ggpio);
    USB_PRINT_REG(guid);
    USB_PRINT_REG(gsnpsid);
    USB_PRINT_REG(ghwcfg1);
    USB_PRINT_REG(ghwcfg2);
    USB_PRINT_REG(ghwcfg3);
    USB_PRINT_REG(ghwcfg4);
    USB_PRINT_REG(glpmcfg);
    USB_PRINT_REG(gpwrdn);
    USB_PRINT_REG(gdfifocfg);
    USB_PRINT_REG(gadpctl);
    USB_PRINT_REG(hptxfsiz);
    USB_PRINT_REG(hcfg);
    USB_PRINT_REG(hfir);
    USB_PRINT_REG(hfnum);
    USB_PRINT_REG(hptxsts);
    USB_PRINT_REG(haint);
    USB_PRINT_REG(haintmsk);
    USB_PRINT_REG(hflbaddr);
    USB_PRINT_REG(hprt);
    USB_PRINT_REG(dcfg);
    USB_PRINT_REG(dctl);
    USB_PRINT_REG(dsts);
    USB_PRINT_REG(diepmsk);
    USB_PRINT_REG(doepmsk);
    USB_PRINT_REG(daint);
    USB_PRINT_REG(daintmsk);
    USB_PRINT_REG(dtknqr1);
    USB_PRINT_REG(dtknqr2);
    USB_PRINT_REG(dvbusdis);
    USB_PRINT_REG(dvbuspulse);
    USB_PRINT_REG(dtknqr3_dthrctl);
    USB_PRINT_REG(dtknqr4_fifoemptymsk);
    USB_PRINT_REG(deachint);
    USB_PRINT_REG(deachintmsk);
    USB_PRINT_REG(pcgctrl);
    USB_PRINT_REG(pcgctrl1);
    USB_PRINT_REG(gnptxfsiz);
    for (i = 0; i < 4; i++) {
        printf("USB0.dieptxf[%u] = 0x%x;\n", i, USB0.dieptxf[i]);
    }
//    for (i = 0; i < 16; i++) {
//        printf("USB0.diepeachintmsk[%u] = 0x%x;\n", i, USB0.diepeachintmsk[i]);
//    }
//    for (i = 0; i < 16; i++) {
//        printf("USB0.doepeachintmsk[%u] = 0x%x;\n", i, USB0.doepeachintmsk[i]);
//    }
    for (i = 0; i < 7; i++) {
        printf("// EP %u:\n", i);
        USB_PRINT_IREG(i, diepctl);
        USB_PRINT_IREG(i, diepint);
        USB_PRINT_IREG(i, dieptsiz);
        USB_PRINT_IREG(i, diepdma);
        USB_PRINT_IREG(i, dtxfsts);
        USB_PRINT_OREG(i, doepctl);
        USB_PRINT_OREG(i, doepint);
        USB_PRINT_OREG(i, doeptsiz);
        USB_PRINT_OREG(i, doepdma);
    }
}
 */
#endif /* CONFIG_USB_ENABLED */
