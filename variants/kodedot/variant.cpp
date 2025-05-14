#include "Arduino.h"

void usbHostPower(UsbHostPower_t mode) {
  static UsbHostPower_t m = USB_HOST_POWER_OFF;
  if (m == mode) {
    return;
  }
  if (mode == USB_HOST_POWER_OFF) {
    digitalWrite(LIMIT_EN, LOW);
    if (m == USB_HOST_POWER_VBUS) {
      digitalWrite(DEV_VBUS_EN, LOW);
    } else if (m == USB_HOST_POWER_BAT) {
      digitalWrite(BOOST_EN, LOW);
    }
  } else if (mode == USB_HOST_POWER_VBUS) {
    if (m == USB_HOST_POWER_BAT) {
      digitalWrite(BOOST_EN, LOW);
    }
    digitalWrite(DEV_VBUS_EN, HIGH);
  } else if (mode == USB_HOST_POWER_BAT) {
    if (m == USB_HOST_POWER_VBUS) {
      digitalWrite(DEV_VBUS_EN, LOW);
    }
    digitalWrite(BOOST_EN, HIGH);
  }
  if (mode != USB_HOST_POWER_OFF) {
    digitalWrite(LIMIT_EN, HIGH);
  }
  m = mode;
}

void usbHostEnable(bool enable) {
  digitalWrite(USB_HOST_EN, enable);
}

extern "C" void initVariant(void) {
  // Route USB to Device Side
  pinMode(BOOST_EN, OUTPUT);
  digitalWrite(BOOST_EN, LOW);
  pinMode(LIMIT_EN, OUTPUT);
  digitalWrite(LIMIT_EN, LOW);
  pinMode(DEV_VBUS_EN, OUTPUT);
  digitalWrite(DEV_VBUS_EN, LOW);
  pinMode(USB_HOST_EN, OUTPUT);
  digitalWrite(USB_HOST_EN, LOW);

  // Turn Off LCD
  pinMode(LCD_RST, OUTPUT);
  digitalWrite(LCD_RST, LOW);
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, LOW);
}
