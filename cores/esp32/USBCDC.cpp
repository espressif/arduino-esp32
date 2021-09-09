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
#include "USB.h"
#if CONFIG_TINYUSB_CDC_ENABLED

#include "USBCDC.h"
#include "esp32-hal-tinyusb.h"

ESP_EVENT_DEFINE_BASE(ARDUINO_USB_CDC_EVENTS);
esp_err_t arduino_usb_event_post(esp_event_base_t event_base, int32_t event_id, void *event_data, size_t event_data_size, TickType_t ticks_to_wait);
esp_err_t arduino_usb_event_handler_register_with(esp_event_base_t event_base, int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg);

#define MAX_USB_CDC_DEVICES 2
USBCDC * devices[MAX_USB_CDC_DEVICES] = {NULL, NULL};

static uint16_t load_cdc_descriptor(uint8_t * dst, uint8_t * itf)
{
    uint8_t str_index = tinyusb_add_string_descriptor("TinyUSB CDC");
    uint8_t descriptor[TUD_CDC_DESC_LEN] = {
            // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
            TUD_CDC_DESCRIPTOR(*itf, str_index, 0x85, 64, 0x03, 0x84, 64)
    };
    *itf+=2;
    memcpy(dst, descriptor, TUD_CDC_DESC_LEN);
    return TUD_CDC_DESC_LEN;
}

// Invoked when line state DTR & RTS are changed via SET_CONTROL_LINE_STATE
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    if(itf < MAX_USB_CDC_DEVICES && devices[itf] != NULL){
        devices[itf]->_onLineState(dtr, rts);
    }
}

// Invoked when line coding is change via SET_LINE_CODING
void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const* p_line_coding)
{
    if(itf < MAX_USB_CDC_DEVICES && devices[itf] != NULL){
        devices[itf]->_onLineCoding(p_line_coding->bit_rate, p_line_coding->stop_bits, p_line_coding->parity, p_line_coding->data_bits);
    }
}

// Invoked when received new data
void tud_cdc_rx_cb(uint8_t itf)
{
    if(itf < MAX_USB_CDC_DEVICES && devices[itf] != NULL){
        devices[itf]->_onRX();
    }
}

// Invoked when received send break
void tud_cdc_send_break_cb(uint8_t itf, uint16_t duration_ms){
    //isr_log_v("itf: %u, duration_ms: %u", itf, duration_ms);
}

// Invoked when space becomes available in TX buffer
void tud_cdc_tx_complete_cb(uint8_t itf){
    if(itf < MAX_USB_CDC_DEVICES && devices[itf] != NULL){
        devices[itf]->_onTX();
    }
}

static size_t ring_buf_flush(uint8_t itf, RingbufHandle_t ring_buf){
    if(!tud_cdc_n_connected(itf)){
        return 0;
    }
    size_t max_size = tud_cdc_n_write_available(itf);
    size_t queued_size = 0;
    if(max_size == 0){
        if(tud_cdc_n_write_flush(itf) > 0){
            // no space but we were able to flush some out
            max_size = tud_cdc_n_write_available(itf);
        }
        if(max_size == 0){
            // no space and could not flush
            return 0;
        }
    }
    uint8_t *queued_buff = (uint8_t *)xRingbufferReceiveUpTo(ring_buf, &queued_size, 0, max_size);
    if (queued_buff == NULL) {
        //log_d("nothing to send");
        return 0;
    }
    size_t sent = tud_cdc_n_write(itf, queued_buff, queued_size);
    if(sent && sent < max_size){
        tud_cdc_n_write_flush(itf);
    }
    vRingbufferReturnItem(ring_buf, queued_buff);
    return sent;
}

