#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "pins_arduino.h"
#include "HardwareSerial.h"

#if CONFIG_IDF_TARGET_ESP32

#ifndef RX1
#define RX1 9
#endif

#ifndef TX1
#define TX1 10
#endif

#ifndef RX2
#define RX2 16
#endif

#ifndef TX2
#define TX2 17
#endif

#else

#ifndef RX1
#define RX1 18
#endif

#ifndef TX1
#define TX1 17
#endif

#endif

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SERIAL)
#if ARDUINO_SERIAL_PORT //Serial used for USB CDC
HardwareSerial Serial0(0);
#else
HardwareSerial Serial(0);
#endif
HardwareSerial Serial1(1);
#if CONFIG_IDF_TARGET_ESP32
HardwareSerial Serial2(2);
#endif
#endif

HardwareSerial::HardwareSerial(int uart_nr) : _uart_nr(uart_nr), _uart(NULL) {}

void HardwareSerial::begin(unsigned long baud, uint32_t config, int8_t rxPin, int8_t txPin, bool invert, unsigned long timeout_ms, uint8_t rxfifo_full_thrhd)
{
    if(0 > _uart_nr || _uart_nr > 2) {
        log_e("Serial number is invalid, please use 0, 1 or 2");
        return;
    }
    if(_uart) {
        end();
    }
    if(_uart_nr == 0 && rxPin < 0 && txPin < 0) {
#if CONFIG_IDF_TARGET_ESP32
        rxPin = 3;
        txPin = 1;
#elif CONFIG_IDF_TARGET_ESP32S2
        rxPin = 44;
        txPin = 43;
#elif CONFIG_IDF_TARGET_ESP32C3
        rxPin = 20;
        txPin = 21;
#endif
    }
    if(_uart_nr == 1 && rxPin < 0 && txPin < 0) {
        rxPin = RX1;
        txPin = TX1;
    }
#if CONFIG_IDF_TARGET_ESP32
    if(_uart_nr == 2 && rxPin < 0 && txPin < 0) {
        rxPin = RX2;
        txPin = TX2;
    }
#endif
    _uart = uartBegin(_uart_nr, baud ? baud : 9600, config, rxPin, txPin, 256, invert, rxfifo_full_thrhd);
    _tx_pin = txPin;
    _rx_pin = rxPin;

    if(!baud) {
        uartStartDetectBaudrate(_uart);
        time_t startMillis = millis();
        unsigned long detectedBaudRate = 0;
        while(millis() - startMillis < timeout_ms && !(detectedBaudRate = uartDetectBaudrate(_uart))) {
            yield();
        }

        end();

        if(detectedBaudRate) {
            delay(100); // Give some time...
            _uart = uartBegin(_uart_nr, detectedBaudRate, config, rxPin, txPin, 256, invert, rxfifo_full_thrhd);
        } else {
            log_e("Could not detect baudrate. Serial data at the port must be present within the timeout for detection to be possible");
            _uart = NULL;
            _tx_pin = 255;
            _rx_pin = 255;
        }
    }
}

void HardwareSerial::updateBaudRate(unsigned long baud)
{
	uartSetBaudRate(_uart, baud);
}

void HardwareSerial::end()
{
    if(uartGetDebug() == _uart_nr) {
        uartSetDebug(0);
    }
    log_v("pins %d %d",_tx_pin, _rx_pin);
    uartEnd(_uart, _tx_pin, _rx_pin);
    _uart = 0;
}

size_t HardwareSerial::setRxBufferSize(size_t new_size) {
    return uartResizeRxBuffer(_uart, new_size);
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
    return true;
}

void HardwareSerial::setRxInvert(bool invert)
{
    uartSetRxInvert(_uart, invert);
}
