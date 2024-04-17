/*
 * This Example demonstrates how to receive Hardware Serial Events
 * This USB interface is available for the ESP32-S3, ESP32-C3, ESP32-C6 and ESP32-H2
 *
 * It will log all events and USB status (plugged/unplugged) into UART0
 * Any data read from UART0 will be sent to the USB CDC
 * Any data read from USB CDC will be sent to the UART0
 *
 * A suggestion is to use Arduino Serial Monitor for the UART0 port
 * and some other serial monitor application for the USB CDC port
 * in order to see the exchanged data and the Hardware Serial Events
 *
 */

#ifndef ARDUINO_USB_MODE
#error This ESP32 SoC has no Native USB interface
#elif ARDUINO_USB_MODE == 0
#warning This sketch should be used when USB is in Hardware CDC and JTAG mode
void setup() {}
void loop() {}
#else

#if !ARDUINO_USB_CDC_ON_BOOT
HWCDC HWCDCSerial;
#endif

// USB Event Callback Function that will log CDC events into UART0
static void usbEventCallback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base == ARDUINO_HW_CDC_EVENTS) {
    switch (event_id) {
      case ARDUINO_HW_CDC_CONNECTED_EVENT:
        Serial0.println("CDC EVENT:: ARDUINO_HW_CDC_CONNECTED_EVENT");
        break;
      case ARDUINO_HW_CDC_BUS_RESET_EVENT:
        Serial0.println("CDC EVENT:: ARDUINO_HW_CDC_BUS_RESET_EVENT");
        break;
      case ARDUINO_HW_CDC_RX_EVENT:
        Serial0.println("\nCDC EVENT:: ARDUINO_HW_CDC_RX_EVENT");
        // sends all bytes read from USB Hardware Serial to UART0
        while (HWCDCSerial.available()) Serial0.write(HWCDCSerial.read());
        break;
      case ARDUINO_HW_CDC_TX_EVENT:
        Serial0.println("CDC EVENT:: ARDUINO_HW_CDC_TX_EVENT");
        break;

      default:
        break;
    }
  }
}

const char* _hwcdc_status[] = {
  " USB Plugged but CDC is NOT connected\r\n",
  " USB Plugged and CDC is connected\r\n",
  " USB Unplugged and CDC NOT connected\r\n",
  " USB Unplugged BUT CDC is connected :: PROBLEM\r\n",
};

const char* HWCDC_Status() {
  int i = HWCDCSerial.isPlugged() ? 0 : 2;
  if (HWCDCSerial.isConnected()) i += 1;
  return _hwcdc_status[i];
}

void setup() {
  HWCDCSerial.onEvent(usbEventCallback);
  HWCDCSerial.begin();

  Serial0.begin(115200);
  Serial0.setDebugOutput(true);
  Serial0.println("Starting...");
}

void loop() {
  static uint32_t counter = 0;

  Serial0.print(counter);
  Serial0.print(HWCDC_Status());

  if (HWCDCSerial) {
    HWCDCSerial.printf("  [%ld] connected\n\r", counter);
  }
  // sends all bytes read from UART0 to USB Hardware Serial
  while (Serial0.available()) HWCDCSerial.write(Serial0.read());
  delay(1000);
  counter++;
}
#endif
