#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "pins_arduino.h"
#include "HardwareSerial.h"
#include "soc/soc_caps.h"

#ifndef SOC_RX0
#if CONFIG_IDF_TARGET_ESP32
#define SOC_RX0 3
#elif CONFIG_IDF_TARGET_ESP32S2
#define SOC_RX0 44
#elif CONFIG_IDF_TARGET_ESP32C3
#define SOC_RX0 20
#endif
#endif

#ifndef SOC_TX0
#if CONFIG_IDF_TARGET_ESP32
#define SOC_TX0 1
#elif CONFIG_IDF_TARGET_ESP32S2
#define SOC_TX0 43
#elif CONFIG_IDF_TARGET_ESP32C3
#define SOC_TX0 21
#endif
#endif

void serialEvent(void) __attribute__((weak));
void serialEvent(void) {}

#if SOC_UART_NUM > 1

#ifndef RX1
#if CONFIG_IDF_TARGET_ESP32
#define RX1 9
#elif CONFIG_IDF_TARGET_ESP32S2
#define RX1 18
#elif CONFIG_IDF_TARGET_ESP32C3
#define RX1 18
#endif
#endif

#ifndef TX1
#if CONFIG_IDF_TARGET_ESP32
#define TX1 10
#elif CONFIG_IDF_TARGET_ESP32S2
#define TX1 17
#elif CONFIG_IDF_TARGET_ESP32C3
#define TX1 19
#endif
#endif

void serialEvent1(void) __attribute__((weak));
void serialEvent1(void) {}
#endif /* SOC_UART_NUM > 1 */

#if SOC_UART_NUM > 2
#ifndef RX2
#if CONFIG_IDF_TARGET_ESP32
#define RX2 16
#endif
#endif

#ifndef TX2
#if CONFIG_IDF_TARGET_ESP32
#define TX2 17
#endif
#endif

void serialEvent2(void) __attribute__((weak));
void serialEvent2(void) {}
#endif /* SOC_UART_NUM > 2 */

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SERIAL)
#if ARDUINO_USB_CDC_ON_BOOT //Serial used for USB CDC
HardwareSerial Serial0(0);
#elif ARDUINO_HW_CDC_ON_BOOT
HardwareSerial Serial0(0);
#else
HardwareSerial Serial(0);
#endif
#if SOC_UART_NUM > 1
HardwareSerial Serial1(1);
#endif
#if SOC_UART_NUM > 2
HardwareSerial Serial2(2);
#endif
#endif

void serialEventRun(void)
{
#if ARDUINO_USB_CDC_ON_BOOT //Serial used for USB CDC
    if(Serial0.available()) serialEvent();
#elif ARDUINO_HW_CDC_ON_BOOT
    if(Serial0.available()) serialEvent();
#else
    if(Serial.available()) serialEvent();
#endif
#if SOC_UART_NUM > 1
    if(Serial1.available()) serialEvent1();
#endif
#if SOC_UART_NUM > 2
    if(Serial2.available()) serialEvent2();
#endif
}


HardwareSerial::HardwareSerial(int uart_nr) : _uart_nr(uart_nr), _uart(NULL), _rxBufferSize(256) {}

