// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "esp32-hal-uart.h"
#include "esp32-hal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "rom/ets_sys.h"
#include "esp_attr.h"
#include "esp_intr.h"
#include "rom/uart.h"
#include "soc/uart_reg.h"
#include "soc/uart_struct.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_sig_map.h"
#include "soc/dport_reg.h"
#include "esp_intr_alloc.h"

#define UART_REG_BASE(u)    ((u==0)?DR_REG_UART_BASE:(      (u==1)?DR_REG_UART1_BASE:(    (u==2)?DR_REG_UART2_BASE:0)))
#define UART_RXD_IDX(u)     ((u==0)?U0RXD_IN_IDX:(          (u==1)?U1RXD_IN_IDX:(         (u==2)?U2RXD_IN_IDX:0)))
#define UART_TXD_IDX(u)     ((u==0)?U0TXD_OUT_IDX:(         (u==1)?U1TXD_OUT_IDX:(        (u==2)?U2TXD_OUT_IDX:0)))
#define UART_INTR_SOURCE(u) ((u==0)?ETS_UART0_INTR_SOURCE:( (u==1)?ETS_UART1_INTR_SOURCE:((u==2)?ETS_UART2_INTR_SOURCE:0)))

static int s_uart_debug_nr = 0;

struct uart_struct_t {
    uart_dev_t * dev;
#if !CONFIG_DISABLE_HAL_LOCKS
    xSemaphoreHandle lock;
#endif
    uint8_t num;
    xQueueHandle queue;
    intr_handle_t intr_handle;
};

#if CONFIG_DISABLE_HAL_LOCKS
#define UART_MUTEX_LOCK()
#define UART_MUTEX_UNLOCK()

static uart_t _uart_bus_array[3] = {
    {(volatile uart_dev_t *)(DR_REG_UART_BASE), 0, NULL, NULL},
    {(volatile uart_dev_t *)(DR_REG_UART1_BASE), 1, NULL, NULL},
    {(volatile uart_dev_t *)(DR_REG_UART2_BASE), 2, NULL, NULL}
};
#else
#define UART_MUTEX_LOCK()    do {} while (xSemaphoreTake(uart->lock, portMAX_DELAY) != pdPASS)
#define UART_MUTEX_UNLOCK()  xSemaphoreGive(uart->lock)

static uart_t _uart_bus_array[3] = {
    {(volatile uart_dev_t *)(DR_REG_UART_BASE), NULL, 0, NULL, NULL},
    {(volatile uart_dev_t *)(DR_REG_UART1_BASE), NULL, 1, NULL, NULL},
    {(volatile uart_dev_t *)(DR_REG_UART2_BASE), NULL, 2, NULL, NULL}
};
#endif

