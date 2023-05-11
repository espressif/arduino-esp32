#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "pins_arduino.h"
#include "HardwareSerial.h"
#include "soc/soc_caps.h"
#include "driver/uart.h"
#include "freertos/queue.h"

#ifndef ARDUINO_SERIAL_EVENT_TASK_STACK_SIZE
#define ARDUINO_SERIAL_EVENT_TASK_STACK_SIZE 2048
#endif

#ifndef ARDUINO_SERIAL_EVENT_TASK_PRIORITY
#define ARDUINO_SERIAL_EVENT_TASK_PRIORITY (configMAX_PRIORITIES-1)
#endif

#ifndef ARDUINO_SERIAL_EVENT_TASK_RUNNING_CORE
#define ARDUINO_SERIAL_EVENT_TASK_RUNNING_CORE -1
#endif

#ifndef SOC_RX0
#if CONFIG_IDF_TARGET_ESP32
#define SOC_RX0 3
#elif CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
#define SOC_RX0 44
#elif CONFIG_IDF_TARGET_ESP32C3
#define SOC_RX0 20
#endif
#endif

#ifndef SOC_TX0
#if CONFIG_IDF_TARGET_ESP32
#define SOC_TX0 1
#elif CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
#define SOC_TX0 43
#elif CONFIG_IDF_TARGET_ESP32C3
#define SOC_TX0 21
#endif
#endif

void serialEvent(void) __attribute__((weak));
void serialEvent(void) {}

#if SOC_UART_NUM > 1

#ifndef RX1
#if CONFIG_IDF_TARGET_ESP32
#define RX1 9
#elif CONFIG_IDF_TARGET_ESP32S2
#define RX1 18
#elif CONFIG_IDF_TARGET_ESP32C3
#define RX1 18
#elif CONFIG_IDF_TARGET_ESP32S3
#define RX1 15
#endif
#endif

#ifndef TX1
#if CONFIG_IDF_TARGET_ESP32
#define TX1 10
#elif CONFIG_IDF_TARGET_ESP32S2
#define TX1 17
#elif CONFIG_IDF_TARGET_ESP32C3
#define TX1 19
#elif CONFIG_IDF_TARGET_ESP32S3
#define TX1 16
#endif
#endif

void serialEvent1(void) __attribute__((weak));
void serialEvent1(void) {}
#endif /* SOC_UART_NUM > 1 */

#if SOC_UART_NUM > 2
#ifndef RX2
#if CONFIG_IDF_TARGET_ESP32
#define RX2 16
#elif CONFIG_IDF_TARGET_ESP32S3
#define RX2 19 
#endif
#endif

#ifndef TX2
#if CONFIG_IDF_TARGET_ESP32
#define TX2 17
#elif CONFIG_IDF_TARGET_ESP32S3
#define TX2 20
#endif
#endif

void serialEvent2(void) __attribute__((weak));
void serialEvent2(void) {}
#endif /* SOC_UART_NUM > 2 */

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SERIAL)
#if ARDUINO_USB_CDC_ON_BOOT //Serial used for USB CDC
HardwareSerial Serial0(0);
#else
HardwareSerial Serial(0);
#endif
#if SOC_UART_NUM > 1
HardwareSerial Serial1(1);
#endif
#if SOC_UART_NUM > 2
HardwareSerial Serial2(2);
#endif

void serialEventRun(void)
{
#if ARDUINO_USB_CDC_ON_BOOT //Serial used for USB CDC
    if(Serial0.available()) serialEvent();
#else
    if(Serial.available()) serialEvent();
#endif
#if SOC_UART_NUM > 1
    if(Serial1.available()) serialEvent1();
#endif
#if SOC_UART_NUM > 2
    if(Serial2.available()) serialEvent2();
#endif
}
#endif

#if !CONFIG_DISABLE_HAL_LOCKS
#define HSERIAL_MUTEX_LOCK()    do {} while (xSemaphoreTake(_lock, portMAX_DELAY) != pdPASS)
#define HSERIAL_MUTEX_UNLOCK()  xSemaphoreGive(_lock)
#else
#define HSERIAL_MUTEX_LOCK()    
#define HSERIAL_MUTEX_UNLOCK()  
#endif

