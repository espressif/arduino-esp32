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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "rom/ets_sys.h"
#include "esp_attr.h"
#include "esp_intr.h"
#include "rom/uart.h"
#include "soc/uart_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_sig_map.h"

#define ETS_UART2_INUM  5

#define UART_REG_BASE(u)    ((u==0)?DR_REG_UART_BASE:(      (u==1)?DR_REG_UART1_BASE:(    (u==2)?DR_REG_UART2_BASE:0)))
#define UART_RXD_IDX(u)     ((u==0)?U0RXD_IN_IDX:(          (u==1)?U1RXD_IN_IDX:(         (u==2)?U2RXD_IN_IDX:0)))
#define UART_TXD_IDX(u)     ((u==0)?U0TXD_OUT_IDX:(         (u==1)?U1TXD_OUT_IDX:(        (u==2)?U2TXD_OUT_IDX:0)))
#define UART_INUM(u)        ((u==0)?ETS_UART0_INUM:(        (u==1)?ETS_UART1_INUM:(       (u==2)?ETS_UART2_INUM:0)))
#define UART_INTR_SOURCE(u) ((u==0)?ETS_UART0_INTR_SOURCE:( (u==1)?ETS_UART1_INTR_SOURCE:((u==2)?ETS_UART2_INTR_SOURCE:0)))

static int s_uart_debug_nr = 0;

static void IRAM_ATTR _uart_isr(void *arg)
{
    uint8_t c;
    BaseType_t xHigherPriorityTaskWoken;
    uart_t* uart = (uart_t*)arg;

    uart->dev->int_clr.val = UART_RXFIFO_FULL_INT_ENA | UART_FRM_ERR_INT_ENA | UART_RXFIFO_TOUT_INT_ENA; //Acknowledge the interrupt
    while(uart->dev->status.rxfifo_cnt) {
        c = uart->dev->fifo.rw_byte;
        if(!xQueueIsQueueFullFromISR(uart->queue)) {
            xQueueSendFromISR(uart->queue, &c, &xHigherPriorityTaskWoken);
        }
    }

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

uart_t* uartBegin(uint8_t uart_nr, uint32_t baudrate, uint32_t config, int8_t rxPin, int8_t txPin, uint16_t queueLen, bool inverted)
{
    uint32_t conf1 = 0;

    if(uart_nr > 2) {
        return NULL;
    }

    if(rxPin == -1 && txPin == -1) {
        return NULL;
    }

    uart_t* uart = (uart_t*) malloc(sizeof(uart_t));
    if(uart == 0) {
        return NULL;
    }

    uart->dev = (uart_dev_t *)UART_REG_BASE(uart_nr);
    uart->num = uart_nr;
    uart->inverted = inverted;

    uart->rxPin = rxPin;
    uart->txPin = txPin;
    uart->rxEnabled = (uart->rxPin != -1);
    uart->txEnabled = (uart->txPin != -1);

    uartFlush(uart);

    if(uart->rxEnabled) {
        uart->queue = xQueueCreate(queueLen, sizeof(uint8_t)); //initialize the queue
        if(uart->queue == NULL) {
            free(uart);
            return NULL;
        }

        pinMode(uart->rxPin, INPUT);
        pinMatrixInAttach(uart->rxPin, UART_RXD_IDX(uart->num), uart->inverted);
        intr_matrix_set(APP_CPU_NUM, UART_INTR_SOURCE(uart->num), UART_INUM(uart->num));
        xt_set_interrupt_handler(UART_INUM(uart->num), _uart_isr, uart);
        ESP_INTR_ENABLE(UART_INUM(uart->num));
        conf1 = (112 << UART_RXFIFO_FULL_THRHD_S) | (0x02 << UART_RX_TOUT_THRHD_S) | UART_RX_TOUT_EN;
        uart->dev->int_ena.val = UART_RXFIFO_FULL_INT_ENA | UART_FRM_ERR_INT_ENA | UART_RXFIFO_TOUT_INT_ENA;
        uart->dev->int_clr.val = 0xffff;
    }
    if(uart->txEnabled) {
        pinMode(uart->txPin, OUTPUT);
        pinMatrixOutAttach(uart->txPin, UART_TXD_IDX(uart->num), uart->inverted, false);
    }

    uartSetBaudRate(uart, baudrate);
    uart->dev->conf0.val = config;
    uart->dev->conf1.val = conf1;
    return uart;
}

void uartEnd(uart_t* uart)
{
    if(uart == 0) {
        return;
    }

    if(uart->rxEnabled) {
        pinMode(uart->rxPin, INPUT);
        if(uart->num || uart->rxPin != 3) {
            pinMatrixInDetach(UART_RXD_IDX(uart->num), uart->inverted, false);
        }

        ESP_INTR_DISABLE(UART_INUM(uart->num));
        xt_set_interrupt_handler(UART_INUM(uart->num), NULL, NULL);
        vQueueDelete(uart->queue);
    }
    if(uart->txEnabled) {
        pinMode(uart->txPin, INPUT);
        if(uart->num || uart->txPin != 1) {
            pinMatrixInDetach(UART_TXD_IDX(uart->num), !uart->inverted, uart->inverted);
        }
    }

    uart->dev->conf0.val = 0;
    uart->dev->conf1.val = 0;
    uart->dev->int_ena.val = 0;
    uart->dev->int_clr.val = 0xffff;

    free(uart);
}

uint32_t uartAvailable(uart_t* uart)
{
    return uxQueueMessagesWaiting(uart->queue);
}

uint8_t uartRead(uart_t* uart)
{
    uint8_t c;
    if(xQueueReceive(uart->queue, &c, 0)) {
        return c;
    }
    return 0;
}

uint8_t uartPeek(uart_t* uart)
{
    uint8_t c;
    if(xQueuePeek(uart->queue, &c, 0)) {
        return c;
    }
    return 0;
}

void uartWrite(uart_t* uart, uint8_t c)
{
    while(uart->dev->status.rxfifo_cnt == 0x7F);
    uart->dev->fifo.rw_byte = c;
}

void uartWriteBuf(uart_t* uart, const uint8_t * data, size_t len)
{
    while(len) {
        while(len && uart->dev->status.txfifo_cnt < 0x7F) {
            uart->dev->fifo.rw_byte = *data++;
            len--;
        }
    }
}

void uartFlush(uart_t* uart)
{
    uint32_t tmp = 0x00000000;

    if(uart == 0) {
        return;
    }

    if(uart->rxEnabled) {
        tmp |= UART_RXFIFO_RST;
    }

    if(uart->txEnabled) {
        while(uart->dev->status.txfifo_cnt);
        tmp |= UART_TXFIFO_RST;
    }

    uart->dev->conf0.val |= (tmp);
    uart->dev->conf0.val &= ~(tmp);
}

void uartSetBaudRate(uart_t* uart, uint32_t baud_rate)
{
    if(uart == 0) {
        return;
    }
    uart->baud_rate = baud_rate;
    uint32_t clk_div = ((UART_CLK_FREQ<<4)/baud_rate);
    uart->dev->clk_div.div_int = clk_div>>4 ;
    uart->dev->clk_div.div_frag = clk_div & 0xf;
}

uint32_t uartGetBaudRate(uart_t* uart)
{
    if(uart == 0) {
        return 0;
    }
    return uart->baud_rate;
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



