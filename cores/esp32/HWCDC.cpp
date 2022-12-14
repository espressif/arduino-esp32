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
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S3

#include "esp32-hal.h"
#include "HWCDC.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "esp_intr_alloc.h"
#include "soc/periph_defs.h"
#include "hal/usb_serial_jtag_ll.h"

ESP_EVENT_DEFINE_BASE(ARDUINO_HW_CDC_EVENTS);

static RingbufHandle_t tx_ring_buf = NULL;
static xQueueHandle rx_queue = NULL;
static uint8_t rx_data_buf[64];
static intr_handle_t intr_handle = NULL;
static volatile bool initial_empty = false;
static xSemaphoreHandle tx_lock = NULL;

// workaround for when USB CDC is not connected
static uint32_t tx_timeout_ms = 0;
static bool tx_timeout_change_request = false;

static esp_event_loop_handle_t arduino_hw_cdc_event_loop_handle = NULL;

static esp_err_t arduino_hw_cdc_event_post(esp_event_base_t event_base, int32_t event_id, void *event_data, size_t event_data_size, BaseType_t *task_unblocked){
    if(arduino_hw_cdc_event_loop_handle == NULL){
        return ESP_FAIL;
    }
    return esp_event_isr_post_to(arduino_hw_cdc_event_loop_handle, event_base, event_id, event_data, event_data_size, task_unblocked);
}

static esp_err_t arduino_hw_cdc_event_handler_register_with(esp_event_base_t event_base, int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg){
    if (!arduino_hw_cdc_event_loop_handle) {
        esp_event_loop_args_t event_task_args = {
            .queue_size = 5,
            .task_name = "arduino_hw_cdc_events",
            .task_priority = 5,
            .task_stack_size = 2048,
            .task_core_id = tskNO_AFFINITY
        };
        if (esp_event_loop_create(&event_task_args, &arduino_hw_cdc_event_loop_handle) != ESP_OK) {
            log_e("esp_event_loop_create failed");
        }
    }
    if(arduino_hw_cdc_event_loop_handle == NULL){
        return ESP_FAIL;
    }
    return esp_event_handler_register_with(arduino_hw_cdc_event_loop_handle, event_base, event_id, event_handler, event_handler_arg);
}

