/*
 HardwareSerial.h - Hardware serial library for Wiring
 Copyright (c) 2006 Nicholas Zambetti.  All right reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

 Modified 28 September 2010 by Mark Sproul
 Modified 14 August 2012 by Alarus
 Modified 3 December 2013 by Matthijs Kooijman
 Modified 18 December 2014 by Ivan Grokhotkov (esp8266 platform support)
 Modified 31 March 2015 by Markus Sattler (rewrite the code for UART0 + UART1 support in ESP8266)
 Modified 25 April 2015 by Thomas Flayols (add configuration different from 8N1 in ESP8266)
 Modified 13 October 2018 by Jeroen Döll (add baudrate detection)
 Baudrate detection example usage (detection on Serial1):
   void setup() {
     Serial.begin(115200);
     delay(100);
     Serial.println();

     Serial1.begin(0, SERIAL_8N1, -1, -1, true, 11000UL);  // Passing 0 for baudrate to detect it, the last parameter is a timeout in ms

     unsigned long detectedBaudRate = Serial1.baudRate();
     if(detectedBaudRate) {
       Serial.printf("Detected baudrate is %lu\n", detectedBaudRate);
     } else {
       Serial.println("No baudrate detected, Serial1 will not work!");
     }
   }

 Pay attention: the baudrate returned by baudRate() may be rounded, eg 115200 returns 115201
 */

#ifndef HardwareSerial_h
#define HardwareSerial_h

#include <inttypes.h>
#include <functional>
#include "Stream.h"
#include "esp32-hal.h"
#include "soc/soc_caps.h"
#include "HWCDC.h"
#include "USBCDC.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

enum SerialConfig {
  SERIAL_5N1 = 0x8000010,
  SERIAL_6N1 = 0x8000014,
  SERIAL_7N1 = 0x8000018,
  SERIAL_8N1 = 0x800001c,
  SERIAL_5N2 = 0x8000030,
  SERIAL_6N2 = 0x8000034,
  SERIAL_7N2 = 0x8000038,
  SERIAL_8N2 = 0x800003c,
  SERIAL_5E1 = 0x8000012,
  SERIAL_6E1 = 0x8000016,
  SERIAL_7E1 = 0x800001a,
  SERIAL_8E1 = 0x800001e,
  SERIAL_5E2 = 0x8000032,
  SERIAL_6E2 = 0x8000036,
  SERIAL_7E2 = 0x800003a,
  SERIAL_8E2 = 0x800003e,
  SERIAL_5O1 = 0x8000013,
  SERIAL_6O1 = 0x8000017,
  SERIAL_7O1 = 0x800001b,
  SERIAL_8O1 = 0x800001f,
  SERIAL_5O2 = 0x8000033,
  SERIAL_6O2 = 0x8000037,
  SERIAL_7O2 = 0x800003b,
  SERIAL_8O2 = 0x800003f
};

typedef uart_mode_t SerialMode;
typedef uart_hw_flowcontrol_t SerialHwFlowCtrl;

typedef enum {
  UART_NO_ERROR,
  UART_BREAK_ERROR,
  UART_BUFFER_FULL_ERROR,
  UART_FIFO_OVF_ERROR,
  UART_FRAME_ERROR,
  UART_PARITY_ERROR
} hardwareSerial_error_t;

typedef enum {
  UART_CLK_SRC_DEFAULT = UART_SCLK_DEFAULT,
#if SOC_UART_SUPPORT_APB_CLK
  UART_CLK_SRC_APB = UART_SCLK_APB,
#endif
#if SOC_UART_SUPPORT_PLL_F40M_CLK
  UART_CLK_SRC_PLL = UART_SCLK_PLL_F40M,
#elif SOC_UART_SUPPORT_PLL_F80M_CLK
  UART_CLK_SRC_PLL = UART_SCLK_PLL_F80M,
#elif CONFIG_IDF_TARGET_ESP32H2
  UART_CLK_SRC_PLL = UART_SCLK_PLL_F48M,
#endif
#if SOC_UART_SUPPORT_XTAL_CLK
  UART_CLK_SRC_XTAL = UART_SCLK_XTAL,
#endif
#if SOC_UART_SUPPORT_RTC_CLK
  UART_CLK_SRC_RTC = UART_SCLK_RTC,
#endif
#if SOC_UART_SUPPORT_REF_TICK
  UART_CLK_SRC_REF_TICK = UART_SCLK_REF_TICK,
#endif
} SerialClkSrc;