static size_t ring_buf_write_nb(uint8_t itf, RingbufHandle_t ring_buf, const uint8_t *buffer, size_t size){
    if(buffer == NULL || ring_buf == NULL || size == 0){
        return 0;
    }
    size_t space = xRingbufferGetCurFreeSize(ring_buf);
    if(space == 0){
        // ring buff is full, maybe we can flush some?
        if(ring_buf_flush(itf, ring_buf)){
            space = xRingbufferGetCurFreeSize(ring_buf);
        }
    }
    if(size > space){
        size = space;
    }
    if(size && xRingbufferSend(ring_buf, (void*) (buffer), size, 0) != pdTRUE){
        return 0;
    }
    ring_buf_flush(itf, ring_buf);
    return size;
}

static void ARDUINO_ISR_ATTR cdc0_write_char(char c){
    if(devices[0] != NULL){
        devices[0]->write(c);
    }
}

static void usb_unplugged_cb(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    ((USBCDC*)arg)->_onUnplugged();
}

USBCDC::USBCDC(uint8_t itfn) : itf(itfn), bit_rate(0), stop_bits(0), parity(0), data_bits(0), dtr(false),  rts(false), connected(false), reboot_enable(true), rx_queue(NULL), tx_ring_buf(NULL) {
    tinyusb_enable_interface(USB_INTERFACE_CDC, TUD_CDC_DESC_LEN, load_cdc_descriptor);
    if(itf < MAX_USB_CDC_DEVICES){
        arduino_usb_event_handler_register_with(ARDUINO_USB_EVENTS, ARDUINO_USB_STOPPED_EVENT, usb_unplugged_cb, this);
    }
}

USBCDC::~USBCDC(){
    end();
}

void USBCDC::onEvent(esp_event_handler_t callback){
    onEvent(ARDUINO_USB_CDC_ANY_EVENT, callback);
}
void USBCDC::onEvent(arduino_usb_cdc_event_t event, esp_event_handler_t callback){
    arduino_usb_event_handler_register_with(ARDUINO_USB_CDC_EVENTS, event, callback, this);
}

size_t USBCDC::setRxBufferSize(size_t rx_queue_len){
    if(rx_queue){
        if(!rx_queue_len){
            vQueueDelete(rx_queue);
            rx_queue = NULL;
        }
        return 0;
    }
    rx_queue = xQueueCreate(rx_queue_len, sizeof(uint8_t));
    if(!rx_queue){
        return 0;
    }
    return rx_queue_len;
}

size_t USBCDC::setTxBufferSize(size_t tx_queue_len){
    if(tx_ring_buf){
        if(!tx_queue_len){
            vRingbufferDelete(tx_ring_buf);
            tx_ring_buf = NULL;
        }
        return 0;
    }
    tx_ring_buf = xRingbufferCreate(tx_queue_len, RINGBUF_TYPE_BYTEBUF);
    if(!tx_ring_buf){
        return 0;
    }
    return tx_queue_len;
}

void USBCDC::begin(unsigned long baud)
{
    setRxBufferSize(256);//default if not preset
    setTxBufferSize(256);//default if not preset
    devices[itf] = this;
}

void USBCDC::end()
{
    connected = false;
    devices[itf] = NULL;
    setRxBufferSize(0);
    setTxBufferSize(0);
}

void USBCDC::_onUnplugged(void){
    if(connected){
        connected = false;
        dtr = false;
        rts = false;
        arduino_usb_cdc_event_data_t p = {0};
        arduino_usb_event_post(ARDUINO_USB_CDC_EVENTS, ARDUINO_USB_CDC_DISCONNECTED_EVENT, &p, sizeof(arduino_usb_cdc_event_data_t), portMAX_DELAY);
    }
}