void HardwareSerial::begin(unsigned long baud, uint32_t config, int8_t rxPin, int8_t txPin, bool invert, unsigned long timeout_ms, uint8_t rxfifo_full_thrhd)
{
    if(0 > _uart_nr || _uart_nr >= SOC_UART_NUM) {
        log_e("Serial number is invalid, please use numers from 0 to %u", SOC_UART_NUM - 1);
        return;
    }
    if(_uart) {
        // in this case it is a begin() over a previous begin() - maybe to change baud rate
        // thus do not disable debug output
        end(false);
    }
    if(_uart_nr == 0 && rxPin < 0 && txPin < 0) {
        rxPin = SOC_RX0;
        txPin = SOC_TX0;
    }
#if SOC_UART_NUM > 1
    if(_uart_nr == 1 && rxPin < 0 && txPin < 0) {
        rxPin = RX1;
        txPin = TX1;
    }
#endif
#if SOC_UART_NUM > 2
    if(_uart_nr == 2 && rxPin < 0 && txPin < 0) {
        rxPin = RX2;
        txPin = TX2;
    }
#endif

    _uart = uartBegin(_uart_nr, baud ? baud : 9600, config, rxPin, txPin, _rxBufferSize, invert, rxfifo_full_thrhd);
    if (!baud) {
        // using baud rate as zero, forces it to try to detect the current baud rate in place
        uartStartDetectBaudrate(_uart);
        time_t startMillis = millis();
        unsigned long detectedBaudRate = 0;
        while(millis() - startMillis < timeout_ms && !(detectedBaudRate = uartDetectBaudrate(_uart))) {
            yield();
        }

        end(false);

        if(detectedBaudRate) {
            delay(100); // Give some time...
            _uart = uartBegin(_uart_nr, detectedBaudRate, config, rxPin, txPin, _rxBufferSize, invert, rxfifo_full_thrhd);
        } else {
            log_e("Could not detect baudrate. Serial data at the port must be present within the timeout for detection to be possible");
            _uart = NULL;
        }
    }
}

void HardwareSerial::updateBaudRate(unsigned long baud)
{
	uartSetBaudRate(_uart, baud);
}

void HardwareSerial::end(bool turnOffDebug)
{
    if(turnOffDebug && uartGetDebug() == _uart_nr) {
        uartSetDebug(0);
    }
    delay(10);
    uartEnd(_uart);
    _uart = 0;
}

void HardwareSerial::setDebugOutput(bool en)
{
    if(_uart == 0) {
        return;
    }
    if(en) {
        uartSetDebug(_uart);
    } else {
        if(uartGetDebug() == _uart_nr) {
            uartSetDebug(NULL);
        }
    }
}

int HardwareSerial::available(void)
{
    return uartAvailable(_uart);
}
int HardwareSerial::availableForWrite(void)
{
    return uartAvailableForWrite(_uart);
}

int HardwareSerial::peek(void)
{
    if (available()) {
        return uartPeek(_uart);
    }
    return -1;
}

int HardwareSerial::read(void)
{
    if(available()) {
        return uartRead(_uart);
    }
    return -1;
}

// read characters into buffer
// terminates if size characters have been read, or no further are pending
// returns the number of characters placed in the buffer
// the buffer is NOT null terminated.
size_t HardwareSerial::read(uint8_t *buffer, size_t size)
{
    size_t avail = available();
    if (size < avail) {
        avail = size;
    }
    size_t count = 0;
    while(count < avail) {
        *buffer++ = uartRead(_uart);
        count++;
    }
    return count;
}

void HardwareSerial::flush(void)
{
    uartFlush(_uart);
}

void HardwareSerial::flush(bool txOnly)
{
    uartFlushTxOnly(_uart, txOnly);
}

size_t HardwareSerial::write(uint8_t c)
{
    uartWrite(_uart, c);
    return 1;
}

size_t HardwareSerial::write(const uint8_t *buffer, size_t size)
{
    uartWriteBuf(_uart, buffer, size);
    return size;
}
uint32_t  HardwareSerial::baudRate()

{
	return uartGetBaudRate(_uart);
}
HardwareSerial::operator bool() const
{
    return uartIsDriverInstalled(_uart);
}

void HardwareSerial::setRxInvert(bool invert)
{
    uartSetRxInvert(_uart, invert);
}

void HardwareSerial::setPins(uint8_t rxPin, uint8_t txPin)
{
    uartSetPins(_uart, rxPin, txPin);
}

size_t HardwareSerial::setRxBufferSize(size_t new_size) {

    if (_uart) {
        log_e("RX Buffer can't be resized when Serial is already running.\n");
        return 0;
    }

    if (new_size <= SOC_UART_FIFO_LEN) {
        log_e("RX Buffer must be higher than %d.\n", SOC_UART_FIFO_LEN);
        return 0;
    }

    _rxBufferSize = new_size;
    return _rxBufferSize;
}
