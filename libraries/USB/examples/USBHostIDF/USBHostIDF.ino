/*
 * ESP-IDF USB Host Example
 *
 * Lists attached devices using the ESP-IDF USB Host Library (usb/usb_host.h). This is
 * not TinyUSB and not the Arduino USBHost class - only one host stack can own the USB
 * peripheral, so do not call USBHost.begin() from this sketch, and select a USB mode
 * that does not start TinyUSB (Tools > USB Mode > "Hardware CDC and JTAG").
 *
 * Since core 3.3.11 the Arduino prebuilt libraries are built with
 * CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK enabled. With that option set,
 * usb_host_config_t::enum_filter_cb has to be provided: ESP-IDF treats a NULL callback
 * as "do not enumerate this device", so every device is rejected without an error and
 * the bus looks dead. See enumFilter() below.
 */

#include <Arduino.h>
#include "soc/soc_caps.h"

#if !SOC_USB_OTG_SUPPORTED
#error "This example needs a target with a USB OTG peripheral, such as ESP32-S3 or ESP32-P4."
#endif

#include "usb/usb_host.h"

/* Weak and empty in the core; the ESP32-S3-USB-OTG variant switches the mux and VBUS. */
extern "C" void USBHostBoardInit(void);

/* Devices are kept open so that removal is reported for them, see deviceAdded(). */
#define MAX_OPEN_DEVICES 8

static usb_host_client_handle_t s_client = NULL;
static usb_device_handle_t s_devices[MAX_OPEN_DEVICES];

#if CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK
/*
 * Called once the device descriptor of a new device has been read. Return false to skip
 * the device, and set bConfigurationValue to choose which configuration to enumerate -
 * 1 unless the device offers several. Must not block and must not submit transfers.
 */
static bool enumFilter(const usb_device_desc_t *dev_desc, uint8_t *bConfigurationValue) {
  (void)dev_desc;
  *bConfigurationValue = 1;
  return true;
}
#endif

static const char *speedName(usb_speed_t speed) {
  switch (speed) {
    case USB_SPEED_LOW:  return "low";
    case USB_SPEED_FULL: return "full";
    case USB_SPEED_HIGH: return "high";
    default:             return "unknown";
  }
}

static void deviceAdded(uint8_t address) {
  usb_device_handle_t dev = NULL;
  if (usb_host_device_open(s_client, address, &dev) != ESP_OK) {
    Serial0.printf("[usbh] could not open device %u\n", address);
    return;
  }

  usb_device_info_t info;
  const usb_device_desc_t *desc = NULL;
  if (usb_host_device_info(dev, &info) == ESP_OK && usb_host_get_device_descriptor(dev, &desc) == ESP_OK) {
    Serial0.printf(
      "[usbh] device %u: %04X:%04X, %s speed, configuration %u\n", address, desc->idVendor, desc->idProduct, speedName(info.speed), info.bConfigurationValue
    );
    usb_print_device_descriptor(desc);
  }

  /* A removal event only arrives for a device this client still holds open. */
  for (int i = 0; i < MAX_OPEN_DEVICES; i++) {
    if (s_devices[i] == NULL) {
      s_devices[i] = dev;
      return;
    }
  }

  Serial0.println("[usbh] no free slot, closing the device again");
  usb_host_device_close(s_client, dev);
}

static void deviceGone(usb_device_handle_t dev) {
  for (int i = 0; i < MAX_OPEN_DEVICES; i++) {
    if (s_devices[i] == dev) {
      s_devices[i] = NULL;
      break;
    }
  }
  usb_host_device_close(s_client, dev);
  Serial0.println("[usbh] device removed");
}

static void clientEvent(const usb_host_client_event_msg_t *msg, void *) {
  switch (msg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:  deviceAdded(msg->new_dev.address); break;
    case USB_HOST_CLIENT_EVENT_DEV_GONE: deviceGone(msg->dev_gone.dev_hdl); break;
    default:                             break;
  }
}

static void daemonTask(void *) {
  while (true) {
    uint32_t event_flags = 0;
    usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
      usb_host_device_free_all();
    }
  }
}

static void clientTask(void *) {
  while (true) {
    usb_host_client_handle_events(s_client, portMAX_DELAY);
  }
}

void setup() {
  Serial0.begin(115200);
  delay(1000);
  Serial0.println("ESP-IDF USB Host example");

  USBHostBoardInit();

  usb_host_config_t host_config = {};
  host_config.intr_flags = ESP_INTR_FLAG_LEVEL1;
#if CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK
  /* Without this nothing enumerates, see the note at the top of this file. */
  host_config.enum_filter_cb = enumFilter;
#endif

  esp_err_t err = usb_host_install(&host_config);
  if (err != ESP_OK) {
    Serial0.printf("usb_host_install() failed: %s\n", esp_err_to_name(err));
    return;
  }

  usb_host_client_config_t client_config = {};
  client_config.is_synchronous = false;
  client_config.max_num_event_msg = 5;
  client_config.async.client_event_callback = clientEvent;
  client_config.async.callback_arg = NULL;

  err = usb_host_client_register(&client_config, &s_client);
  if (err != ESP_OK) {
    Serial0.printf("usb_host_client_register() failed: %s\n", esp_err_to_name(err));
    return;
  }

  xTaskCreate(daemonTask, "usbh_daemon", 4096, NULL, 2, NULL);
  xTaskCreate(clientTask, "usbh_client", 4096, NULL, 3, NULL);

  Serial0.println("Host ready. Plug in a USB device.");
}

void loop() {
  delay(1000);
}
