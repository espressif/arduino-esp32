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
    int8_t rx_pin;
    bool invertedLogic;
    uint16_t queue_len;
};

#if CONFIG_DISABLE_HAL_LOCKS
#define UART_MUTEX_LOCK()
#define UART_MUTEX_UNLOCK()

static uart_t _uart_bus_array[3] = {
    {(volatile uart_dev_t *)(DR_REG_UART_BASE), 0, NULL, NULL,-1,false,0},
    {(volatile uart_dev_t *)(DR_REG_UART1_BASE), 1, NULL, NULL,-1,false,0},
    {(volatile uart_dev_t *)(DR_REG_UART2_BASE), 2, NULL, NULL,-1,false,0}
};
#else
#define UART_MUTEX_LOCK()    do {} while (xSemaphoreTakeRecursive(uart->lock, portMAX_DELAY) != pdPASS)
#define UART_MUTEX_UNLOCK()  xSemaphoreGiveRecursive(uart->lock)

static uart_t _uart_bus_array[3] = {
    {(volatile uart_dev_t *)(DR_REG_UART_BASE), NULL, 0, NULL, NULL,-1,false,0},
    {(volatile uart_dev_t *)(DR_REG_UART1_BASE), NULL, 1, NULL, NULL,-1,false,0},
    {(volatile uart_dev_t *)(DR_REG_UART2_BASE), NULL, 2, NULL, NULL,-1,false,0}
};
#endif

//#define DEBUG_RX

static bool uart_on_apb_change(void * arg, apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb);

static void _uart_rx_read_fifo( uart_t * uart){
    uint8_t c;
    UART_MUTEX_LOCK();
    UBaseType_t count=0, spaces=0;
    // wr_addr, rd_addr are circular buffer pointers, wrapping is controlled by UART config,
    // standard fifo is 128 byte, but address is 11bits (1024byte buffer, allocated into 6 128byte buffers rx/tx)
    
    count = (uart->dev->mem_rx_status.wr_addr - uart->dev->mem_rx_status.rd_addr)% 0x80;
    if(uart->queue != NULL) {
        spaces=uxQueueSpacesAvailable( uart->queue);
    
        while((spaces >0)&&(count-- > 0)){
        //(uart->dev->status.rxfifo_cnt || (uart->dev->mem_rx_status.wr_addr != uart->dev->mem_rx_status.rd_addr))) {
            c = uart->dev->fifo.rw_byte;
            xQueueSend(uart->queue, &c,0);
            spaces--;
        }
    }  
    if(spaces == 0){ // disable rxFifo_full interrupt, no room in Queue or queue does not exit
        uart->dev->int_ena.rxfifo_full = 0;
        uart->dev->int_ena.rxfifo_tout = 0;
#ifdef DEBUG_RX
        digitalWrite(25,HIGH);
#endif
    }
    UART_MUTEX_UNLOCK();
}

