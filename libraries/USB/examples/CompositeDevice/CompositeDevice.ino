#if ARDUINO_USB_MODE
#warning This sketch should be used when USB is in OTG mode
void setup(){}
void loop(){}
#else
#include "USB.h"
#include "USBHIDMouse.h"
#include "USBHIDKeyboard.h"
#include "USBHIDGamepad.h"
#include "USBHIDConsumerControl.h"
#include "USBHIDSystemControl.h"
#include "USBHIDVendor.h"
#include "FirmwareMSC.h"

#if !ARDUINO_USB_MSC_ON_BOOT
FirmwareMSC MSC_Update;
#endif
#if ARDUINO_USB_CDC_ON_BOOT
#define HWSerial Serial0
#define USBSerial Serial
#else
#define HWSerial Serial
USBCDC USBSerial;
#endif

USBHID HID;
USBHIDKeyboard Keyboard;
USBHIDMouse Mouse;
USBHIDGamepad Gamepad;
USBHIDConsumerControl ConsumerControl;
USBHIDSystemControl SystemControl;
USBHIDVendor Vendor;

const int buttonPin = 0;
int previousButtonState = HIGH;

static void usbEventCallback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
  if(event_base == ARDUINO_USB_EVENTS){
    arduino_usb_event_data_t * data = (arduino_usb_event_data_t*)event_data;
    switch (event_id){
      case ARDUINO_USB_STARTED_EVENT:
        HWSerial.println("USB PLUGGED");
        break;
      case ARDUINO_USB_STOPPED_EVENT:
        HWSerial.println("USB UNPLUGGED");
        break;
      case ARDUINO_USB_SUSPEND_EVENT:
        HWSerial.printf("USB SUSPENDED: remote_wakeup_en: %u\n", data->suspend.remote_wakeup_en);
        break;
      case ARDUINO_USB_RESUME_EVENT:
        HWSerial.println("USB RESUMED");
        break;
      
      default:
        break;
    }
  } else if(event_base == ARDUINO_USB_CDC_EVENTS){
    arduino_usb_cdc_event_data_t * data = (arduino_usb_cdc_event_data_t*)event_data;
    switch (event_id){
      case ARDUINO_USB_CDC_CONNECTED_EVENT:
        HWSerial.println("CDC CONNECTED");
        break;
      case ARDUINO_USB_CDC_DISCONNECTED_EVENT:
        HWSerial.println("CDC DISCONNECTED");
        break;
      case ARDUINO_USB_CDC_LINE_STATE_EVENT:
        HWSerial.printf("CDC LINE STATE: dtr: %u, rts: %u\n", data->line_state.dtr, data->line_state.rts);
        break;
      case ARDUINO_USB_CDC_LINE_CODING_EVENT:
        HWSerial.printf("CDC LINE CODING: bit_rate: %u, data_bits: %u, stop_bits: %u, parity: %u\n", data->line_coding.bit_rate, data->line_coding.data_bits, data->line_coding.stop_bits, data->line_coding.parity);
        break;
      case ARDUINO_USB_CDC_RX_EVENT:
        HWSerial.printf("CDC RX [%u]:", data->rx.len);
        {
            uint8_t buf[data->rx.len];
            size_t len = USBSerial.read(buf, data->rx.len);
            HWSerial.write(buf, len);
        }
        HWSerial.println();
        break;
      case ARDUINO_USB_CDC_RX_OVERFLOW_EVENT:
        HWSerial.printf("CDC RX Overflow of %d bytes", data->rx_overflow.dropped_bytes);
        break;

      default:
        break;
    }
  } else if(event_base == ARDUINO_FIRMWARE_MSC_EVENTS){
    arduino_firmware_msc_event_data_t * data = (arduino_firmware_msc_event_data_t*)event_data;
    switch (event_id){
      case ARDUINO_FIRMWARE_MSC_START_EVENT:
        HWSerial.println("MSC Update Start");
        break;
      case ARDUINO_FIRMWARE_MSC_WRITE_EVENT:
        //HWSerial.printf("MSC Update Write %u bytes at offset %u\n", data->write.size, data->write.offset);
        HWSerial.print(".");
        break;
      case ARDUINO_FIRMWARE_MSC_END_EVENT:
        HWSerial.printf("\nMSC Update End: %u bytes\n", data->end.size);
        break;
      case ARDUINO_FIRMWARE_MSC_ERROR_EVENT:
        HWSerial.printf("MSC Update ERROR! Progress: %u bytes\n", data->error.size);
        break;
      case ARDUINO_FIRMWARE_MSC_POWER_EVENT:
        HWSerial.printf("MSC Update Power: power: %u, start: %u, eject: %u\n", data->power.power_condition, data->power.start, data->power.load_eject);
        break;
      
      default:
        break;
    }
  } else if(event_base == ARDUINO_USB_HID_EVENTS){
    arduino_usb_hid_event_data_t * data = (arduino_usb_hid_event_data_t*)event_data;
    switch (event_id){
      case ARDUINO_USB_HID_SET_PROTOCOL_EVENT:
        HWSerial.printf("HID SET PROTOCOL: %s\n", data->set_protocol.protocol?"REPORT":"BOOT");
        break;
      case ARDUINO_USB_HID_SET_IDLE_EVENT:
        HWSerial.printf("HID SET IDLE: %u\n", data->set_idle.idle_rate);
        break;
      
      default:
        break;
    }
  } else if(event_base == ARDUINO_USB_HID_KEYBOARD_EVENTS){
    arduino_usb_hid_keyboard_event_data_t * data = (arduino_usb_hid_keyboard_event_data_t*)event_data;
    switch (event_id){
      case ARDUINO_USB_HID_KEYBOARD_LED_EVENT:
        HWSerial.printf("HID KEYBOARD LED: NumLock:%u, CapsLock:%u, ScrollLock:%u\n", data->numlock, data->capslock, data->scrolllock);
        break;
      
      default:
        break;
    }
  } else if(event_base == ARDUINO_USB_HID_VENDOR_EVENTS){
    arduino_usb_hid_vendor_event_data_t * data = (arduino_usb_hid_vendor_event_data_t*)event_data;
    switch (event_id){
      case ARDUINO_USB_HID_VENDOR_GET_FEATURE_EVENT:
        HWSerial.printf("HID VENDOR GET FEATURE: len:%u\n", data->len);
        for(uint16_t i=0; i<data->len; i++){
          HWSerial.write(data->buffer[i]?data->buffer[i]:'.');
        }
        HWSerial.println();
        break;
      case ARDUINO_USB_HID_VENDOR_SET_FEATURE_EVENT:
        HWSerial.printf("HID VENDOR SET FEATURE: len:%u\n", data->len);
        for(uint16_t i=0; i<data->len; i++){
          HWSerial.write(data->buffer[i]?data->buffer[i]:'.');
        }
        HWSerial.println();
        break;
      case ARDUINO_USB_HID_VENDOR_OUTPUT_EVENT:
        HWSerial.printf("HID VENDOR OUTPUT: len:%u\n", data->len);
        for(uint16_t i=0; i<data->len; i++){
          HWSerial.write(Vendor.read());
        }
        HWSerial.println();
        break;
      
      default:
        break;
    }
  }
}

