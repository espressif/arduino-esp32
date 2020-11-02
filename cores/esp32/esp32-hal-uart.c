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
#include "soc/rtc.h"
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

static void uart_on_apb_change(void * arg, apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb);

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
            if(uart->queue != NULL)  {
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

void uartDetachRx(uart_t* uart, uint8_t rxPin)
{
    if(uart == NULL) {
        return;
    }
    pinMatrixInDetach(rxPin, false, false);
    uartDisableInterrupt(uart);
}

void uartDetachTx(uart_t* uart, uint8_t txPin)
{
    if(uart == NULL) {
        return;
    }
    pinMatrixOutDetach(txPin, false, false);
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

    // tx_idle_num : idle interval after tx FIFO is empty(unit: the time it takes to send one bit under current baudrate)
    // Setting it to 0 prevents line idle time/delays when sending messages with small intervals
    uart->dev->idle_conf.tx_idle_num = 0;  //

    UART_MUTEX_UNLOCK();

    if(rxPin != -1) {
        uartAttachRx(uart, rxPin, inverted);
    }

    if(txPin != -1) {
        uartAttachTx(uart, txPin, inverted);
    }
    addApbChangeCallback(uart, uart_on_apb_change);
    return uart;
}

void uartEnd(uart_t* uart, uint8_t txPin, uint8_t rxPin)
{
    if(uart == NULL) {
        return;
    }
    removeApbChangeCallback(uart, uart_on_apb_change);

    UART_MUTEX_LOCK();
    if(uart->queue != NULL) {
        vQueueDelete(uart->queue);
        uart->queue = NULL;
    }

    uart->dev->conf0.val = 0;

    UART_MUTEX_UNLOCK();

    uartDetachRx(uart, rxPin);
    uartDetachTx(uart, txPin);
}

size_t uartResizeRxBuffer(uart_t * uart, size_t new_size) {
    if(uart == NULL) {
        return 0;
    }

    UART_MUTEX_LOCK();
    if(uart->queue != NULL) {
        vQueueDelete(uart->queue);
        uart->queue = xQueueCreate(new_size, sizeof(uint8_t));
        if(uart->queue == NULL) {
            UART_MUTEX_UNLOCK();
            return NULL;
        }
    }
    UART_MUTEX_UNLOCK();

    return new_size;
}

void uartSetRxInvert(uart_t* uart, bool invert)
{
    if (uart == NULL)
        return;
    
    if (invert)
        uart->dev->conf0.rxd_inv = 1;
    else
        uart->dev->conf0.rxd_inv = 0;
}

uint32_t uartAvailable(uart_t* uart)
{
    if(uart == NULL || uart->queue == NULL) {
        return 0;
    }
    return (uxQueueMessagesWaiting(uart->queue) + uart->dev->status.rxfifo_cnt) ;
}

uint32_t uartAvailableForWrite(uart_t* uart)
{
    if(uart == NULL) {
        return 0;
    }
    return 0x7f - uart->dev->status.txfifo_cnt;
}

void uartRxFifoToQueue(uart_t* uart)
{
	uint8_t c;
    UART_MUTEX_LOCK();
	//disable interrupts
	uart->dev->int_ena.val = 0;
	uart->dev->int_clr.val = 0xffffffff;
	while (uart->dev->status.rxfifo_cnt || (uart->dev->mem_rx_status.wr_addr != uart->dev->mem_rx_status.rd_addr)) {
		c = uart->dev->fifo.rw_byte;
		xQueueSend(uart->queue, &c, 0);
	}
	//enable interrupts
	uart->dev->int_ena.rxfifo_full = 1;
	uart->dev->int_ena.frm_err = 1;
	uart->dev->int_ena.rxfifo_tout = 1;
	uart->dev->int_clr.val = 0xffffffff;
    UART_MUTEX_UNLOCK();
}

uint8_t uartRead(uart_t* uart)
{
    if(uart == NULL || uart->queue == NULL) {
        return 0;
    }
    uint8_t c;
    if ((uxQueueMessagesWaiting(uart->queue) == 0) && (uart->dev->status.rxfifo_cnt > 0))
    {
    	uartRxFifoToQueue(uart);
    }
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
    if ((uxQueueMessagesWaiting(uart->queue) == 0) && (uart->dev->status.rxfifo_cnt > 0))
    {
    	uartRxFifoToQueue(uart);
    }
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
        while(uart->dev->status.txfifo_cnt == 0x7F);
        uart->dev->fifo.rw_byte = *data++;
        len--;
    }
    UART_MUTEX_UNLOCK();
}

