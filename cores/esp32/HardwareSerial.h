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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

typedef enum {
    UART_BREAK_ERROR,
    UART_BUFFER_FULL_ERROR,
    UART_FIFO_OVF_ERROR,
    UART_FRAME_ERROR,
    UART_PARITY_ERROR
} hardwareSerial_error_t;

typedef std::function<void(void)> OnReceiveCb;
typedef std::function<void(hardwareSerial_error_t)> OnReceiveErrorCb;

class HardwareSerial: public Stream
{
public:
    HardwareSerial(int uart_nr);
    ~HardwareSerial();

    // onReceive will setup a callback for whenever UART data is received
    // it will work as UART Rx interrupt -- Using C++ 11 std::fuction 
    void onReceive(OnReceiveCb function);
    void onReceiveError(OnReceiveErrorCb function);
 
    void begin(unsigned long baud, uint32_t config=SERIAL_8N1, int8_t rxPin=-1, int8_t txPin=-1, bool invert=false, unsigned long timeout_ms = 20000UL, uint8_t rxfifo_full_thrhd = 112);
    void end(bool fullyTerminate = true);
    void updateBaudRate(unsigned long baud);
    int available(void);
    int availableForWrite(void);
    int peek(void);
    int read(void);
    size_t read(uint8_t *buffer, size_t size);
    inline size_t read(char * buffer, size_t size)
    {
        return read((uint8_t*) buffer, size);
    }
    void flush(void);
    void flush( bool txOnly);
    size_t write(uint8_t);
    size_t write(const uint8_t *buffer, size_t size);
    inline size_t write(const char * buffer, size_t size)
    {
        return write((uint8_t*) buffer, size);
    }
    inline size_t write(const char * s)
    {
        return write((uint8_t*) s, strlen(s));
    }
    inline size_t write(unsigned long n)
    {
        return write((uint8_t) n);
    }
    inline size_t write(long n)
    {
        return write((uint8_t) n);
    }
    inline size_t write(unsigned int n)
    {
        return write((uint8_t) n);
    }
    inline size_t write(int n)
    {
        return write((uint8_t) n);
    }
    uint32_t baudRate();
    operator bool() const;

    void setDebugOutput(bool);
    
    void setRxInvert(bool);

    // Negative Pin Number will keep it unmodified, thus this function can set individual pins
    // SetPins shall be called after Serial begin()
    void setPins(int8_t rxPin, int8_t txPin, int8_t ctsPin = -1, int8_t rtsPin = -1);
    // Enables or disables Hardware Flow Control using RTS and/or CTS pins (must use setAllPins() before)
    void setHwFlowCtrlMode(uint8_t mode = HW_FLOWCTRL_CTS_RTS, uint8_t threshold = 64);   // 64 is half FIFO Length

    size_t setRxBufferSize(size_t new_size);

protected:
    int _uart_nr;
    uart_t* _uart;
    size_t _rxBufferSize;
    OnReceiveCb _onReceiveCB;
    OnReceiveErrorCb _onReceiveErrorCB;
    TaskHandle_t _eventTask;

    void _createEventTask(void *args);
    void _destroyEventTask(void);
    static void _uartEventTask(void *args);
#if !CONFIG_DISABLE_HAL_LOCKS
    SemaphoreHandle_t _lock;
#endif
};

extern void serialEventRun(void) __attribute__((weak));

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SERIAL)
#ifndef ARDUINO_USB_CDC_ON_BOOT
#define ARDUINO_USB_CDC_ON_BOOT 0
#endif
#if ARDUINO_USB_CDC_ON_BOOT //Serial used for USB CDC
#include "USB.h"
#include "USBCDC.h"
extern HardwareSerial Serial0;
#elif ARDUINO_HW_CDC_ON_BOOT
extern HardwareSerial Serial0;
#else
extern HardwareSerial Serial;
#endif
#if SOC_UART_NUM > 1
extern HardwareSerial Serial1;
#endif
#if SOC_UART_NUM > 2
extern HardwareSerial Serial2;
#endif
#endif

#endif // HardwareSerial_h