static void IRAM_ATTR _uart_isr(void *arg)
{
    uint8_t i, c;
    BaseType_t xHigherPriorityTaskWoken=pdFALSE;
    uart_t* uart =(uart_t *)arg;
    if( uart != NULL){
        UBaseType_t spaces=0,count=0;
        count = (uart->dev->mem_rx_status.wr_addr - uart->dev->mem_rx_status.rd_addr)%0x80;
        if(uart->queue != NULL) {
            spaces = uart->queue_len - uxQueueMessagesWaitingFromISR( uart->queue);
     
            while((spaces > 0 )&&( (count--) > 0 )){ // if room in queue move from fifo to queue
          // store it
                c = uart->dev->fifo.rw_byte;
                xQueueSendFromISR(uart->queue, &c, &xHigherPriorityTaskWoken);
                spaces--;
            }
        }

        if(spaces==0) { // disable interrupts until space exists in queue
        // if fifo overflows then, characters are lost.  But which characters? top of fifo or bottom?
           uart->dev->int_ena.rxfifo_full = 0;
           uart->dev->int_ena.rxfifo_tout = 0;
#ifdef DEBUG_RX
           digitalWrite(25,HIGH);
#endif
        }
        uart->dev->int_clr.rxfifo_full = 1;
        uart->dev->int_clr.frm_err = 1;
        uart->dev->int_clr.rxfifo_tout = 1;
    }

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

static void uartEnableInterrupt(uart_t* uart)
{
    UART_MUTEX_LOCK();
    uart->dev->int_clr.val = 0xffffffff; // clear all pending interrupt
    uart->dev->int_ena.val = 0;  // disable all new interrupts
    uart->dev->conf1.rxfifo_full_thrhd = 112; // needs speed compensation?
//    uart->dev->conf1.rx_tout_thrhd = 32; // no need to force rxFiFo empty, because uartRead() will empty if necessary
    uart->dev->conf1.rx_tout_en = 0;
    uint32_t flags = ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_SHARED | ESP_INTR_FLAG_LOWMED;
    uint32_t interrupts_enabled =  UART_RXFIFO_OVF_INT_ENA | UART_FRM_ERR_INT_ENA | UART_RXFIFO_FULL_INT_ENA;
    uint32_t ret = esp_intr_alloc_intrstatus(UART_INTR_SOURCE(uart->num), flags, (uint32_t)&uart->dev->int_st.val, interrupts_enabled, _uart_isr,uart, &uart->intr_handle);
    if(ret != ESP_OK ){
        log_e("unable to configure interrupt %u",ret);
    } else {
        uart->dev->int_ena.val = interrupts_enabled;
#ifdef DEBUG_RX
        digitalWrite(25,LOW);
#endif
    }
    UART_MUTEX_UNLOCK();
}

static void uartDisableInterrupt(uart_t* uart)
{
    UART_MUTEX_LOCK();
    uart->dev->conf1.val = 0;
    uart->dev->int_ena.val = 0;
    uart->dev->int_clr.val = 0xffffffff;

    esp_intr_free(uart->intr_handle);
    uart->intr_handle = NULL;

    UART_MUTEX_UNLOCK();
}

static void uartDetachRx(uart_t* uart)
{
    if(uart == NULL) {
        return;
    }
    pinMatrixInDetach(UART_RXD_IDX(uart->num), false, false);
    uartDisableInterrupt(uart);
    uart->rx_pin = -1;
}

static void uartDetachTx(uart_t* uart)
{
    if(uart == NULL) {
        return;
    }
    pinMatrixOutDetach(UART_TXD_IDX(uart->num), false, false);
}

static void uartAttachRx(uart_t* uart, uint8_t rxPin, bool inverted)
{
    if(uart == NULL || rxPin > 39) {
        return;
    }
    uart->rx_pin = rxPin;
    pinMode(rxPin, INPUT_PULLUP);
    pinMatrixInAttach(rxPin, UART_RXD_IDX(uart->num), inverted);
    uartEnableInterrupt(uart);
}

static void uartAttachTx(uart_t* uart, uint8_t txPin, bool inverted)
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
        uart->lock = xSemaphoreCreateRecursiveMutex();
        if(uart->lock == NULL) {
            return NULL;
        }
    }
#endif
    UART_MUTEX_LOCK();
    uart->invertedLogic = inverted;
    if(queueLen && uart->queue == NULL) {
        uart->queue_len = queueLen;
        uart->queue = xQueueCreate(queueLen, sizeof(uint8_t)); //initialize the queue
        if(uart->queue == NULL) {
            uart->queue_len = 0;
            return NULL;
        }
    }
//    uartDetachRx(uart); // release ISR
//    uartDetachTx(uart);

    if(txPin != -1) {
        uartAttachTx(uart, txPin, inverted);
    }

    if(rxPin != -1) {
        uartAttachRx(uart, rxPin, inverted); //Attach ISR interrupt also
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

    uartSetBaudRate(uart, baudrate);
    uart->dev->conf0.val = config;
//    uart->dev->mem_conf.mem_pd=0; // power up Fifo memory
    #define TWO_STOP_BITS_CONF 0x3
    #define ONE_STOP_BITS_CONF 0x1

    if ( uart->dev->conf0.stop_bit_num == TWO_STOP_BITS_CONF) {
        uart->dev->conf0.stop_bit_num = ONE_STOP_BITS_CONF;
        uart->dev->rs485_conf.dl1_en = 1;
    }
    uartFlush(uart);
    UART_MUTEX_UNLOCK();
    addApbChangeCallback(uart, uart_on_apb_change);

    return uart;
}