HardwareSerial::HardwareSerial(int uart_nr) : 
_uart_nr(uart_nr), 
_uart(NULL),
_rxBufferSize(256),
_txBufferSize(0), 
_onReceiveCB(NULL), 
_onReceiveErrorCB(NULL),
_onReceiveTimeout(false),
_rxTimeout(2),
_rxFIFOFull(0),
_eventTask(NULL)
#if !CONFIG_DISABLE_HAL_LOCKS
    ,_lock(NULL)
#endif
,_rxPin(-1) 
,_txPin(-1)
,_ctsPin(-1)
,_rtsPin(-1)
{
#if !CONFIG_DISABLE_HAL_LOCKS
    if(_lock == NULL){
        _lock = xSemaphoreCreateMutex();
        if(_lock == NULL){
            log_e("xSemaphoreCreateMutex failed");
            return;
        }
    }
#endif
}

HardwareSerial::~HardwareSerial()
{
    end();
#if !CONFIG_DISABLE_HAL_LOCKS
    if(_lock != NULL){
        vSemaphoreDelete(_lock);
    }
#endif
}


void HardwareSerial::_createEventTask(void *args)
{
    // Creating UART event Task
    xTaskCreateUniversal(_uartEventTask, "uart_event_task", ARDUINO_SERIAL_EVENT_TASK_STACK_SIZE, this, ARDUINO_SERIAL_EVENT_TASK_PRIORITY, &_eventTask, ARDUINO_SERIAL_EVENT_TASK_RUNNING_CORE);
    if (_eventTask == NULL) {
        log_e(" -- UART%d Event Task not Created!", _uart_nr);
    }
}

void HardwareSerial::_destroyEventTask(void)
{
    if (_eventTask != NULL) {
        vTaskDelete(_eventTask);
        _eventTask = NULL;
    }
}

void HardwareSerial::onReceiveError(OnReceiveErrorCb function) 
{
    HSERIAL_MUTEX_LOCK();
    // function may be NULL to cancel onReceive() from its respective task 
    _onReceiveErrorCB = function;
    // this can be called after Serial.begin(), therefore it shall create the event task
    if (function != NULL && _uart != NULL && _eventTask == NULL) {
        _createEventTask(this);
    }
    HSERIAL_MUTEX_UNLOCK();
}

void HardwareSerial::onReceive(OnReceiveCb function, bool onlyOnTimeout)
{
    HSERIAL_MUTEX_LOCK();
    // function may be NULL to cancel onReceive() from its respective task 
    _onReceiveCB = function;

    // setting the callback to NULL will just disable it
    if (_onReceiveCB != NULL) {
        // When Rx timeout is Zero (disabled), there is only one possible option that is callback when FIFO reaches 120 bytes
        _onReceiveTimeout = _rxTimeout > 0 ? onlyOnTimeout : false;

        // in case that onReceive() shall work only with RX Timeout, FIFO shall be high
        // this is a work around for an IDF issue with events and low FIFO Full value (< 3)
        if (_onReceiveTimeout) {
            uartSetRxFIFOFull(_uart, 120);
            log_w("OnReceive is set to Timeout only, thus FIFO Full is now 120 bytes.");
        }

        // this method can be called after Serial.begin(), therefore it shall create the event task
        if (_uart != NULL && _eventTask == NULL) {
            _createEventTask(this); // Create event task
        }
    }
    HSERIAL_MUTEX_UNLOCK();
}