#ifndef ARDUINO_SERIAL_EVENT_TASK_STACK_SIZE
#ifndef CONFIG_ARDUINO_SERIAL_EVENT_TASK_STACK_SIZE
#define ARDUINO_SERIAL_EVENT_TASK_STACK_SIZE 2048
#else
#define ARDUINO_SERIAL_EVENT_TASK_STACK_SIZE CONFIG_ARDUINO_SERIAL_EVENT_TASK_STACK_SIZE
#endif
#endif

#ifndef ARDUINO_SERIAL_EVENT_TASK_PRIORITY
#ifndef CONFIG_ARDUINO_SERIAL_EVENT_TASK_PRIORITY
#define ARDUINO_SERIAL_EVENT_TASK_PRIORITY (configMAX_PRIORITIES - 1)
#else
#define ARDUINO_SERIAL_EVENT_TASK_PRIORITY CONFIG_ARDUINO_SERIAL_EVENT_TASK_PRIORITY
#endif
#endif

#ifndef ARDUINO_SERIAL_EVENT_TASK_RUNNING_CORE
#ifndef CONFIG_ARDUINO_SERIAL_EVENT_TASK_RUNNING_CORE
#define ARDUINO_SERIAL_EVENT_TASK_RUNNING_CORE -1
#else
#define ARDUINO_SERIAL_EVENT_TASK_RUNNING_CORE CONFIG_ARDUINO_SERIAL_EVENT_TASK_RUNNING_CORE
#endif
#endif

// UART0 pins are defined by default by the bootloader.
// The definitions for SOC_* should not be changed unless the bootloader pins
// have changed and you know what you are doing.

#ifndef SOC_RX0
#if CONFIG_IDF_TARGET_ESP32
#define SOC_RX0 (gpio_num_t)3
#elif CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
#define SOC_RX0 (gpio_num_t)44
#elif CONFIG_IDF_TARGET_ESP32C2
#define SOC_RX0 (gpio_num_t)19
#elif CONFIG_IDF_TARGET_ESP32C3
#define SOC_RX0 (gpio_num_t)20
#elif CONFIG_IDF_TARGET_ESP32C6
#define SOC_RX0 (gpio_num_t)17
#elif CONFIG_IDF_TARGET_ESP32H2
#define SOC_RX0 (gpio_num_t)23
#elif CONFIG_IDF_TARGET_ESP32P4
#define SOC_RX0 (gpio_num_t)38
#endif
#endif

#ifndef SOC_TX0
#if CONFIG_IDF_TARGET_ESP32
#define SOC_TX0 (gpio_num_t)1
#elif CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
#define SOC_TX0 (gpio_num_t)43
#elif CONFIG_IDF_TARGET_ESP32C2
#define SOC_TX0 (gpio_num_t)20
#elif CONFIG_IDF_TARGET_ESP32C3
#define SOC_TX0 (gpio_num_t)21
#elif CONFIG_IDF_TARGET_ESP32C6
#define SOC_TX0 (gpio_num_t)16
#elif CONFIG_IDF_TARGET_ESP32H2
#define SOC_TX0 (gpio_num_t)24
#elif CONFIG_IDF_TARGET_ESP32P4
#define SOC_TX0 (gpio_num_t)37
#endif
#endif

// Default pins for UART1 are arbitrary, and defined here for convenience.

#if SOC_UART_HP_NUM > 1
#ifndef RX1
#if CONFIG_IDF_TARGET_ESP32
#define RX1 (gpio_num_t)26
#elif CONFIG_IDF_TARGET_ESP32S2
#define RX1 (gpio_num_t)4
#elif CONFIG_IDF_TARGET_ESP32C2
#define RX1 (gpio_num_t)10
#elif CONFIG_IDF_TARGET_ESP32C3
#define RX1 (gpio_num_t)18
#elif CONFIG_IDF_TARGET_ESP32S3
#define RX1 (gpio_num_t)15
#elif CONFIG_IDF_TARGET_ESP32C6
#define RX1 (gpio_num_t)4
#elif CONFIG_IDF_TARGET_ESP32H2
#define RX1 (gpio_num_t)0
#elif CONFIG_IDF_TARGET_ESP32P4
#define RX1 (gpio_num_t)11
#endif
#endif