static void hw_cdc_isr_handler(void *arg) {
    portBASE_TYPE xTaskWoken = 0;
    uint32_t usbjtag_intr_status = 0;
    arduino_hw_cdc_event_data_t event = {0};
    usbjtag_intr_status = usb_serial_jtag_ll_get_intsts_mask();

    if (usbjtag_intr_status & USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY) {
        // Interrupt tells us the host picked up the data we sent.
        if (usb_serial_jtag_ll_txfifo_writable() == 1) {
            // We disable the interrupt here so that the interrupt won't be triggered if there is no data to send.
            usb_serial_jtag_ll_disable_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
            if(!initial_empty){
                initial_empty = true;
                // First time USB is plugged and the application has not explicitly set TX Timeout, set it to default 100ms.
                // Otherwise, USB is still unplugged and the timeout will be kept as Zero in order to avoid any delay in the
                // application whenever it uses write() and the TX Queue gets full.
                if (!tx_timeout_change_request) {
                    tx_timeout_ms = 100;
                }
                //send event?
                //ets_printf("CONNECTED\n");
                arduino_hw_cdc_event_post(ARDUINO_HW_CDC_EVENTS, ARDUINO_HW_CDC_CONNECTED_EVENT, &event, sizeof(arduino_hw_cdc_event_data_t), &xTaskWoken);
            }
            size_t queued_size;
            uint8_t *queued_buff = (uint8_t *)xRingbufferReceiveUpToFromISR(tx_ring_buf, &queued_size, 64);
            // If the hardware fifo is avaliable, write in it. Otherwise, do nothing.
            if (queued_buff != NULL) {  //Although tx_queued_bytes may be larger than 0. We may have interrupt before xRingbufferSend() was called.
                //Copy the queued buffer into the TX FIFO
                usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
                usb_serial_jtag_ll_write_txfifo(queued_buff, queued_size);
                usb_serial_jtag_ll_txfifo_flush();
                vRingbufferReturnItemFromISR(tx_ring_buf, queued_buff, &xTaskWoken);
                usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
                //send event?
                //ets_printf("TX:%u\n", queued_size);
                event.tx.len = queued_size;
                arduino_hw_cdc_event_post(ARDUINO_HW_CDC_EVENTS, ARDUINO_HW_CDC_TX_EVENT, &event, sizeof(arduino_hw_cdc_event_data_t), &xTaskWoken);
            }
        } else {
            usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
        }
    }

    if (usbjtag_intr_status & USB_SERIAL_JTAG_INTR_SERIAL_OUT_RECV_PKT) {
        // read rx buffer(max length is 64), and send avaliable data to ringbuffer.
        // Ensure the rx buffer size is larger than RX_MAX_SIZE.
        usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_INTR_SERIAL_OUT_RECV_PKT);
        uint32_t rx_fifo_len = usb_serial_jtag_ll_read_rxfifo(rx_data_buf, 64);
        uint32_t i=0;
        for(i=0; i<rx_fifo_len; i++){
            if(rx_queue == NULL || !xQueueSendFromISR(rx_queue, rx_data_buf+i, &xTaskWoken)){
                break;
            }
        }
        //send event?
        //ets_printf("RX:%u/%u\n", i, rx_fifo_len);
        event.rx.len = i;
        arduino_hw_cdc_event_post(ARDUINO_HW_CDC_EVENTS, ARDUINO_HW_CDC_RX_EVENT, &event, sizeof(arduino_hw_cdc_event_data_t), &xTaskWoken);
    }

    if (usbjtag_intr_status & USB_SERIAL_JTAG_INTR_BUS_RESET) {
        usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_INTR_BUS_RESET);
        initial_empty = false;
        usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
        //ets_printf("BUS_RESET\n");
        arduino_hw_cdc_event_post(ARDUINO_HW_CDC_EVENTS, ARDUINO_HW_CDC_BUS_RESET_EVENT, &event, sizeof(arduino_hw_cdc_event_data_t), &xTaskWoken);
    }

    if (xTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void ARDUINO_ISR_ATTR cdc0_write_char(char c) {
    if(xPortInIsrContext()){
        xRingbufferSendFromISR(tx_ring_buf, (void*) (&c), 1, NULL);
    } else {
        xRingbufferSend(tx_ring_buf, (void*) (&c), 1, tx_timeout_ms / portTICK_PERIOD_MS);
    }
    usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
}

HWCDC::HWCDC() {

}

HWCDC::~HWCDC(){
    end();
}

HWCDC::operator bool() const
{
    return initial_empty;
}

void HWCDC::onEvent(esp_event_handler_t callback){
    onEvent(ARDUINO_HW_CDC_ANY_EVENT, callback);
}

void HWCDC::onEvent(arduino_hw_cdc_event_t event, esp_event_handler_t callback){
    arduino_hw_cdc_event_handler_register_with(ARDUINO_HW_CDC_EVENTS, event, callback, this);
}

void HWCDC::begin(unsigned long baud)
{
    if(tx_lock == NULL) {
        tx_lock = xSemaphoreCreateMutex();
    }
    setRxBufferSize(256);//default if not preset
    setTxBufferSize(256);//default if not preset

    usb_serial_jtag_ll_disable_intr_mask(USB_SERIAL_JTAG_LL_INTR_MASK);
    usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_LL_INTR_MASK);
    usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY | USB_SERIAL_JTAG_INTR_SERIAL_OUT_RECV_PKT | USB_SERIAL_JTAG_INTR_BUS_RESET);
    if(!intr_handle && esp_intr_alloc(ETS_USB_SERIAL_JTAG_INTR_SOURCE, 0, hw_cdc_isr_handler, NULL, &intr_handle) != ESP_OK){
        isr_log_e("HW USB CDC failed to init interrupts");
        end();
        return;
    }
    usb_serial_jtag_ll_txfifo_flush();
}

void HWCDC::end()
{
    //Disable tx/rx interrupt.
    usb_serial_jtag_ll_disable_intr_mask(USB_SERIAL_JTAG_LL_INTR_MASK);
    esp_intr_free(intr_handle);
    intr_handle = NULL;
    if(tx_lock != NULL) {
        vSemaphoreDelete(tx_lock);
    }
    setRxBufferSize(0);
    setTxBufferSize(0);
    if (arduino_hw_cdc_event_loop_handle) {
        esp_event_loop_delete(arduino_hw_cdc_event_loop_handle);
        arduino_hw_cdc_event_loop_handle = NULL;
    }
}

void HWCDC::setTxTimeoutMs(uint32_t timeout){
    tx_timeout_ms = timeout;
    // it registers that the user has explicitly requested to use a value as TX timeout
    // used for the workaround with unplugged USB and TX Queue Full that causes a delay on every write()
    tx_timeout_change_request = true;
}

/*
 * WRITING
*/