// This function allow the user to define how many bytes will trigger an Interrupt that will copy RX FIFO to the internal RX Ringbuffer
// ISR will also move data from FIFO to RX Ringbuffer after a RX Timeout defined in HardwareSerial::setRxTimeout(uint8_t symbols_timeout)
// A low value of FIFO Full bytes will consume more CPU time within the ISR
// A high value of FIFO Full bytes will make the application wait longer to have byte available for the Stkech in a streaming scenario
// Both RX FIFO Full and RX Timeout may affect when onReceive() will be called
void HardwareSerial::setRxFIFOFull(uint8_t fifoBytes)
{
    HSERIAL_MUTEX_LOCK();
    // in case that onReceive() shall work only with RX Timeout, FIFO shall be high
    // this is a work around for an IDF issue with events and low FIFO Full value (< 3)
    if (_onReceiveCB != NULL && _onReceiveTimeout) {
        fifoBytes = 120;
        log_w("OnReceive is set to Timeout only, thus FIFO Full is now 120 bytes.");
    }
    uartSetRxFIFOFull(_uart, fifoBytes); // Set new timeout
    if (fifoBytes > 0 && fifoBytes < SOC_UART_FIFO_LEN - 1) _rxFIFOFull = fifoBytes;
    HSERIAL_MUTEX_UNLOCK();
}

// timout is calculates in time to receive UART symbols at the UART baudrate.
// the estimation is about 11 bits per symbol (SERIAL_8N1)
void HardwareSerial::setRxTimeout(uint8_t symbols_timeout)
{
    HSERIAL_MUTEX_LOCK();
    
    // Zero disables timeout, thus, onReceive callback will only be called when RX FIFO reaches 120 bytes
    // Any non-zero value will activate onReceive callback based on UART baudrate with about 11 bits per symbol 
    _rxTimeout = symbols_timeout;   
    if (!symbols_timeout) _onReceiveTimeout = false;  // only when RX timeout is disabled, we also must disable this flag 

    uartSetRxTimeout(_uart, _rxTimeout); // Set new timeout
    
    HSERIAL_MUTEX_UNLOCK();
}

void HardwareSerial::eventQueueReset()
{
    QueueHandle_t uartEventQueue = NULL;
    if (_uart == NULL) {
	    return;
    }
    uartGetEventQueue(_uart, &uartEventQueue);
    if (uartEventQueue != NULL) {
        xQueueReset(uartEventQueue);
    }
}

void HardwareSerial::_uartEventTask(void *args)
{
    HardwareSerial *uart = (HardwareSerial *)args;
    uart_event_t event;
    QueueHandle_t uartEventQueue = NULL;
    uartGetEventQueue(uart->_uart, &uartEventQueue);
    if (uartEventQueue != NULL) {
        for(;;) {
            //Waiting for UART event.
            if(xQueueReceive(uartEventQueue, (void * )&event, (portTickType)portMAX_DELAY)) {
                hardwareSerial_error_t currentErr = UART_NO_ERROR;
                switch(event.type) {
                    case UART_DATA:
                        if(uart->_onReceiveCB && uart->available() > 0 && 
                            ((uart->_onReceiveTimeout && event.timeout_flag) || !uart->_onReceiveTimeout) ) 
                                uart->_onReceiveCB();
                        break;
                    case UART_FIFO_OVF:
                        log_w("UART%d FIFO Overflow. Consider adding Hardware Flow Control to your Application.", uart->_uart_nr);
                        currentErr = UART_FIFO_OVF_ERROR;
                        break;
                    case UART_BUFFER_FULL:
                        log_w("UART%d Buffer Full. Consider increasing your buffer size of your Application.", uart->_uart_nr);
                        currentErr = UART_BUFFER_FULL_ERROR;
                        break;
                    case UART_BREAK:
                        log_w("UART%d RX break.", uart->_uart_nr);
                        currentErr = UART_BREAK_ERROR;
                        break;
                    case UART_PARITY_ERR:
                        log_w("UART%d parity error.", uart->_uart_nr);
                        currentErr = UART_PARITY_ERROR;
                        break;
                    case UART_FRAME_ERR:
                        log_w("UART%d frame error.", uart->_uart_nr);
                        currentErr = UART_FRAME_ERROR;
                        break;
                    default:
                        log_w("UART%d unknown event type %d.", uart->_uart_nr, event.type);
                        break;
                }
                if (currentErr != UART_NO_ERROR) {
                    if(uart->_onReceiveErrorCB) uart->_onReceiveErrorCB(currentErr);
                }
            }
        }
    }
    vTaskDelete(NULL);
}