#ifndef TX1
#if CONFIG_IDF_TARGET_ESP32
#define TX1 (gpio_num_t)27
#elif CONFIG_IDF_TARGET_ESP32S2
#define TX1 (gpio_num_t)5
#elif CONFIG_IDF_TARGET_ESP32C2
#define TX1 (gpio_num_t)18
#elif CONFIG_IDF_TARGET_ESP32C3
#define TX1 (gpio_num_t)19
#elif CONFIG_IDF_TARGET_ESP32S3
#define TX1 (gpio_num_t)16
#elif CONFIG_IDF_TARGET_ESP32C6
#define TX1 (gpio_num_t)5
#elif CONFIG_IDF_TARGET_ESP32H2
#define TX1 (gpio_num_t)1
#elif CONFIG_IDF_TARGET_ESP32P4
#define TX1 (gpio_num_t)10
#endif
#endif
#endif /* SOC_UART_HP_NUM > 1 */

// Default pins for UART2 are arbitrary, and defined here for convenience.

#if SOC_UART_HP_NUM > 2
#ifndef RX2
#if CONFIG_IDF_TARGET_ESP32
#define RX2 (gpio_num_t)4
#elif CONFIG_IDF_TARGET_ESP32S3
#define RX2 (gpio_num_t)19
#endif
#endif

#ifndef TX2
#if CONFIG_IDF_TARGET_ESP32
#define TX2 (gpio_num_t)25
#elif CONFIG_IDF_TARGET_ESP32S3
#define TX2 (gpio_num_t)20
#endif
#endif
#endif /* SOC_UART_HP_NUM > 2 */

#if SOC_UART_LP_NUM >= 1
#ifndef LP_RX0
#define LP_RX0 (gpio_num_t) LP_U0RXD_GPIO_NUM
#endif

#ifndef LP_TX0
#define LP_TX0 (gpio_num_t) LP_U0TXD_GPIO_NUM
#endif
#endif /* SOC_UART_LP_NUM >= 1 */

typedef std::function<void(void)> OnReceiveCb;
typedef std::function<void(hardwareSerial_error_t)> OnReceiveErrorCb;

class HardwareSerial : public Stream {
public:
  HardwareSerial(uint8_t uart_nr);
  ~HardwareSerial();

  // setRxTimeout sets the timeout after which onReceive callback will be called (after receiving data, it waits for this time of UART rx inactivity to call the callback fnc)
  // param symbols_timeout defines a timeout threshold in uart symbol periods. Setting 0 symbol timeout disables the callback call by timeout.
  //                       Maximum timeout setting is calculacted automatically by IDF. If set above the maximum, it is ignored and an error is printed on Serial0 (check console).
  //                       Examples: Maximum for 11 bits symbol is 92 (SERIAL_8N2, SERIAL_8E1, SERIAL_8O1, etc), Maximum for 10 bits symbol is 101 (SERIAL_8N1).
  //                       For example symbols_timeout=1 defines a timeout equal to transmission time of one symbol (~11 bit) on current baudrate.
  //                       For a baudrate of 9600, SERIAL_8N1 (10 bit symbol) and symbols_timeout = 3, the timeout would be 3 / (9600 / 10) = 3.125 ms
  bool setRxTimeout(uint8_t symbols_timeout);

  // setRxFIFOFull(uint8_t fifoBytes) will set the number of bytes that will trigger UART_INTR_RXFIFO_FULL interrupt and fill up RxRingBuffer
  // This affects some functions such as Serial::available() and Serial.read() because, in a UART flow of receiving data, Serial internal
  // RxRingBuffer will be filled only after these number of bytes arrive or a RX Timeout happens.
  // This parameter can be set to 1 in order to receive byte by byte, but it will also consume more CPU time as the ISR will be activates often.
  bool setRxFIFOFull(uint8_t fifoBytes);

