#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#define USB_VID          0xAFD0
#define USB_PID          0x0003
#define USB_MANUFACTURER "Alfredo"
#define USB_PRODUCT      "NoU3"
#define USB_SERIAL       ""  // Empty string for MAC adddress

// User LED
#define LED_BUILTIN 45
#define BUILTIN_LED LED_BUILTIN  // backward compatibility

//static const uint8_t TX = 39;
//static const uint8_t RX = 40;
//#define TX1 TX
//#define RX1 RX

static const uint8_t SDA = -1;
static const uint8_t SCL = -1;

static const uint8_t SS = -1;
static const uint8_t MOSI = -1;
static const uint8_t SCK = -1;
static const uint8_t MISO = -1;

#endif /* Pins_Arduino_h */