void HardwareSerial::begin(unsigned long baud, uint32_t config, int8_t rxPin, int8_t txPin, bool invert, unsigned long timeout_ms, uint8_t rxfifo_full_thrhd)
{
    if(0 > _uart_nr || _uart_nr >= SOC_UART_NUM) {
        log_e("Serial number is invalid, please use numers from 0 to %u", SOC_UART_NUM - 1);
        return;
    }

#if !CONFIG_DISABLE_HAL_LOCKS
    if(_lock == NULL){
        log_e("MUTEX Lock failed. Can't begin.");
        return;
    }
#endif

    HSERIAL_MUTEX_LOCK();
    // First Time or after end() --> set default Pins
    if (!uartIsDriverInstalled(_uart)) {
        switch (_uart_nr) {
            case UART_NUM_0:
                if (rxPin < 0 && txPin < 0) {
                    rxPin = SOC_RX0;
                    txPin = SOC_TX0;
                }
            break;
#if SOC_UART_NUM > 1                   // may save some flash bytes...
            case UART_NUM_1:
               if (rxPin < 0 && txPin < 0) {
                    rxPin = RX1;
                    txPin = TX1;
                }
            break;
#endif
#if SOC_UART_NUM > 2                   // may save some flash bytes...
            case UART_NUM_2:
               if (rxPin < 0 && txPin < 0) {
                    rxPin = RX2;
                    txPin = TX2;
                }
            break;
#endif
        }
    }

    if(_uart) {
        // in this case it is a begin() over a previous begin() - maybe to change baud rate
        // thus do not disable debug output
        end(false);
    }

    // IDF UART driver keeps Pin setting on restarting. Negative Pin number will keep it unmodified.
    _uart = uartBegin(_uart_nr, baud ? baud : 9600, config, rxPin, txPin, _rxBufferSize, _txBufferSize, invert, rxfifo_full_thrhd);
    if (!baud) {
        // using baud rate as zero, forces it to try to detect the current baud rate in place
        uartStartDetectBaudrate(_uart);
        time_t startMillis = millis();
        unsigned long detectedBaudRate = 0;
        while(millis() - startMillis < timeout_ms && !(detectedBaudRate = uartDetectBaudrate(_uart))) {
            yield();
        }

        end(false);

        if(detectedBaudRate) {
            delay(100); // Give some time...
            _uart = uartBegin(_uart_nr, detectedBaudRate, config, rxPin, txPin, _rxBufferSize, _txBufferSize, invert, rxfifo_full_thrhd);
        } else {
            log_e("Could not detect baudrate. Serial data at the port must be present within the timeout for detection to be possible");
            _uart = NULL;
        }
    }
    // create a task to deal with Serial Events when, for example, calling begin() twice to change the baudrate,
    // or when setting the callback before calling begin() 
    if (_uart != NULL && (_onReceiveCB != NULL || _onReceiveErrorCB != NULL) && _eventTask == NULL) {
        _createEventTask(this);
    }

    // Set UART RX timeout
    uartSetRxTimeout(_uart, _rxTimeout);

    // Set UART FIFO Full depending on the baud rate. 
    // Lower baud rates will force to emulate byte-by-byte reading
    // Higher baud rates will keep IDF default of 120 bytes for FIFO FULL Interrupt
    // It can also be changed by the application at any time 
    if (!_rxFIFOFull) {    // it has not being changed before calling begin()
      //  set a default FIFO Full value for the IDF driver
      uint8_t fifoFull = 1;
      if (baud > 57600 || (_onReceiveCB != NULL && _onReceiveTimeout)) {
        fifoFull = 120;
      }
      uartSetRxFIFOFull(_uart, fifoFull);
      _rxFIFOFull = fifoFull;
    }

    _rxPin = rxPin;
    _txPin = txPin;

    HSERIAL_MUTEX_UNLOCK();
}