void uartEnd(uart_t* uart)
{
    if(uart == NULL) {
        return;
    }

    UART_MUTEX_LOCK();

    uartDetachRx(uart); // release pin, disable interrupt
    uartDetachTx(uart); // release pin


    uart->dev->conf0.val = 0;
    // power down uart, disable clocking
    uart->dev->mem_conf.mem_pd=1; // power down Fifo memory
    if(uart->num == 1){
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_UART1_RST);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_UART1_CLK_EN);
    } else if(uart->num == 2){
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_UART2_RST);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_UART2_CLK_EN);
    } else {
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_UART_RST);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_UART_CLK_EN);
    }
    if(uart->queue != NULL) {
        uint8_t c;
        while(xQueueReceive(uart->queue, &c, 0));
        vQueueDelete(uart->queue);
        uart->queue = NULL;
    }
    removeApbChangeCallback(uart, uart_on_apb_change);

    UART_MUTEX_UNLOCK();

}

static bool uart_on_apb_change(void * arg, apb_change_ev_t ev_type, uint32_t old_apb, uint32_t new_apb)
{
    uart_t* uart = (uart_t*)arg;
    bool success=true;
    int8_t saveRxPin;
    uint32_t clk_div,baud_rate;
    switch (ev_type){
        case APB_BEFORE_CHANGE:
            if(new_apb < 3000000) {
                success=false;
                break;
            }
            UART_MUTEX_LOCK();
            saveRxPin = uart->rx_pin;
            uartDetachRx(uart); // release pin, disable interrupt
            uart->rx_pin =saveRxPin;
            uartDrain(uart);
            break;
        case APB_ABORT_CHANGE:
            uartAttachRx(uart,uart->rx_pin,uart->invertedLogic);
            UART_MUTEX_UNLOCK();
            break;
        case APB_AFTER_CHANGE:
        // set baudrate
            clk_div = (uart->dev->clk_div.div_int << 4) | (uart->dev->clk_div.div_frag & 0x0F);
            baud_rate = ((old_apb<<4)/clk_div);
            clk_div = ((new_apb<<4)/baud_rate);
            uart->dev->clk_div.div_int = clk_div>>4 ;
            uart->dev->clk_div.div_frag = clk_div & 0xf;
        // attach rx
            uartAttachRx(uart,uart->rx_pin,uart->invertedLogic);
            UART_MUTEX_UNLOCK();
            break;
        default : ;
    }
    return success;
}

size_t uartResizeRxBuffer(uart_t * uart, size_t new_size) {
    if(uart == NULL) {
        return 0;
    }

    UART_MUTEX_LOCK();
    if(uart->queue != NULL) {
        uart->dev->int_ena.rxfifo_full = 0; // shut off interrupt
        uart->dev->int_ena.rxfifo_tout = 0;
#ifdef DEBUG_RX
        digitalWrite(25,HIGH);
#endif
        vQueueDelete(uart->queue);
        uart->queue_len = 0;
    }
    if( new_size > 0){
        uart->queue_len = new_size;
        uart->queue = xQueueCreate(new_size, sizeof(uint8_t));
        if(uart->queue == NULL) {
            uart->queue_len = 0;
            return NULL;
        } else { // reEnable interrupt
            uart->dev->int_ena.rxfifo_full = 1;
            uart->dev->int_ena.rxfifo_tout = 1;
#ifdef DEBUG_RX
            digitalWrite(25,LOW);
#endif
        }
    
    }
    UART_MUTEX_UNLOCK();

    return new_size;
}

int uartAvailable(uart_t* uart)
{
    if(uart == NULL ){
        return -1; // invalid uart control block
    }
    UART_MUTEX_LOCK();
    uint32_t count = 0;
    if(uart->queue){
        _uart_rx_read_fifo(uart);
        count = uxQueueMessagesWaiting(uart->queue);
    }
    // include direct from FiFo
    count += (uart->dev->mem_rx_status.wr_addr - uart->dev->mem_rx_status.rd_addr)% 0x80;
    UART_MUTEX_UNLOCK();
    return count;
}

int uartAvailableForWrite(uart_t* uart)
{
    if(uart == NULL) {
        return -1; // invalid uart control block
    }
    return 0x7f - uart->dev->status.txfifo_cnt;
}

