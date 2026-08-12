/*
 * USB Host Mouse Example (callback style)
 *
 * Same as USBHostMouse but uses setReportCallback() so you get direct callbacks
 * when the mouse sends a report (move, button press, wheel). No need to poll
 * available() in loop().
 *
 * Requirements: same as USBHostMouse (CFG_TUH_HID, USB Host board).
 */

#include <Arduino.h>
#include <USBHost.h>
#include <USBHostHID.h>
#include <USBHostHIDReportMapDump.h>
#include <USBHostHIDMouse.h>

#if CFG_TUH_HID
static USBHostHIDReportMapDumper s_hidReportMapDumper(&Serial);
#endif

static void onMouseReport(int8_t x, int8_t y, uint8_t buttons, int8_t wheel, void *arg) {
  (void)arg;
  Serial.printf("mouse: dx=%d dy=%d btns=0x%02x wheel=%d\n", (int)x, (int)y, (unsigned)buttons, (int)wheel);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("USB Host Mouse (callback) example");

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

  // Get callbacks for every mouse report (movement, buttons, wheel)
  USBHostMouse.setReportCallback(onMouseReport, nullptr);

  Serial.println("USB Host started. Plug in a USB mouse.");
}

void loop() {
  USBHost.task();
  delay(1);
}