enum { CDC_LINE_IDLE, CDC_LINE_1, CDC_LINE_2, CDC_LINE_3 };
void USBCDC::_onLineState(bool _dtr, bool _rts){
    static uint8_t lineState = CDC_LINE_IDLE;

    if(dtr == _dtr && rts == _rts){
        return; // Skip duplicate events
    }

    dtr = _dtr;
    rts = _rts;

    if(reboot_enable){
        if(!dtr && rts){
            if(lineState == CDC_LINE_IDLE){
                lineState++;
                if(connected){
                    connected = false;
                    arduino_usb_cdc_event_data_t p = {0};
                    arduino_usb_event_post(ARDUINO_USB_CDC_EVENTS, ARDUINO_USB_CDC_DISCONNECTED_EVENT, &p, sizeof(arduino_usb_cdc_event_data_t), portMAX_DELAY);
                }
            } else {
                lineState = CDC_LINE_IDLE;
            }
        } else if(dtr && rts){
            if(lineState == CDC_LINE_1){
                lineState++;
            } else {
                lineState = CDC_LINE_IDLE;
            }
        } else if(dtr && !rts){
            if(lineState == CDC_LINE_2){
                lineState++;
            } else {
                lineState = CDC_LINE_IDLE;
            }
        } else if(!dtr && !rts){
            if(lineState == CDC_LINE_3){
                usb_persist_restart(RESTART_BOOTLOADER);
            } else {
                lineState = CDC_LINE_IDLE;
            }
        }
    }
    
    if(lineState == CDC_LINE_IDLE){
        if(dtr && rts && !connected){
            connected = true;
            arduino_usb_cdc_event_data_t p = {0};
            arduino_usb_event_post(ARDUINO_USB_CDC_EVENTS, ARDUINO_USB_CDC_CONNECTED_EVENT, &p, sizeof(arduino_usb_cdc_event_data_t), portMAX_DELAY);
        } else if(!dtr && connected){
            connected = false;
            arduino_usb_cdc_event_data_t p = {0};
            arduino_usb_event_post(ARDUINO_USB_CDC_EVENTS, ARDUINO_USB_CDC_DISCONNECTED_EVENT, &p, sizeof(arduino_usb_cdc_event_data_t), portMAX_DELAY);
        }
        arduino_usb_cdc_event_data_t l = {0};
        l.line_state.dtr = dtr;
        l.line_state.rts = rts;
        arduino_usb_event_post(ARDUINO_USB_CDC_EVENTS, ARDUINO_USB_CDC_LINE_STATE_EVENT, &l, sizeof(arduino_usb_cdc_event_data_t), portMAX_DELAY);
    }

}

void USBCDC::_onLineCoding(uint32_t _bit_rate, uint8_t _stop_bits, uint8_t _parity, uint8_t _data_bits){
    if(bit_rate != _bit_rate || data_bits != _data_bits || stop_bits != _stop_bits || parity != _parity){
        // ArduinoIDE sends LineCoding with 1200bps baud to reset the device
        if(reboot_enable && _bit_rate == 1200){
            usb_persist_restart(RESTART_BOOTLOADER);
        } else {
            bit_rate = _bit_rate;
            data_bits = _data_bits;
            stop_bits = _stop_bits;
            parity = _parity;
            arduino_usb_cdc_event_data_t p = {0};
            p.line_coding.bit_rate = bit_rate;
            p.line_coding.data_bits = data_bits;
            p.line_coding.stop_bits = stop_bits;
            p.line_coding.parity = parity;
            arduino_usb_event_post(ARDUINO_USB_CDC_EVENTS, ARDUINO_USB_CDC_LINE_CODING_EVENT, &p, sizeof(arduino_usb_cdc_event_data_t), portMAX_DELAY);
        }
    }
}

void USBCDC::_onRX(){
    uint8_t buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE+1];
    uint32_t count = tud_cdc_n_read(itf, buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE);
    for(uint32_t i=0; i<count; i++){
        if(rx_queue == NULL || !xQueueSend(rx_queue, buf+i, 0)){
            return;
        }
    }
    arduino_usb_cdc_event_data_t p = {0};
    p.rx.len = count;
    arduino_usb_event_post(ARDUINO_USB_CDC_EVENTS, ARDUINO_USB_CDC_RX_EVENT, &p, sizeof(arduino_usb_cdc_event_data_t), portMAX_DELAY);
}

