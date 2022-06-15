// Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include "sdkconfig.h"
#if CONFIG_TINYUSB_CDC_ENABLED

#include <inttypes.h>
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "Stream.h"

ESP_EVENT_DECLARE_BASE(ARDUINO_USB_CDC_EVENTS);

typedef enum {
    ARDUINO_USB_CDC_ANY_EVENT = ESP_EVENT_ANY_ID,
    ARDUINO_USB_CDC_CONNECTED_EVENT = 0,
    ARDUINO_USB_CDC_DISCONNECTED_EVENT,
    ARDUINO_USB_CDC_LINE_STATE_EVENT,
    ARDUINO_USB_CDC_LINE_CODING_EVENT,
    ARDUINO_USB_CDC_RX_EVENT,
    ARDUINO_USB_CDC_TX_EVENT,
    ARDUINO_USB_CDC_RX_OVERFLOW_EVENT,
    ARDUINO_USB_CDC_MAX_EVENT,
} arduino_usb_cdc_event_t;

typedef union {
    struct {
            bool dtr;
            bool rts;
    } line_state;
    struct {
            uint32_t bit_rate;
            uint8_t  stop_bits; ///< 0: 1 stop bit - 1: 1.5 stop bits - 2: 2 stop bits
            uint8_t  parity;    ///< 0: None - 1: Odd - 2: Even - 3: Mark - 4: Space
            uint8_t  data_bits; ///< can be 5, 6, 7, 8 or 16
    } line_coding;
    struct {
            size_t len;
    } rx;
    struct {
            size_t dropped_bytes;
    } rx_overflow;
} arduino_usb_cdc_event_data_t;

class USBCDC: public Stream
{
public:
    USBCDC(uint8_t itf=0);
    ~USBCDC();

    void onEvent(esp_event_handler_t callback);
    void onEvent(arduino_usb_cdc_event_t event, esp_event_handler_t callback);

    size_t setRxBufferSize(size_t size);
    void setTxTimeoutMs(uint32_t timeout);
    void begin(unsigned long baud=0);
    void end();
    
    int available(void);
    int availableForWrite(void);
    int peek(void);
    int read(void);
    size_t read(uint8_t *buffer, size_t size);
    size_t write(uint8_t);
    size_t write(const uint8_t *buffer, size_t size);
    void flush(void);
    
    inline size_t read(char * buffer, size_t size)
    {
        return read((uint8_t*) buffer, size);
    }
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
    void setDebugOutput(bool);
    operator bool() const;

    void enableReboot(bool enable);
    bool rebootEnabled(void);

    //internal methods
    void _onDFU(void);
    void _onLineState(bool _dtr, bool _rts);
    void _onLineCoding(uint32_t _bit_rate, uint8_t _stop_bits, uint8_t _parity, uint8_t _data_bits);
    void _onRX(void);
    void _onTX(void);
    void _onUnplugged(void);
    
protected:
    uint8_t  itf;
    uint32_t bit_rate;
    uint8_t  stop_bits; ///< 0: 1 stop bit - 1: 1.5 stop bits - 2: 2 stop bits
    uint8_t  parity;    ///< 0: None - 1: Odd - 2: Even - 3: Mark - 4: Space
    uint8_t  data_bits; ///< can be 5, 6, 7, 8 or 16
    bool     dtr;
    bool     rts;
    bool     connected;
    bool     reboot_enable;
    xQueueHandle rx_queue;
    xSemaphoreHandle tx_lock;
    uint32_t tx_timeout_ms;
    
};

#if ARDUINO_USB_CDC_ON_BOOT && !ARDUINO_USB_MODE //Serial used for USB CDC
extern USBCDC Serial;
#endif

#endif /* CONFIG_TINYUSB_CDC_ENABLED */