  // onReceive will setup a callback that will be called whenever an UART interruption occurs (UART_INTR_RXFIFO_FULL or UART_INTR_RXFIFO_TOUT)
  // UART_INTR_RXFIFO_FULL interrupt triggers at UART_FULL_THRESH_DEFAULT bytes received (defined as 120 bytes by default in IDF)
  // UART_INTR_RXFIFO_TOUT interrupt triggers at UART_TOUT_THRESH_DEFAULT symbols passed without any reception (defined as 10 symbols by default in IDF)
  // onlyOnTimeout parameter will define how onReceive will behave:
  // Default: true -- The callback will only be called when RX Timeout happens.
  //                  Whole stream of bytes will be ready for being read on the callback function at once.
  //                  This option may lead to Rx Overflow depending on the Rx Buffer Size and number of bytes received in the streaming
  //         false -- The callback will be called when FIFO reaches 120 bytes and also on RX Timeout.
  //                  The stream of incommig bytes will be "split" into blocks of 120 bytes on each callback.
  //                  This option avoid any sort of Rx Overflow, but leaves the UART packet reassembling work to the Application.
  void onReceive(OnReceiveCb function, bool onlyOnTimeout = false);

  // onReceive will be called on error events (see hardwareSerial_error_t)
  void onReceiveError(OnReceiveErrorCb function);

  // eventQueueReset clears all events in the queue (the events that trigger onReceive and onReceiveError) - maybe useful in some use cases
  void eventQueueReset();

  // When pins are changed, it will detach the previous ones
  // if pin is negative, it won't be set/changed and will be kept as is
  // timeout_ms is used in baudrate detection (ESP32, ESP32S2 only)
  // invert will invert RX/TX polarity
  // rxfifo_full_thrhd if the UART Flow Control Threshold in the UART FIFO (max 127)
  void begin(
    unsigned long baud, uint32_t config = SERIAL_8N1, int8_t rxPin = -1, int8_t txPin = -1, bool invert = false, unsigned long timeout_ms = 20000UL,
    uint8_t rxfifo_full_thrhd = 120
  );
  void end(void);
  void updateBaudRate(unsigned long baud);
  int available(void);
  int availableForWrite(void);
  int peek(void);
  int read(void);
  size_t read(uint8_t *buffer, size_t size);
  inline size_t read(char *buffer, size_t size) {
    return read((uint8_t *)buffer, size);
  }
  // Overrides Stream::readBytes() to be faster using IDF
  size_t readBytes(uint8_t *buffer, size_t length);
  size_t readBytes(char *buffer, size_t length) {
    return readBytes((uint8_t *)buffer, length);
  }
  void flush(void);
  void flush(bool txOnly);
  size_t write(uint8_t);
  size_t write(const uint8_t *buffer, size_t size);
  inline size_t write(const char *buffer, size_t size) {
    return write((uint8_t *)buffer, size);
  }
  inline size_t write(const char *s) {
    return write((uint8_t *)s, strlen(s));
  }
  inline size_t write(unsigned long n) {
    return write((uint8_t)n);
  }
  inline size_t write(long n) {
    return write((uint8_t)n);
  }
  inline size_t write(unsigned int n) {
    return write((uint8_t)n);
  }
  inline size_t write(int n) {
    return write((uint8_t)n);
  }
  uint32_t baudRate();
  operator bool() const;

  void setDebugOutput(bool);

  void setRxInvert(bool);