static void IRAM_ATTR _uart_isr(void *arg)
{
    uint8_t i, c;
    BaseType_t xHigherPriorityTaskWoken;
    uart_t* uart;

    for(i=0;i<3;i++){
        uart = &_uart_bus_array[i];
        if(uart->intr_handle == NULL){
            continue;
        }
        uart->dev->int_clr.rxfifo_full = 1;
        uart->dev->int_clr.frm_err = 1;
        uart->dev->int_clr.rxfifo_tout = 1;
        while(uart->dev->status.rxfifo_cnt || (uart->dev->mem_rx_status.wr_addr != uart->dev->mem_rx_status.rd_addr)) {
            c = uart->dev->fifo.rw_byte;
            if(uart->queue != NULL && !xQueueIsQueueFullFromISR(uart->queue)) {
                xQueueSendFromISR(uart->queue, &c, &xHigherPriorityTaskWoken);
            }
        }
    }

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

void uartEnableInterrupt(uart_t* uart)
{
    UART_MUTEX_LOCK();
    uart->dev->conf1.rxfifo_full_thrhd = 112;
    uart->dev->conf1.rx_tout_thrhd = 2;
    uart->dev->conf1.rx_tout_en = 1;
    uart->dev->int_ena.rxfifo_full = 1;
    uart->dev->int_ena.frm_err = 1;
    uart->dev->int_ena.rxfifo_tout = 1;
    uart->dev->int_clr.val = 0xffffffff;

    esp_intr_alloc(UART_INTR_SOURCE(uart->num), (int)ESP_INTR_FLAG_IRAM, _uart_isr, NULL, &uart->intr_handle);
    UART_MUTEX_UNLOCK();
}

void uartDisableInterrupt(uart_t* uart)
{
    UART_MUTEX_LOCK();
    uart->dev->conf1.val = 0;
    uart->dev->int_ena.val = 0;
    uart->dev->int_clr.val = 0xffffffff;

    esp_intr_free(uart->intr_handle);
    uart->intr_handle = NULL;

    UART_MUTEX_UNLOCK();
}

void uartDetachRx(uart_t* uart)
{
    if(uart == NULL) {
        return;
    }
    pinMatrixInDetach(UART_RXD_IDX(uart->num), false, false);
    uartDisableInterrupt(uart);
}

void uartDetachTx(uart_t* uart)
{
    if(uart == NULL) {
        return;
    }
    pinMatrixOutDetach(UART_TXD_IDX(uart->num), false, false);
}

void uartAttachRx(uart_t* uart, uint8_t rxPin, bool inverted)
{
    if(uart == NULL || rxPin > 39) {
        return;
    }
    pinMode(rxPin, INPUT);
    pinMatrixInAttach(rxPin, UART_RXD_IDX(uart->num), inverted);
    uartEnableInterrupt(uart);
}

void uartAttachTx(uart_t* uart, uint8_t txPin, bool inverted)
{
    if(uart == NULL || txPin > 39) {
        return;
    }
    pinMode(txPin, OUTPUT);
    pinMatrixOutAttach(txPin, UART_TXD_IDX(uart->num), inverted, false);
}

uart_t* uartBegin(uint8_t uart_nr, uint32_t baudrate, uint32_t config, int8_t rxPin, int8_t txPin, uint16_t queueLen, bool inverted)
{
    if(uart_nr > 2) {
        return NULL;
    }

    if(rxPin == -1 && txPin == -1) {
        return NULL;
    }

    uart_t* uart = &_uart_bus_array[uart_nr];

#if !CONFIG_DISABLE_HAL_LOCKS
    if(uart->lock == NULL) {
        uart->lock = xSemaphoreCreateMutex();
        if(uart->lock == NULL) {
            return NULL;
        }
    }
#endif

    if(queueLen && uart->queue == NULL) {
        uart->queue = xQueueCreate(queueLen, sizeof(uint8_t)); //initialize the queue
        if(uart->queue == NULL) {
            return NULL;
        }
    }
    if(uart_nr == 1){
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_UART1_CLK_EN);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_UART1_RST);
    } else if(uart_nr == 2){
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_UART2_CLK_EN);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_UART2_RST);
    } else {
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_UART_CLK_EN);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_UART_RST);
    }
    uartFlush(uart);
    uartSetBaudRate(uart, baudrate);
    UART_MUTEX_LOCK();
    uart->dev->conf0.val = config;
    #define TWO_STOP_BITS_CONF 0x3
    #define ONE_STOP_BITS_CONF 0x1

    if ( uart->dev->conf0.stop_bit_num == TWO_STOP_BITS_CONF) {
        uart->dev->conf0.stop_bit_num = ONE_STOP_BITS_CONF;
        uart->dev->rs485_conf.dl1_en = 1;
    }
    UART_MUTEX_UNLOCK();

    if(rxPin != -1) {
        uartAttachRx(uart, rxPin, inverted);
    }

    if(txPin != -1) {
        uartAttachTx(uart, txPin, inverted);
    }

    return uart;
}

void uartEnd(uart_t* uart)
{
    if(uart == NULL) {
        return;
    }

    UART_MUTEX_LOCK();
    if(uart->queue != NULL) {
        uint8_t c;
        while(xQueueReceive(uart->queue, &c, 0));
        vQueueDelete(uart->queue);
        uart->queue = NULL;
    }

    uart->dev->conf0.val = 0;

    UART_MUTEX_UNLOCK();

    uartDetachRx(uart);
    uartDetachTx(uart);
}

size_t uartResizeRxBuffer(uart_t * uart, size_t new_size) {
    if(uart == NULL) {
        return;
    }

    UART_MUTEX_LOCK();
    if(uart->queue != NULL) {
        uint8_t c;
        while(xQueueReceive(uart->queue, &c, 0));
        vQueueDelete(uart->queue);
        uart->queue = xQueueCreate(new_size, sizeof(uint8_t));
        if(uart->queue == NULL) {
            return NULL;
        }
    }
    UART_MUTEX_UNLOCK();

    return new_size;
}

uint32_t uartAvailable(uart_t* uart)
{
    if(uart == NULL || uart->queue == NULL) {
        return 0;
    }
    return uxQueueMessagesWaiting(uart->queue);
}

uint32_t uartAvailableForWrite(uart_t* uart)
{
    if(uart == NULL) {
        return 0;
    }
    return 0x7f - uart->dev->status.txfifo_cnt;
}

uint8_t uartRead(uart_t* uart)
{
    if(uart == NULL || uart->queue == NULL) {
        return 0;
    }
    uint8_t c;
    if(xQueueReceive(uart->queue, &c, 0)) {
        return c;
    }
    return 0;
}

uint8_t uartPeek(uart_t* uart)
{
    if(uart == NULL || uart->queue == NULL) {
        return 0;
    }
    uint8_t c;
    if(xQueuePeek(uart->queue, &c, 0)) {
        return c;
    }
    return 0;
}

