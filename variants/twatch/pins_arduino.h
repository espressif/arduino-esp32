#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define NUM_DIGITAL_PINS        SOC_GPIO_PIN_COUNT    // GPIO 0..39
#define NUM_ANALOG_INPUTS       16                    // ESP32 has 16 ADC pins
#define EXTERNAL_NUM_INTERRUPTS NUM_DIGITAL_PINS      // All GPIOs

#define analogInputToDigitalPin(p)  (((p)<NUM_ANALOG_INPUTS)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<NUM_DIGITAL_PINS)?(p):NOT_AN_INTERRUPT)
#define digitalPinHasPWM(p)         (p < 34)  // PWM only for GPIO0..33 - NOT GPIO 34,35,36 and 39

// touch screen
#define TP_SDA              23
#define TP_SCL              32
#define TP_INT              38

// Interrupt IO port
#define RTC_INT             37
#define APX20X_INT          35
#define BMA42X_INT1         39

static const uint8_t TX = 1;
static const uint8_t RX = 3;

//Serial1 Already assigned to GPS LORA
#define TX1                 33
#define RX1                 34

// Already assigned to BMA423 PCF8563 and external extensions
static const uint8_t SDA = 21;
static const uint8_t SCL = 22;
// SPI has been configured as an SD card slot and must be removed when downloading
static const uint8_t SS    = 13;
static const uint8_t MOSI  = 15;
static const uint8_t MISO  = 2;
static const uint8_t SCK   = 14;
// Externally programmable IO
static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

#endif /* Pins_Arduino_h */