size_t HWCDC::setTxBufferSize(size_t tx_queue_len){
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

int HWCDC::availableForWrite(void)
{
    if(tx_ring_buf == NULL || tx_lock == NULL){
        return 0;
    }
    if(xSemaphoreTake(tx_lock, tx_timeout_ms / portTICK_PERIOD_MS) != pdPASS){
        return 0;
    }
    size_t a = xRingbufferGetCurFreeSize(tx_ring_buf);
    xSemaphoreGive(tx_lock);
    return a;
}

size_t HWCDC::write(const uint8_t *buffer, size_t size)
{
    if(buffer == NULL || size == 0 || tx_ring_buf == NULL || tx_lock == NULL){
        return 0;
    }
    if(xSemaphoreTake(tx_lock, tx_timeout_ms / portTICK_PERIOD_MS) != pdPASS){
        return 0;
    }
    size_t max_size = xRingbufferGetMaxItemSize(tx_ring_buf);
    size_t space = xRingbufferGetCurFreeSize(tx_ring_buf);
    size_t to_send = size, so_far = 0;

    if(space > size){
        space = size;
    }
    // Non-Blocking method, Sending data to ringbuffer, and handle the data in ISR.
    if(xRingbufferSend(tx_ring_buf, (void*) (buffer), space, 0) != pdTRUE){
        size = 0;
    } else {
        to_send -= space;
        so_far += space;
        // Now trigger the ISR to read data from the ring buffer.
        usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);

        while(to_send){
            if(max_size > to_send){
                max_size = to_send;
            }
            // Blocking method, Sending data to ringbuffer, and handle the data in ISR.
            if(xRingbufferSend(tx_ring_buf, (void*) (buffer+so_far), max_size, tx_timeout_ms / portTICK_PERIOD_MS) != pdTRUE){
                size = so_far;
                break;
            }
            so_far += max_size;
            to_send -= max_size;
            // Now trigger the ISR to read data from the ring buffer.
            usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
        }
    }
    xSemaphoreGive(tx_lock);
    return size;
}

size_t HWCDC::write(uint8_t c)
{
    return write(&c, 1);
}

void HWCDC::flush(void)
{
    if(tx_ring_buf == NULL || tx_lock == NULL){
        return;
    }
    if(xSemaphoreTake(tx_lock, tx_timeout_ms / portTICK_PERIOD_MS) != pdPASS){
        return;
    }
    UBaseType_t uxItemsWaiting = 0;
    vRingbufferGetInfo(tx_ring_buf, NULL, NULL, NULL, NULL, &uxItemsWaiting);
    if(uxItemsWaiting){
        // Now trigger the ISR to read data from the ring buffer.
        usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
    }
    while(uxItemsWaiting){
        delay(5);
        vRingbufferGetInfo(tx_ring_buf, NULL, NULL, NULL, NULL, &uxItemsWaiting);
    }
    xSemaphoreGive(tx_lock);
}

/*
 * READING
*/

size_t HWCDC::setRxBufferSize(size_t rx_queue_len){
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
    if(!tx_ring_buf){
        tx_ring_buf = xRingbufferCreate(rx_queue_len, RINGBUF_TYPE_BYTEBUF);
    }
    return rx_queue_len;
}

int HWCDC::available(void)
{
    if(rx_queue == NULL){
        return -1;
    }
    return uxQueueMessagesWaiting(rx_queue);
}

int HWCDC::peek(void)
{
    if(rx_queue == NULL){
        return -1;
    }
    uint8_t c;
    if(xQueuePeek(rx_queue, &c, 0)) {
        return c;
    }
    return -1;
}

int HWCDC::read(void)
{
    if(rx_queue == NULL){
        return -1;
    }
    uint8_t c = 0;
    if(xQueueReceive(rx_queue, &c, 0)) {
        return c;
    }
    return -1;
}

size_t HWCDC::read(uint8_t *buffer, size_t size)
{
    if(rx_queue == NULL){
        return -1;
    }
    uint8_t c = 0;
    size_t count = 0;
    while(count < size && xQueueReceive(rx_queue, &c, 0)){
        buffer[count++] = c;
    }
    return count;
}

/*
 * DEBUG
*/

void HWCDC::setDebugOutput(bool en)
{
    if(en) {
        uartSetDebug(NULL);
        ets_install_putc1((void (*)(char)) &cdc0_write_char);
    } else {
        ets_install_putc1(NULL);
    }
}

#if ARDUINO_USB_MODE
#if ARDUINO_USB_CDC_ON_BOOT//Serial used for USB CDC
HWCDC Serial;
#else
HWCDC USBSerial;
#endif
#endif

#endif /* CONFIG_TINYUSB_CDC_ENABLED */
