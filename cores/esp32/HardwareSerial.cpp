#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "HardwareSerial.h"

HardwareSerial Serial(0);

HardwareSerial::HardwareSerial(int uart_nr) : _uart_nr(uart_nr), _uart(NULL) {}

void HardwareSerial::begin(unsigned long baud, uint32_t config, int8_t rxPin, int8_t txPin)
{
    if(_uart_nr == 0 && rxPin < 0 && txPin < 0) {
        rxPin = 3;
        txPin = 1;
    }
    _uart = uartBegin(_uart_nr, baud, config, rxPin, txPin, 256, false);
}

void HardwareSerial::end()
{
    if(uartGetDebug() == _uart_nr) {
        uartSetDebug(0);
    }
    uartEnd(_uart);
    _uart = 0;
}

void HardwareSerial::setDebugOutput(bool en)
{
    if(_uart == 0) {
        return;
    }
    if(en) {
        if(_uart->txEnabled) {
            uartSetDebug(_uart);
        } else {
            uartSetDebug(0);
        }
    } else {
        if(uartGetDebug() == _uart_nr) {
            uartSetDebug(0);
        }
    }
}

bool HardwareSerial::isTxEnabled(void)
{
    if(_uart == 0) {
        return false;
    }
    return _uart->txEnabled;
}

bool HardwareSerial::isRxEnabled(void)
{
    if(_uart == 0) {
        return false;
    }
    return _uart->rxEnabled;
}

int HardwareSerial::available(void)
{
    if (_uart && _uart->rxEnabled) {
        return uartAvailable(_uart);
    }
    return 0;
}

int HardwareSerial::peek(void)
{
    if (_uart && _uart->rxEnabled) {
        return uartPeek(_uart);
    }
    return -1;
}

int HardwareSerial::read(void)
{
    if(_uart && _uart->rxEnabled) {
        return uartRead(_uart);
    }
    return -1;
}

void HardwareSerial::flush()
{
    if(_uart == 0 || !_uart->txEnabled) {
        return;
    }
    uartFlush(_uart);
}

size_t HardwareSerial::write(uint8_t c)
{
    if(_uart == 0 || !_uart->txEnabled) {
        return 0;
    }
    uartWrite(_uart, c);
    return 1;
}

size_t HardwareSerial::write(const uint8_t *buffer, size_t size)
{
    if(_uart == 0 || !_uart->txEnabled) {
        return 0;
    }
    uartWriteBuf(_uart, buffer, size);
    return size;
}

HardwareSerial::operator bool() const
{
    return _uart != 0;
}
