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
#include "freertos/semphr.h"

#include "driver/uart.h"
#include "hal/uart_ll.h"
#include "soc/soc_caps.h"
#include "soc/uart_struct.h"

#include "hal/gpio_hal.h"
#include "esp_rom_gpio.h"

static int s_uart_debug_nr = 0;

struct uart_struct_t {

#if !CONFIG_DISABLE_HAL_LOCKS
    xSemaphoreHandle lock;
#endif

    uint8_t num;
    bool has_peek;
    uint8_t peek_byte;
    QueueHandle_t uart_event_queue;   // export it by some uartGetEventQueue() function
};

#if CONFIG_DISABLE_HAL_LOCKS

#define UART_MUTEX_LOCK()
#define UART_MUTEX_UNLOCK()

static uart_t _uart_bus_array[] = {
    {0, false, 0, NULL},
#if SOC_UART_NUM > 1
    {1, false, 0, NULL},
#endif
#if SOC_UART_NUM > 2
    {2, false, 0, NULL},
#endif
};

#else

#define UART_MUTEX_LOCK()    do {} while (xSemaphoreTake(uart->lock, portMAX_DELAY) != pdPASS)
#define UART_MUTEX_UNLOCK()  xSemaphoreGive(uart->lock)

static uart_t _uart_bus_array[] = {
    {NULL, 0, false, 0, NULL},
#if SOC_UART_NUM > 1
    {NULL, 1, false, 0, NULL},
#endif
#if SOC_UART_NUM > 2
    {NULL, 2, false, 0, NULL},
#endif
};

#endif

// IDF UART has no detach function. As consequence, after ending a UART, the previous pins continue
// to work as RX/TX. It can be verified by changing the UART pins and writing to the UART. Output can 
// be seen in the previous pins and new pins as well. 
// Valid pin UART_PIN_NO_CHANGE is defined to (-1)
// Negative Pin Number will keep it unmodified, thus this function can detach individual pins
void uartDetachPins(uart_t* uart, int8_t rxPin, int8_t txPin, int8_t ctsPin, int8_t rtsPin)
{
    if(uart == NULL) {
        return;
    }

    UART_MUTEX_LOCK();
    if (txPin >= 0) {
        gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[txPin], PIN_FUNC_GPIO);
        esp_rom_gpio_connect_out_signal(txPin, SIG_GPIO_OUT_IDX, false, false);
    }

    if (rxPin >= 0) {
        gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[rxPin], PIN_FUNC_GPIO);
        esp_rom_gpio_connect_in_signal(GPIO_FUNC_IN_LOW, UART_PERIPH_SIGNAL(uart->num, SOC_UART_RX_PIN_IDX), false);
    }

    if (rtsPin >= 0) {
        gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[rtsPin], PIN_FUNC_GPIO);
        esp_rom_gpio_connect_out_signal(rtsPin, SIG_GPIO_OUT_IDX, false, false);
    }

    if (ctsPin >= 0) {
        gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[ctsPin], PIN_FUNC_GPIO);
        esp_rom_gpio_connect_in_signal(GPIO_FUNC_IN_LOW, UART_PERIPH_SIGNAL(uart->num, SOC_UART_CTS_PIN_IDX), false);
    }
    UART_MUTEX_UNLOCK();  
}

// solves issue https://github.com/espressif/arduino-esp32/issues/6032
// baudrate must be multiplied when CPU Frequency is lower than APB 80MHz
uint32_t _get_effective_baudrate(uint32_t baudrate) 
{
    uint32_t Freq = getApbFrequency()/1000000;
    if (Freq < 80) {
        return 80 / Freq * baudrate;
     }
    else {
        return baudrate;
    }
}

// Routines that take care of UART events will be in the HardwareSerial Class code
void uartGetEventQueue(uart_t* uart, QueueHandle_t *q)
{
    // passing back NULL for the Queue pointer when UART is not initialized yet
    *q = NULL;
    if(uart == NULL) {
        return;
    }
    *q = uart->uart_event_queue;
    return;
}

bool uartIsDriverInstalled(uart_t* uart) 
{
    if(uart == NULL) {
        return false;
    }

    if (uart_is_driver_installed(uart->num)) {
        return true;
    }
    return false;
}