void HardwareSerial::updateBaudRate(unsigned long baud)
{
	uartSetBaudRate(_uart, baud);
}

void HardwareSerial::end(bool fullyTerminate)
{
    // default Serial.end() will completely disable HardwareSerial, 
    // including any tasks or debug message channel (log_x()) - but not for IDF log messages!
    if(fullyTerminate) {
        _onReceiveCB = NULL;
        _onReceiveErrorCB = NULL;
        if (uartGetDebug() == _uart_nr) {
            uartSetDebug(0);
        }

        _rxFIFOFull = 0; 

        uartDetachPins(_uart, _rxPin, _txPin, _ctsPin, _rtsPin);
        _rxPin = _txPin = _ctsPin = _rtsPin = -1;

    }
    delay(10);
    uartEnd(_uart);
    _uart = 0;
    _destroyEventTask();
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
    uint8_t c = 0;
    if (uartReadBytes(_uart, &c, 1, 0) == 1) {
        return c;
    } else {
        return -1;
    }
}

// read characters into buffer
// terminates if size characters have been read, or no further are pending
// returns the number of characters placed in the buffer
// the buffer is NOT null terminated.
size_t HardwareSerial::read(uint8_t *buffer, size_t size)
{
    return uartReadBytes(_uart, buffer, size, 0);
}

// Overrides Stream::readBytes() to be faster using IDF
size_t HardwareSerial::readBytes(uint8_t *buffer, size_t length)
{
    return uartReadBytes(_uart, buffer, length, (uint32_t)getTimeout());
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
    return uartIsDriverInstalled(_uart);
}

void HardwareSerial::setRxInvert(bool invert)
{
    uartSetRxInvert(_uart, invert);
}

// negative Pin value will keep it unmodified
void HardwareSerial::setPins(int8_t rxPin, int8_t txPin, int8_t ctsPin, int8_t rtsPin)
{
    if(_uart == NULL) {
        log_e("setPins() shall be called after begin() - nothing done\n");
        return;
    }

    // uartSetPins() checks if pins are valid for each function and for the SoC 
    if (uartSetPins(_uart, rxPin, txPin, ctsPin, rtsPin)) {
        _txPin = _txPin >= 0 ? txPin : _txPin;
        _rxPin = _rxPin >= 0 ? rxPin : _rxPin;
        _rtsPin = _rtsPin >= 0 ? rtsPin : _rtsPin;
        _ctsPin = _ctsPin >= 0 ? ctsPin : _ctsPin;
    } else {
        log_e("Error when setting Serial port Pins. Invalid Pin.\n");
    }
}

// Enables or disables Hardware Flow Control using RTS and/or CTS pins (must use setAllPins() before)
void HardwareSerial::setHwFlowCtrlMode(uint8_t mode, uint8_t threshold)
{
    uartSetHwFlowCtrlMode(_uart, mode, threshold);
}

size_t HardwareSerial::setRxBufferSize(size_t new_size) {

    if (_uart) {
        log_e("RX Buffer can't be resized when Serial is already running.\n");
        return 0;
    }

    if (new_size <= SOC_UART_FIFO_LEN) {
        log_e("RX Buffer must be higher than %d.\n", SOC_UART_FIFO_LEN);  // ESP32, S2, S3 and C3 means higher than 128
        return 0;
    }

    _rxBufferSize = new_size;
    return _rxBufferSize;
}

size_t HardwareSerial::setTxBufferSize(size_t new_size) {

    if (_uart) {
        log_e("TX Buffer can't be resized when Serial is already running.\n");
        return 0;
    }

    if (new_size <= SOC_UART_FIFO_LEN) {
        log_e("TX Buffer must be higher than %d.\n", SOC_UART_FIFO_LEN);  // ESP32, S2, S3 and C3 means higher than 128
        return 0;
    }

    _txBufferSize = new_size;
    return _txBufferSize;
}