  // Negative Pin Number will keep it unmodified, thus this function can set individual pins
  // setPins() can be called after or before begin()
  // When pins are changed, it will detach the previous ones
  bool setPins(int8_t rxPin, int8_t txPin, int8_t ctsPin = -1, int8_t rtsPin = -1);
  // Enables or disables Hardware Flow Control using RTS and/or CTS pins (must use setAllPins() before)
  //    UART_HW_FLOWCTRL_DISABLE = 0x0   disable hardware flow control
  //    UART_HW_FLOWCTRL_RTS     = 0x1   enable RX hardware flow control (rts)
  //    UART_HW_FLOWCTRL_CTS     = 0x2   enable TX hardware flow control (cts)
  //    UART_HW_FLOWCTRL_CTS_RTS = 0x3   enable hardware flow control
  bool setHwFlowCtrlMode(SerialHwFlowCtrl mode = UART_HW_FLOWCTRL_CTS_RTS, uint8_t threshold = 64);  // 64 is half FIFO Length
  // Used to set RS485 modes such as UART_MODE_RS485_HALF_DUPLEX for Auto RTS function on ESP32
  //    UART_MODE_UART                   = 0x00    mode: regular UART mode
  //    UART_MODE_RS485_HALF_DUPLEX      = 0x01    mode: half duplex RS485 UART mode control by RTS pin
  //    UART_MODE_IRDA                   = 0x02    mode: IRDA UART mode
  //    UART_MODE_RS485_COLLISION_DETECT = 0x03    mode: RS485 collision detection UART mode (used for test purposes)
  //    UART_MODE_RS485_APP_CTRL         = 0x04    mode: application control RS485 UART mode (used for test purposes)
  bool setMode(SerialMode mode);
  // Used to set the UART clock source mode. It must be set before calling begin(), otherwise it won't have any effect.
  // Not all clock source are available to every SoC. The compatible option are listed here:
  // UART_CLK_SRC_DEFAULT      :: any SoC - it will set whatever IDF defines as the default UART Clock Source
  // UART_CLK_SRC_APB          :: ESP32, ESP32-S2, ESP32-C3 and ESP32-S3
  // UART_CLK_SRC_PLL          :: ESP32-C2, ESP32-C5, ESP32-C6, ESP32-C61, ESP32-H2 and ESP32-P4
  // UART_CLK_SRC_XTAL         :: ESP32-C2, ESP32-C3, ESP32-C5, ESP32-C6, ESP32-C61, ESP32-H2, ESP32-S3 and ESP32-P4
  // UART_CLK_SRC_RTC          :: ESP32-C2, ESP32-C3, ESP32-C5, ESP32-C6, ESP32-C61, ESP32-H2, ESP32-S3 and ESP32-P4
  // UART_CLK_SRC_REF_TICK     :: ESP32 and ESP32-S2
  // Note: CLK_SRC_PLL Freq depends on the SoC - ESP32-C2 has 40MHz, ESP32-H2 has 48MHz and ESP32-C5, C6, C61 and P4 has 80MHz
  // Note: ESP32-C6, C61, ESP32-P4 and ESP32-C5 have LP UART that will use only RTC_FAST or XTAL/2 as Clock Source
  bool setClockSource(SerialClkSrc clkSrc);
  size_t setRxBufferSize(size_t new_size);
  size_t setTxBufferSize(size_t new_size);

protected:
  uint8_t _uart_nr;
  uart_t *_uart;
  size_t _rxBufferSize;
  size_t _txBufferSize;
  OnReceiveCb _onReceiveCB;
  OnReceiveErrorCb _onReceiveErrorCB;
  // _onReceive and _rxTimeout have be consistent when timeout is disabled
  bool _onReceiveTimeout;
  uint8_t _rxTimeout, _rxFIFOFull;
  TaskHandle_t _eventTask;
#if !CONFIG_DISABLE_HAL_LOCKS
  SemaphoreHandle_t _lock;
#endif

  void _createEventTask(void *args);
  void _destroyEventTask(void);
  static void _uartEventTask(void *args);
};

extern void serialEventRun(void) __attribute__((weak));

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SERIAL)
#ifndef ARDUINO_USB_CDC_ON_BOOT
#define ARDUINO_USB_CDC_ON_BOOT 0
#endif
#if ARDUINO_USB_CDC_ON_BOOT  //Serial used from Native_USB_CDC | HW_CDC_JTAG
#if ARDUINO_USB_MODE         // Hardware CDC mode
// Arduino Serial is the HW JTAG CDC device
#define Serial HWCDCSerial
#else  // !ARDUINO_USB_MODE -- Native USB Mode
// Arduino Serial is the Native USB CDC device
#define Serial USBSerial
#endif  // ARDUINO_USB_MODE
#else   // !ARDUINO_USB_CDC_ON_BOOT -- Serial is used from UART0
// if not using CDC on Boot, Arduino Serial is the UART0 device
#define Serial Serial0
#endif  // ARDUINO_USB_CDC_ON_BOOT
// There is always Seria0 for UART0
extern HardwareSerial Serial0;
#if SOC_UART_NUM > 1
extern HardwareSerial Serial1;
#endif
#if SOC_UART_NUM > 2
extern HardwareSerial Serial2;
#endif
#if SOC_UART_NUM > 3
extern HardwareSerial Serial3;
#endif
#if SOC_UART_NUM > 4
extern HardwareSerial Serial4;
#endif
#if SOC_UART_NUM > 5
extern HardwareSerial Serial5;
#endif
#endif  //!defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SERIAL)

#endif  // HardwareSerial_h