void uartFlush(uart_t* uart)
{
    uartFlushTxOnly(uart,true);
}

void uartFlushTxOnly(uart_t* uart, bool txOnly)
{
    if(uart == NULL) {
        return;
    }

    UART_MUTEX_LOCK();
    while(uart->dev->status.txfifo_cnt || uart->dev->status.st_utx_out);
    
    if( !txOnly ){
        //Due to hardware issue, we can not use fifo_rst to reset uart fifo.
        //See description about UART_TXFIFO_RST and UART_RXFIFO_RST in <<esp32_technical_reference_manual>> v2.6 or later.

        // we read the data out and make `fifo_len == 0 && rd_addr == wr_addr`.
        while(uart->dev->status.rxfifo_cnt != 0 || (uart->dev->mem_rx_status.wr_addr != uart->dev->mem_rx_status.rd_addr)) {
            READ_PERI_REG(UART_FIFO_REG(uart->num));
        }

        xQueueReset(uart->queue);
    }
    
    UART_MUTEX_UNLOCK();
}

void uartSetBaudRate(uart_t* uart, uint32_t baud_rate)
{
    if(uart == NULL) {
        return;
    }
    UART_MUTEX_LOCK();
    uint32_t clk_div = ((getApbFrequency()<<4)/baud_rate);
    uart->dev->clk_div.div_int = clk_div>>4 ;
    uart->dev->clk_div.div_frag = clk_div & 0xf;
    UART_MUTEX_UNLOCK();
}

static void uart_on_apb_change(void * arg, apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb)
{
    uart_t* uart = (uart_t*)arg;
    if(ev_type == APB_BEFORE_CHANGE){
        UART_MUTEX_LOCK();
        //disabple interrupt
        uart->dev->int_ena.val = 0;
        uart->dev->int_clr.val = 0xffffffff;
        // read RX fifo
        uint8_t c;
   //     BaseType_t xHigherPriorityTaskWoken;
        while(uart->dev->status.rxfifo_cnt != 0 || (uart->dev->mem_rx_status.wr_addr != uart->dev->mem_rx_status.rd_addr)) {
            c = uart->dev->fifo.rw_byte;
            if(uart->queue != NULL ) {
                xQueueSend(uart->queue, &c, 1); //&xHigherPriorityTaskWoken);
            }
        }
        UART_MUTEX_UNLOCK();
 
        // wait TX empty
        while(uart->dev->status.txfifo_cnt || uart->dev->status.st_utx_out);
    } else {
        //todo:
        // set baudrate
        UART_MUTEX_LOCK();
        uint32_t clk_div = (uart->dev->clk_div.div_int << 4) | (uart->dev->clk_div.div_frag & 0x0F);
        uint32_t baud_rate = ((old_apb<<4)/clk_div);
        clk_div = ((new_apb<<4)/baud_rate);
        uart->dev->clk_div.div_int = clk_div>>4 ;
        uart->dev->clk_div.div_frag = clk_div & 0xf;
        //enable interrupts
        uart->dev->int_ena.rxfifo_full = 1;
        uart->dev->int_ena.frm_err = 1;
        uart->dev->int_ena.rxfifo_tout = 1;
        uart->dev->int_clr.val = 0xffffffff;
        UART_MUTEX_UNLOCK();
    }
}