void USBCDC::_onTX(){
    ring_buf_flush(itf, tx_ring_buf);
    arduino_usb_cdc_event_data_t p = {0};
    arduino_usb_event_post(ARDUINO_USB_CDC_EVENTS, ARDUINO_USB_CDC_TX_EVENT, &p, sizeof(arduino_usb_cdc_event_data_t), portMAX_DELAY);
}

void  USBCDC::enableReboot(bool enable){
    reboot_enable = enable;
}
bool  USBCDC::rebootEnabled(void){
    return reboot_enable;
}

int USBCDC::available(void)
{
    if(itf >= MAX_USB_CDC_DEVICES || rx_queue == NULL){
        return -1;
    }
    return uxQueueMessagesWaiting(rx_queue);
}

int USBCDC::peek(void)
{
    if(itf >= MAX_USB_CDC_DEVICES || rx_queue == NULL){
        return -1;
    }
    uint8_t c;
    if(xQueuePeek(rx_queue, &c, 0)) {
        return c;
    }
    return -1;
}

int USBCDC::read(void)
{
    if(itf >= MAX_USB_CDC_DEVICES || rx_queue == NULL){
        return -1;
    }
    uint8_t c = 0;
    if(xQueueReceive(rx_queue, &c, 0)) {
        return c;
    }
    return -1;
}

size_t USBCDC::read(uint8_t *buffer, size_t size)
{
    if(itf >= MAX_USB_CDC_DEVICES || rx_queue == NULL){
        return -1;
    }
    uint8_t c = 0;
    size_t count = 0;
    while(count < size && xQueueReceive(rx_queue, &c, 0)){
        buffer[count++] = c;
    }
    return count;
}

void USBCDC::flush(void)
{
    if(itf >= MAX_USB_CDC_DEVICES || tx_ring_buf == NULL){
        return;
    }
    UBaseType_t uxItemsWaiting = 0;
    vRingbufferGetInfo(tx_ring_buf, NULL, NULL, NULL, NULL, &uxItemsWaiting);
    while(uxItemsWaiting){
        ring_buf_flush(itf, tx_ring_buf);
        delay(5);
        vRingbufferGetInfo(tx_ring_buf, NULL, NULL, NULL, NULL, &uxItemsWaiting);
    }
}

int USBCDC::availableForWrite(void)
{
    if(itf >= MAX_USB_CDC_DEVICES || tx_ring_buf == NULL){
        return 0;
    }
    return xRingbufferGetCurFreeSize(tx_ring_buf);
}

size_t USBCDC::write(const uint8_t *buffer, size_t size)
{
    if(itf >= MAX_USB_CDC_DEVICES || tx_ring_buf == NULL){
        return 0;
    }
    size_t sent = ring_buf_write_nb(itf, tx_ring_buf, buffer, size);
    if(sent < size){
        //log_w("sent %u less bytes", size - sent);
        //maybe wait to send the rest?
        //xRingbufferSend(tx_ring_buf, (void*) (buffer+sent), size-sent, timeout_ms / portTICK_PERIOD_MS)
    }
    return sent;
}

size_t USBCDC::write(uint8_t c)
{
    return write(&c, 1);
}

uint32_t  USBCDC::baudRate()
{
    return bit_rate;
}

void USBCDC::setDebugOutput(bool en)
{
    if(en) {
        uartSetDebug(NULL);
        ets_install_putc1((void (*)(char)) &cdc0_write_char);
    } else {
        ets_install_putc1(NULL);
    }
}

USBCDC::operator bool() const
{
    if(itf >= MAX_USB_CDC_DEVICES){
        return false;
    }
    return connected;
}

#if ARDUINO_USB_CDC_ON_BOOT //Serial used for USB CDC
USBCDC Serial(0);
#endif

#endif /* CONFIG_TINYUSB_CDC_ENABLED */
