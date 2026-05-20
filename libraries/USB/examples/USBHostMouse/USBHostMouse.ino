/*
 * USB Host Mouse Example
 *
 * Connect a boot-protocol USB HID mouse (ESP32-S2 / S3 / P4, USB host).
 * Same flow as USBHostGamepad: register before begin(), VBUS on S3-USB-OTG.
 *
 * If enumeration crashes (IntegerDivideByZero in TinyUSB), see
 * libraries/USB/patches/README.md when rebuilding Arduino ESP32 libs.
 */

#include <Arduino.h>

#if !SOC_USB_OTG_SUPPORTED
#error This SoC has no USB OTG use ESP32-S2 / S3 / P4 with host support.
#elif (ARDUINO_USB_MODE != 1) && !defined(ARDUINO_ESP32_S3_USB_OTG)
#warning USB host: set USB Mode (e.g. Hardware CDC) OR use ESP32-S3-USB-OTG board.
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("USB host: Tools -> USB Mode -> Hardware CDC, or board ESP32-S3-USB-OTG."));
}
void loop() {}
#else

#include <USBHost.h>
#include <USBHostHID.h>
#include <USBHostHIDReportMapDump.h>
#include <USBHostHIDMouse.h>

#if CFG_TUH_HID
/** Registered first: prints usbhid_parse_report_map() for each HID interface, then yields to the mouse handler. */
static USBHostHIDReportMapDumper s_hidReportMapDumper(&Serial);
#endif

/** 1 = print only when movement, wheel, or buttons change (fewer idle lines). */
#ifndef MOUSE_PRINT_ON_ACTIVITY_ONLY
#define MOUSE_PRINT_ON_ACTIVITY_ONLY 1
#endif

#if MOUSE_PRINT_ON_ACTIVITY_ONLY
static uint8_t s_last_buttons = 0;
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("USB Host Mouse example");

#if CFG_TUH_HID
  USBHostHID.addDevice(&s_hidReportMapDumper);
#endif
  USBHostMouse.registerWithHost();

#if defined(USB_HOST_EN) && defined(DEV_VBUS_EN)
  usbHostEnable(true);
  delay(10);
  usbHostPower(USB_HOST_POWER_VBUS);
  delay(10);
#endif

  if (!USBHost.begin()) {
    Serial.println("USBHost.begin() failed");
    return;
  }
  Serial.println(F("Host ready. Plug mouse after boot or replug; hub helps some boards."));
}

void loop() {
  USBHost.task();

  if (USBHostMouse.mounted() && USBHostMouse.available()) {
    int8_t dx = USBHostMouse.getX();
    int8_t dy = USBHostMouse.getY();
    uint8_t btns = USBHostMouse.getButtons();
    int8_t wh = USBHostMouse.getWheel();

#if MOUSE_PRINT_ON_ACTIVITY_ONLY
    if (dx == 0 && dy == 0 && wh == 0 && btns == s_last_buttons) {
      USBHostMouse.clear();
    } else {
      s_last_buttons = btns;
      Serial.printf("dx=%d dy=%d btns=0x%02x wheel=%d\n",
                    (int)dx, (int)dy, (unsigned)btns, (int)wh);
      USBHostMouse.clear();
    }
#else
    Serial.printf("dx=%d dy=%d btns=0x%02x wheel=%d\n",
                  (int)dx, (int)dy, (unsigned)btns, (int)wh);
    USBHostMouse.clear();
#endif
  }

  delay(1);
}
#endif /* host build allowed */