uint32_t uartGetBaudRate(uart_t* uart)
{
    if(uart == NULL) {
        return 0;
    }

    uint32_t clk_div = (uart->dev->clk_div.div_int << 4) | (uart->dev->clk_div.div_frag & 0x0F);
    if(!clk_div) {
        return 0;
    }

    return ((getApbFrequency()<<4)/clk_div);
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

void uart_install_putc()
{
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

void uartSetDebug(uart_t* uart)
{
    if(uart == NULL || uart->num > 2) {
        s_uart_debug_nr = -1;
        //ets_install_putc1(NULL);
        //return;
    } else
    if(s_uart_debug_nr == uart->num) {
        return;
    } else
    s_uart_debug_nr = uart->num;
    uart_install_putc();
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
        xSemaphoreTake(_uart_bus_array[s_uart_debug_nr].lock, portMAX_DELAY);
        ets_printf("%s", temp);
        xSemaphoreGive(_uart_bus_array[s_uart_debug_nr].lock);
    } else {
        ets_printf("%s", temp);
    }
#else
    ets_printf("%s", temp);
#endif
    va_end(arg);
    if(len >= sizeof(loc_buf)){
        free(temp);
    }
    return len;
}

/*
 * if enough pulses are detected return the minimum high pulse duration + minimum low pulse duration divided by two. 
 * This equals one bit period. If flag is true the function return inmediately, otherwise it waits for enough pulses.
 */
unsigned long uartBaudrateDetect(uart_t *uart, bool flg)
{
    while(uart->dev->rxd_cnt.edge_cnt < 30) { // UART_PULSE_NUM(uart_num)
        if(flg) return 0;
        ets_delay_us(1000);
    }

    UART_MUTEX_LOCK();
    unsigned long ret = ((uart->dev->lowpulse.min_cnt + uart->dev->highpulse.min_cnt) >> 1) + 12;
    UART_MUTEX_UNLOCK();

    return ret;
}

/*
 * To start detection of baud rate with the uart the auto_baud.en bit needs to be cleared and set. The bit period is 
 * detected calling uartBadrateDetect(). The raw baudrate is computed using the UART_CLK_FREQ. The raw baudrate is 
 * rounded to the closed real baudrate.
*/
void uartStartDetectBaudrate(uart_t *uart) {
  if(!uart) return;

  uart->dev->auto_baud.glitch_filt = 0x08;
  uart->dev->auto_baud.en = 0;
  uart->dev->auto_baud.en = 1;
}

unsigned long
uartDetectBaudrate(uart_t *uart)
{
    static bool uartStateDetectingBaudrate = false;

    if(!uartStateDetectingBaudrate) {
        uart->dev->auto_baud.glitch_filt = 0x08;
        uart->dev->auto_baud.en = 0;
        uart->dev->auto_baud.en = 1;
        uartStateDetectingBaudrate = true;
    }

    unsigned long divisor = uartBaudrateDetect(uart, true);
    if (!divisor) {
        return 0;
    }

    uart->dev->auto_baud.en = 0;
    uartStateDetectingBaudrate = false; // Initialize for the next round

    unsigned long baudrate = getApbFrequency() / divisor;

    static const unsigned long default_rates[] = {300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 74880, 115200, 230400, 256000, 460800, 921600, 1843200, 3686400};

    size_t i;
    for (i = 1; i < sizeof(default_rates) / sizeof(default_rates[0]) - 1; i++)	// find the nearest real baudrate
    {
        if (baudrate <= default_rates[i])
        {
            if (baudrate - default_rates[i - 1] < default_rates[i] - baudrate) {
                i--;
            }
            break;
        }
    }

    return default_rates[i];
}

/*
 * Returns the status of the RX state machine, if the value is non-zero the state machine is active.
 */
bool uartRxActive(uart_t* uart) {
    return uart->dev->status.st_urx_out != 0;
}