void uartWrite(uart_t* uart, uint8_t c)
{
    if(uart == NULL) {
        return;
    }
    UART_MUTEX_LOCK();
    while(uart->dev->status.txfifo_cnt == 0x7F);
    uart->dev->fifo.rw_byte = c;
    UART_MUTEX_UNLOCK();
}

void uartWriteBuf(uart_t* uart, const uint8_t * data, size_t len)
{
    if(uart == NULL) {
        return;
    }
    UART_MUTEX_LOCK();
    while(len) {
        while(len && uart->dev->status.txfifo_cnt < 0x7F) {
            uart->dev->fifo.rw_byte = *data++;
            len--;
        }
    }
    UART_MUTEX_UNLOCK();
}

void uartFlush(uart_t* uart)
{
    if(uart == NULL) {
        return;
    }

    UART_MUTEX_LOCK();
    while(uart->dev->status.txfifo_cnt);

    //Due to hardware issue, we can not use fifo_rst to reset uart fifo.
    //See description about UART_TXFIFO_RST and UART_RXFIFO_RST in <<esp32_technical_reference_manual>> v2.6 or later.

    // we read the data out and make `fifo_len == 0 && rd_addr == wr_addr`.
    while(uart->dev->status.rxfifo_cnt != 0 || (uart->dev->mem_rx_status.wr_addr != uart->dev->mem_rx_status.rd_addr)) {
        READ_PERI_REG(UART_FIFO_REG(uart->num));
    }

    UART_MUTEX_UNLOCK();
}

void uartSetBaudRate(uart_t* uart, uint32_t baud_rate)
{
    if(uart == NULL) {
        return;
    }
    UART_MUTEX_LOCK();
    uint32_t clk_div = ((UART_CLK_FREQ<<4)/baud_rate);
    uart->dev->clk_div.div_int = clk_div>>4 ;
    uart->dev->clk_div.div_frag = clk_div & 0xf;
    UART_MUTEX_UNLOCK();
}

uint32_t uartGetBaudRate(uart_t* uart)
{
    if(uart == NULL) {
        return 0;
    }
    uint32_t clk_div = (uart->dev->clk_div.div_int << 4) | (uart->dev->clk_div.div_frag & 0x0F);
    return ((UART_CLK_FREQ<<4)/clk_div);
}

static void IRAM_ATTR uart0_write_char(char c)
{
    while(((ESP_REG(0x01C+DR_REG_UART_BASE) >> UART_TXFIFO_CNT_S) & 0x7F) == 0x7F);
    ESP_REG(DR_REG_UART_BASE) = c;
}

static void IRAM_ATTR uart1_write_char(char c)
{
    while(((ESP_REG(0x01C+DR_REG_UART1_BASE) >> UART_TXFIFO_CNT_S) & 0x7F) == 0x7F);
    ESP_REG(DR_REG_UART1_BASE) = c;
}

static void IRAM_ATTR uart2_write_char(char c)
{
    while(((ESP_REG(0x01C+DR_REG_UART2_BASE) >> UART_TXFIFO_CNT_S) & 0x7F) == 0x7F);
    ESP_REG(DR_REG_UART2_BASE) = c;
}

void uartSetDebug(uart_t* uart)
{
    if(uart == NULL || uart->num > 2) {
        s_uart_debug_nr = -1;
        ets_install_putc1(NULL);
        return;
    }
    if(s_uart_debug_nr == uart->num) {
        return;
    }
    s_uart_debug_nr = uart->num;
    switch(s_uart_debug_nr) {
    case 0:
        ets_install_putc1((void (*)(char)) &uart0_write_char);
        break;
    case 1:
        ets_install_putc1((void (*)(char)) &uart1_write_char);
        break;
    case 2:
        ets_install_putc1((void (*)(char)) &uart2_write_char);
        break;
    default:
        ets_install_putc1(NULL);
        break;
    }
}

int uartGetDebug()
{
    return s_uart_debug_nr;
}

int log_printf(const char *format, ...)
{
    if(s_uart_debug_nr < 0){
        return 0;
    }
    static char loc_buf[64];
    char * temp = loc_buf;
    int len;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    len = vsnprintf(NULL, 0, format, arg);
    va_end(copy);
    if(len >= sizeof(loc_buf)){
        temp = (char*)malloc(len+1);
        if(temp == NULL) {
            return 0;
        }
    }
    vsnprintf(temp, len+1, format, arg);
#if !CONFIG_DISABLE_HAL_LOCKS
    if(_uart_bus_array[s_uart_debug_nr].lock){
        while (xSemaphoreTake(_uart_bus_array[s_uart_debug_nr].lock, portMAX_DELAY) != pdPASS);
        ets_printf("%s", temp);
        xSemaphoreGive(_uart_bus_array[s_uart_debug_nr].lock);
    } else {
        ets_printf("%s", temp);
    }
#else
    ets_printf("%s", temp);
#endif
    va_end(arg);
    if(len > 64){
        free(temp);
    }
    return len;
}