// Valid pin UART_PIN_NO_CHANGE is defined to (-1)
// Negative Pin Number will keep it unmodified, thus this function can set individual pins
bool uartSetPins(uart_t* uart, int8_t rxPin, int8_t txPin, int8_t ctsPin, int8_t rtsPin)
{
    if(uart == NULL) {
        return false;
    }
    UART_MUTEX_LOCK();
    // IDF uart_set_pin() will issue necessary Error Message and take care of all GPIO Number validation.
    bool retCode = uart_set_pin(uart->num, txPin, rxPin, rtsPin, ctsPin) == ESP_OK; 
    UART_MUTEX_UNLOCK();  
    return retCode;
}

// 
void uartSetHwFlowCtrlMode(uart_t *uart, uint8_t mode, uint8_t threshold) {
    if(uart == NULL) {
        return;
    }
    // IDF will issue corresponding error message when mode or threshold are wrong and prevent crashing
    // IDF will check (mode > HW_FLOWCTRL_CTS_RTS || threshold >= SOC_UART_FIFO_LEN)
    uart_set_hw_flow_ctrl(uart->num, (uart_hw_flowcontrol_t) mode, threshold);
}


uart_t* uartBegin(uint8_t uart_nr, uint32_t baudrate, uint32_t config, int8_t rxPin, int8_t txPin, uint16_t rx_buffer_size, uint16_t tx_buffer_size, bool inverted, uint8_t rxfifo_full_thrhd)
{
    if(uart_nr >= SOC_UART_NUM) {
        return NULL;
    }

    uart_t* uart = &_uart_bus_array[uart_nr];

    if (uart_is_driver_installed(uart_nr)) {
        uartEnd(uart);
    }

#if !CONFIG_DISABLE_HAL_LOCKS
    if(uart->lock == NULL) {
        uart->lock = xSemaphoreCreateMutex();
        if(uart->lock == NULL) {
            return NULL;
        }
    }
#endif

    UART_MUTEX_LOCK();

    uart_config_t uart_config;
    uart_config.baud_rate = _get_effective_baudrate(baudrate);
    uart_config.data_bits = (config & 0xc) >> 2;
    uart_config.parity = (config & 0x3);
    uart_config.stop_bits = (config & 0x30) >> 4;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.rx_flow_ctrl_thresh = rxfifo_full_thrhd;
    uart_config.source_clk = UART_SCLK_APB;

    ESP_ERROR_CHECK(uart_driver_install(uart_nr, rx_buffer_size, tx_buffer_size, 20, &(uart->uart_event_queue), 0));
    ESP_ERROR_CHECK(uart_param_config(uart_nr, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uart_nr, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Is it right or the idea is to swap rx and tx pins? 
    if (inverted) {
        // invert signal for both Rx and Tx
        ESP_ERROR_CHECK(uart_set_line_inverse(uart_nr, UART_SIGNAL_TXD_INV | UART_SIGNAL_RXD_INV));    
    }
    
    UART_MUTEX_UNLOCK();

    uartFlush(uart);
    return uart;
}

// This code is under testing - for now just keep it here
void uartSetFastReading(uart_t* uart)
{
    if(uart == NULL) {
        return;
    }

    UART_MUTEX_LOCK();
    // override default RX IDF Driver Interrupt - no BREAK, PARITY or OVERFLOW
    uart_intr_config_t uart_intr = {
        .intr_enable_mask = UART_INTR_RXFIFO_FULL | UART_INTR_RXFIFO_TOUT,   // only these IRQs - no BREAK, PARITY or OVERFLOW
        .rx_timeout_thresh = 1,
        .txfifo_empty_intr_thresh = 10,
        .rxfifo_full_thresh = 2,
    };

    ESP_ERROR_CHECK(uart_intr_config(uart->num, &uart_intr));
    UART_MUTEX_UNLOCK();
}


void uartSetRxTimeout(uart_t* uart, uint8_t numSymbTimeout)
{
    if(uart == NULL) {
        return;
    }

    UART_MUTEX_LOCK();
    uart_set_rx_timeout(uart->num, numSymbTimeout);
    UART_MUTEX_UNLOCK();
}

void uartSetRxFIFOFull(uart_t* uart, uint8_t numBytesFIFOFull)
{
    if(uart == NULL) {
        return;
    }

    UART_MUTEX_LOCK();
    uart_set_rx_full_threshold(uart->num, numBytesFIFOFull);
    UART_MUTEX_UNLOCK();
}

void uartEnd(uart_t* uart)
{
    if(uart == NULL) {
        return;
    }
   
    UART_MUTEX_LOCK();
    uart_driver_delete(uart->num);
    UART_MUTEX_UNLOCK();
}


void uartSetRxInvert(uart_t* uart, bool invert)
{
    if (uart == NULL)
        return;
#if 0
    // POTENTIAL ISSUE :: original code only set/reset rxd_inv bit 
    // IDF or LL set/reset the whole inv_mask!
    if (invert)
        ESP_ERROR_CHECK(uart_set_line_inverse(uart->num, UART_SIGNAL_RXD_INV));
    else
        ESP_ERROR_CHECK(uart_set_line_inverse(uart->num, UART_SIGNAL_INV_DISABLE));
    
#else
    // this implementation is better over IDF API because it only affects RXD
    // this is supported in ESP32, ESP32-S2 and ESP32-C3
    uart_dev_t *hw = UART_LL_GET_HW(uart->num);
    if (invert)
        hw->conf0.rxd_inv = 1;
    else
        hw->conf0.rxd_inv = 0;
#endif 
}


uint32_t uartAvailable(uart_t* uart)
{

    if(uart == NULL) {
        return 0;
    }

    UART_MUTEX_LOCK();
    size_t available;
    uart_get_buffered_data_len(uart->num, &available);
    if (uart->has_peek) available++;
    UART_MUTEX_UNLOCK();
    return available;
}


uint32_t uartAvailableForWrite(uart_t* uart)
{
    if(uart == NULL) {
        return 0;
    }
    UART_MUTEX_LOCK();
    uint32_t available =  uart_ll_get_txfifo_len(UART_LL_GET_HW(uart->num));  
    size_t txRingBufferAvailable = 0;
    if (ESP_OK == uart_get_tx_buffer_free_size(uart->num, &txRingBufferAvailable)) {
        available += txRingBufferAvailable; 
    }
    UART_MUTEX_UNLOCK();
    return available;
}


uint8_t uartRead(uart_t* uart)
{
    if(uart == NULL) {
        return 0;
    }
    uint8_t c = 0;

    UART_MUTEX_LOCK();

    if (uart->has_peek) {
      uart->has_peek = false;
      c = uart->peek_byte;
    } else {

        int len = uart_read_bytes(uart->num, &c, 1, 20 / portTICK_RATE_MS);
        if (len == 0) {
            c  = 0;
        }
    }
    UART_MUTEX_UNLOCK();
    return c;
}

uint8_t uartPeek(uart_t* uart)
{
    if(uart == NULL) {
        return 0;
    }
    uint8_t c = 0;

    UART_MUTEX_LOCK();

    if (uart->has_peek) {
      c = uart->peek_byte;
    } else {
        int len = uart_read_bytes(uart->num, &c, 1, 20 / portTICK_RATE_MS);
        if (len == 0) {
            c  = 0;
        } else {
            uart->has_peek = true;
            uart->peek_byte = c;
        }
    }
    UART_MUTEX_UNLOCK();
    return c;
}

void uartWrite(uart_t* uart, uint8_t c)
{
    if(uart == NULL) {
        return;
    }
    UART_MUTEX_LOCK();
    uart_write_bytes(uart->num, &c, 1);
    UART_MUTEX_UNLOCK();
}

void uartWriteBuf(uart_t* uart, const uint8_t * data, size_t len)
{
    if(uart == NULL || data == NULL || !len) {
        return;
    }

    UART_MUTEX_LOCK();
    uart_write_bytes(uart->num, data, len);
    UART_MUTEX_UNLOCK();
}

void uartFlush(uart_t* uart)
{
    uartFlushTxOnly(uart, true);
}

void uartFlushTxOnly(uart_t* uart, bool txOnly)
{
    if(uart == NULL) {
        return;
    }
    
    UART_MUTEX_LOCK();
    while(!uart_ll_is_tx_idle(UART_LL_GET_HW(uart->num)));

    if ( !txOnly ) {
        ESP_ERROR_CHECK(uart_flush_input(uart->num));
    }
    UART_MUTEX_UNLOCK();
}

void uartSetBaudRate(uart_t* uart, uint32_t baud_rate)
{
    if(uart == NULL) {
        return;
    }
    UART_MUTEX_LOCK();
    uart_ll_set_baudrate(UART_LL_GET_HW(uart->num), _get_effective_baudrate(baud_rate));
    UART_MUTEX_UNLOCK();
}

uint32_t uartGetBaudRate(uart_t* uart)
{
    if(uart == NULL) {
        return 0;
    }

    UART_MUTEX_LOCK();
    uint32_t baud_rate = uart_ll_get_baudrate(UART_LL_GET_HW(uart->num));
    UART_MUTEX_UNLOCK();
    return baud_rate;
}

static void ARDUINO_ISR_ATTR uart0_write_char(char c)
{
    while (uart_ll_get_txfifo_len(&UART0) == 0);
    uart_ll_write_txfifo(&UART0, (const uint8_t *) &c, 1);
}

#if SOC_UART_NUM > 1
static void ARDUINO_ISR_ATTR uart1_write_char(char c)
{
    while (uart_ll_get_txfifo_len(&UART1) == 0);
    uart_ll_write_txfifo(&UART1, (const uint8_t *) &c, 1);
}
#endif

#if SOC_UART_NUM > 2
static void ARDUINO_ISR_ATTR uart2_write_char(char c)
{
    while (uart_ll_get_txfifo_len(&UART2) == 0);
    uart_ll_write_txfifo(&UART2, (const uint8_t *) &c, 1);
}
#endif

void uart_install_putc()
{
    switch(s_uart_debug_nr) {
    case 0:
        ets_install_putc1((void (*)(char)) &uart0_write_char);
        break;
#if SOC_UART_NUM > 1
    case 1:
        ets_install_putc1((void (*)(char)) &uart1_write_char);
        break;
#endif
#if SOC_UART_NUM > 2
    case 2:
        ets_install_putc1((void (*)(char)) &uart2_write_char);
        break;
#endif
    default:
        ets_install_putc1(NULL);
        break;
    }
}

void uartSetDebug(uart_t* uart)
{
    if(uart == NULL || uart->num >= SOC_UART_NUM) {
        s_uart_debug_nr = -1;
    } else {
        s_uart_debug_nr = uart->num;
    }
    uart_install_putc();
}

int uartGetDebug()
{
    return s_uart_debug_nr;
}

int log_printf(const char *format, ...)
{
    static char loc_buf[64];
    char * temp = loc_buf;
    int len;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    len = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if(len >= sizeof(loc_buf)){
        temp = (char*)malloc(len+1);
        if(temp == NULL) {
            va_end(arg);
            return 0;
        }
    }
#if !CONFIG_DISABLE_HAL_LOCKS
    if(s_uart_debug_nr != -1 && _uart_bus_array[s_uart_debug_nr].lock){
        xSemaphoreTake(_uart_bus_array[s_uart_debug_nr].lock, portMAX_DELAY);
    }
#endif
    
    vsnprintf(temp, len+1, format, arg);
    ets_printf("%s", temp);

#if !CONFIG_DISABLE_HAL_LOCKS
    if(s_uart_debug_nr != -1 && _uart_bus_array[s_uart_debug_nr].lock){
        xSemaphoreGive(_uart_bus_array[s_uart_debug_nr].lock);
    }
#endif
    va_end(arg);
    if(len >= sizeof(loc_buf)){
        free(temp);
    }
    return len;
}


static void log_print_buf_line(const uint8_t *b, size_t len, size_t total_len){
    for(size_t i = 0; i<len; i++){
        log_printf("%s0x%02x,",i?" ":"", b[i]);
    }
    if(total_len > 16){
        for(size_t i = len; i<16; i++){
            log_printf("      ");
        }
        log_printf("    // ");
    } else {
        log_printf(" // ");
    }
    for(size_t i = 0; i<len; i++){
        log_printf("%c",((b[i] >= 0x20) && (b[i] < 0x80))?b[i]:'.');
    }
    log_printf("\n");
}

void log_print_buf(const uint8_t *b, size_t len){
    if(!len || !b){
        return;
    }
    for(size_t i = 0; i<len; i+=16){
        if(len > 16){
            log_printf("/* 0x%04X */ ", i);
        }
        log_print_buf_line(b+i, ((len-i)<16)?(len - i):16, len);
    }
}

/*
 * if enough pulses are detected return the minimum high pulse duration + minimum low pulse duration divided by two. 
 * This equals one bit period. If flag is true the function return inmediately, otherwise it waits for enough pulses.
 */
unsigned long uartBaudrateDetect(uart_t *uart, bool flg)
{
#ifndef CONFIG_IDF_TARGET_ESP32S3
    if(uart == NULL) {
        return 0;
    }

    uart_dev_t *hw = UART_LL_GET_HW(uart->num);

    while(hw->rxd_cnt.edge_cnt < 30) { // UART_PULSE_NUM(uart_num)
        if(flg) return 0;
        ets_delay_us(1000);
    }

    UART_MUTEX_LOCK();
    //log_i("lowpulse_min_cnt = %d hightpulse_min_cnt = %d", hw->lowpulse.min_cnt, hw->highpulse.min_cnt);
    unsigned long ret = ((hw->lowpulse.min_cnt + hw->highpulse.min_cnt) >> 1);
    UART_MUTEX_UNLOCK();

    return ret;
#else
    return 0;
#endif
}


/*
 * To start detection of baud rate with the uart the auto_baud.en bit needs to be cleared and set. The bit period is 
 * detected calling uartBadrateDetect(). The raw baudrate is computed using the UART_CLK_FREQ. The raw baudrate is 
 * rounded to the closed real baudrate.
 * 
 * ESP32-C3 reports wrong baud rate detection as shown below:
 * 
 * This will help in a future recall for the C3.
 * Baud Sent:          Baud Read:
 *  300        -->       19536
 * 2400        -->       19536
 * 4800        -->       19536 
 * 9600        -->       28818 
 * 19200       -->       57678
 * 38400       -->       115440
 * 57600       -->       173535
 * 115200      -->       347826
 * 230400      -->       701754
 * 
 * 
*/
void uartStartDetectBaudrate(uart_t *uart) {
    if(uart == NULL) {
        return;
    }

#ifdef CONFIG_IDF_TARGET_ESP32C3
    
    // ESP32-C3 requires further testing
    // Baud rate detection returns wrong values 
   
    log_e("ESP32-C3 baud rate detection is not supported.");
    return;

    // Code bellow for C3 kept for future recall
    //hw->rx_filt.glitch_filt = 0x08;
    //hw->rx_filt.glitch_filt_en = 1;
    //hw->conf0.autobaud_en = 0;
    //hw->conf0.autobaud_en = 1;
#elif CONFIG_IDF_TARGET_ESP32S3
#else
    uart_dev_t *hw = UART_LL_GET_HW(uart->num);
    hw->auto_baud.glitch_filt = 0x08;
    hw->auto_baud.en = 0;
    hw->auto_baud.en = 1;
#endif
}
 
unsigned long
uartDetectBaudrate(uart_t *uart)
{
    if(uart == NULL) {
        return 0;
    }

#ifndef CONFIG_IDF_TARGET_ESP32C3    // ESP32-C3 requires further testing - Baud rate detection returns wrong values 

    static bool uartStateDetectingBaudrate = false;

    if(!uartStateDetectingBaudrate) {
        uartStartDetectBaudrate(uart);
        uartStateDetectingBaudrate = true;
    }

    unsigned long divisor = uartBaudrateDetect(uart, true);
    if (!divisor) {
        return 0;
    }
    //  log_i(...) below has been used to check C3 baud rate detection results
    //log_i("Divisor = %d\n", divisor);
    //log_i("BAUD RATE based on Positive Pulse %d\n", getApbFrequency()/((hw->pospulse.min_cnt + 1)/2));
    //log_i("BAUD RATE based on Negative Pulse %d\n", getApbFrequency()/((hw->negpulse.min_cnt + 1)/2));


#ifdef CONFIG_IDF_TARGET_ESP32C3
    //hw->conf0.autobaud_en = 0;
#elif CONFIG_IDF_TARGET_ESP32S3
#else
    uart_dev_t *hw = UART_LL_GET_HW(uart->num);
    hw->auto_baud.en = 0;
#endif
    uartStateDetectingBaudrate = false; // Initialize for the next round

    unsigned long baudrate = getApbFrequency() / divisor;
    //log_i("APB_FREQ = %d\nraw baudrate detected = %d", getApbFrequency(), baudrate);

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
#else
    log_e("ESP32-C3 baud rate detection is not supported.");
    return 0;
#endif
}