void setup() {
  HWSerial.begin(115200);
  HWSerial.setDebugOutput(true);
  
  USB.onEvent(usbEventCallback);
  USBSerial.onEvent(usbEventCallback);
  MSC_Update.onEvent(usbEventCallback);
  HID.onEvent(usbEventCallback);
  Keyboard.onEvent(usbEventCallback);
  Vendor.onEvent(usbEventCallback);
  
  USBSerial.begin();
  MSC_Update.begin();
  Vendor.begin();
  Mouse.begin();
  Keyboard.begin();
  Gamepad.begin();
  ConsumerControl.begin();
  SystemControl.begin();
  USB.begin();
}

void loop() {
  int buttonState = digitalRead(buttonPin);
  if (HID.ready() && buttonState != previousButtonState) {
    previousButtonState = buttonState;
    if (buttonState == LOW) {
      HWSerial.println("Button Pressed");
      USBSerial.println("Button Pressed");
      Vendor.println("Button Pressed");
      Mouse.move(10,10);
      Keyboard.pressRaw(HID_KEY_CAPS_LOCK);
      Gamepad.leftStick(100,100);
      ConsumerControl.press(CONSUMER_CONTROL_VOLUME_INCREMENT);
      //SystemControl.press(SYSTEM_CONTROL_POWER_OFF);
    } else {
      Keyboard.releaseRaw(HID_KEY_CAPS_LOCK);
      Gamepad.leftStick(0,0);
      ConsumerControl.release();
      //SystemControl.release();
      Vendor.println("Button Released");
      USBSerial.println("Button Released");
      HWSerial.println("Button Released");
    }
    delay(100);
  }
  
  while(HWSerial.available()){
    size_t l = HWSerial.available();
    uint8_t b[l];
    l = HWSerial.read(b, l);
    USBSerial.write(b, l);
    if(HID.ready()){
      Vendor.write(b,l);
    }
  }
}
#endif /* ARDUINO_USB_MODE */