int uartRead(uart_t* uart)
{
    if(uart == NULL ) {
        return -1;
    }
    uint8_t c;
    uart->dev->int_ena.rxfifo_full = 1;
    uart->dev->int_ena.rxfifo_tout = 1;
#ifdef DEBUG_RX
    digitalWrite(25,LOW);
#endif
    if(uart->queue != NULL){
        if(xQueueReceive(uart->queue, &c, 0)) {
            return c;
        } else {
            _uart_rx_read_fifo(uart);
            if(xQueueReceive(uart->queue, &c, 0)) {
                return c;
            }
        }
    } else { // direct from fifo
        if( ((uart->dev->mem_rx_status.wr_addr - uart->dev->mem_rx_status.rd_addr)% 0x80)>0){
            c = uart->dev->fifo.rw_byte;
            return c;
        }
    }
    return -1;
}

int uartPeek(uart_t* uart) // peek cannot work without Queue
{
    if(uart == NULL || uart->queue == NULL) {
        return -1;
    }
    uint8_t c;
    if(xQueuePeek(uart->queue, &c, 0)) {
        return c;
    } else {
        _uart_rx_read_fifo(uart);
        if(xQueuePeek(uart->queue, &c, 0)) {
            return c;
        }
    }
    return -1;
}

void uartWrite(uart_t* uart, uint8_t c)
{
    if(uart == NULL) {
        return;
    }
    
    UART_MUTEX_LOCK();
    while(uart->dev->status.txfifo_cnt == 0x7F){
      digitalWrite(25,HIGH);
    }
    digitalWrite(25,LOW);
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
        if (len) digitalWrite(25,HIGH);
    }
    digitalWrite(25,LOW);
    UART_MUTEX_UNLOCK();
}

void uartFlush(uart_t* uart)
{
    if(uart == NULL) {
        return;
    }

    UART_MUTEX_LOCK();
    while(uart->dev->status.txfifo_cnt || uart->dev->status.st_utx_out);

    //Due to hardware issue, we can not use fifo_rst to reset uart fifo.
    //See description about UART_TXFIFO_RST and UART_RXFIFO_RST in <<esp32_technical_reference_manual>> v2.6 or later.

    // we read the data out and make `fifo_len == 0 && rd_addr == wr_addr`.

    while(uart->dev->status.rxfifo_cnt != 0 || (uart->dev->mem_rx_status.wr_addr != uart->dev->mem_rx_status.rd_addr)) {
        READ_PERI_REG(UART_FIFO_REG(uart->num));
    }
    if(uart->queue != NULL){ // empty input queue
        uint8_t c;
        while(xQueueReceive(uart->queue, &c, 0));
    }
    UART_MUTEX_UNLOCK();
}

void uartDrain(uart_t * uart)
{
    if(uart == NULL) {
        return;
    }

    UART_MUTEX_LOCK(); 
    //wait for txFifo to empty
    while(uart->dev->status.txfifo_cnt || uart->dev->status.st_utx_out){
        digitalWrite(25,HIGH);
    }
    digitalWrite(25,LOW);
    //empty rxFifo, save rx characters so reset of UART doesn't cause problem
    _uart_rx_read_fifo(uart);

    UART_MUTEX_UNLOCK();
}
  

void uartSetBaudRate(uart_t* uart, uint32_t baud_rate)
{
    if(uart == NULL) {
        return;
    }
    UART_MUTEX_LOCK();
    int8_t oldPin = uart->rx_pin;
    uartDetachRx(uart);  // detach RX pin
    uartDrain(uart); // empty tx,rxFifo
    // attach RX pin
    uint32_t clk_div = ((getApbFrequency()<<4)/baud_rate);
    uart->dev->clk_div.div_int = clk_div>>4 ;
    uart->dev->clk_div.div_frag = clk_div & 0xf;
    uartAttachRx(uart,oldPin,uart->invertedLogic);
    UART_MUTEX_UNLOCK();
}
uint32_t uartGetBaudRate(uart_t* uart)
{
    if(uart == NULL) {
        return 0;
    }
    uint32_t clk_div = (uart->dev->clk_div.div_int << 4) | (uart->dev->clk_div.div_frag & 0x0F);
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
        while (xSemaphoreTakeRecursive(_uart_bus_array[s_uart_debug_nr].lock, portMAX_DELAY) != pdPASS);
        ets_printf("%s", temp);
        xSemaphoreGiveRecursive(_uart_bus_array[s_uart_debug_nr].lock);
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
