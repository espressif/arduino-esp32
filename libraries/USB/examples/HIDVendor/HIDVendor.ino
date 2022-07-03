#if ARDUINO_USB_MODE
#warning This sketch should be used when USB is in OTG mode
void setup(){}
void loop(){}
#else
#include "USB.h"
#include "USBHIDVendor.h"
USBHIDVendor Vendor;

const int buttonPin = 0;
int previousButtonState = HIGH;

static void vendorEventCallback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
  if(event_base == ARDUINO_USB_HID_VENDOR_EVENTS){
    arduino_usb_hid_vendor_event_data_t * data = (arduino_usb_hid_vendor_event_data_t*)event_data;
    switch (event_id){
      case ARDUINO_USB_HID_VENDOR_GET_FEATURE_EVENT:
        Serial.printf("HID VENDOR GET FEATURE: len:%u\n", data->len);
        break;
      case ARDUINO_USB_HID_VENDOR_SET_FEATURE_EVENT:
        Serial.printf("HID VENDOR SET FEATURE: len:%u\n", data->len);
        for(uint16_t i=0; i<data->len; i++){
          Serial.printf("0x%02X ",*(data->buffer));
        }
        Serial.println();
        break;
      case ARDUINO_USB_HID_VENDOR_OUTPUT_EVENT:
        Serial.printf("HID VENDOR OUTPUT: len:%u\n", data->len);
        // for(uint16_t i=0; i<data->len; i++){
        //   Serial.write(Vendor.read());
        // }
        break;
      
      default:
        break;
    }
  }
}

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  Serial.begin(115200);
  Vendor.onEvent(vendorEventCallback);
  Vendor.begin();
  USB.begin();
}

void loop() {
  int buttonState = digitalRead(buttonPin);
  if ((buttonState != previousButtonState) && (buttonState == LOW)) {
    Vendor.println("Hello World!");
  }
  previousButtonState = buttonState;
  while(Vendor.available()){
    Serial.write(Vendor.read());
  }
}
#endif /* ARDUINO_USB_MODE */
