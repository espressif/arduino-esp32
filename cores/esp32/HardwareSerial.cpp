#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <ctime>

#include "pins_arduino.h"
#include "io_pin_remap.h"
#include "HardwareSerial.h"
#include "soc/soc_caps.h"
#include "driver/uart.h"
#include "freertos/queue.h"

#if (SOC_UART_LP_NUM >= 1)
#define UART_HW_FIFO_LEN(uart_num) ((uart_num < SOC_UART_HP_NUM) ? SOC_UART_FIFO_LEN : SOC_LP_UART_FIFO_LEN)
#else
#define UART_HW_FIFO_LEN(uart_num) SOC_UART_FIFO_LEN
#endif

void serialEvent(void) __attribute__((weak));

#if SOC_UART_NUM > 1
void serialEvent1(void) __attribute__((weak));
#endif /* SOC_UART_NUM > 1 */

#if SOC_UART_NUM > 2
void serialEvent2(void) __attribute__((weak));
#endif /* SOC_UART_NUM > 2 */

#if SOC_UART_NUM > 3
void serialEvent3(void) __attribute__((weak));
#endif /* SOC_UART_NUM > 3 */

#if SOC_UART_NUM > 4
void serialEvent4(void) __attribute__((weak));
#endif /* SOC_UART_NUM > 4 */

#if SOC_UART_NUM > 5
void serialEvent5(void) __attribute__((weak));
#endif /* SOC_UART_NUM > 5 */

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SERIAL)
// There is always Seria0 for UART0
HardwareSerial Serial0(0);
#if SOC_UART_NUM > 1
HardwareSerial Serial1(1);
#endif
#if SOC_UART_NUM > 2
HardwareSerial Serial2(2);
#endif
#if SOC_UART_NUM > 3
HardwareSerial Serial3(3);
#endif
#if SOC_UART_NUM > 4
HardwareSerial Serial4(4);
#endif
#if (SOC_UART_NUM > 5)
HardwareSerial Serial5(5);
#endif
#if HWCDC_SERIAL_IS_DEFINED == 1  // Hardware JTAG CDC Event
extern void HWCDCSerialEvent(void) __attribute__((weak));
#endif

#if USB_SERIAL_IS_DEFINED == 1  // Native USB CDC Event
// Used by Hardware Serial for USB CDC events
extern void USBSerialEvent(void) __attribute__((weak));
#endif

void serialEventRun(void) {
#if HWCDC_SERIAL_IS_DEFINED == 1  // Hardware JTAG CDC Event
  if (HWCDCSerialEvent && HWCDCSerial.available()) {
    HWCDCSerialEvent();
  }
#endif
#if USB_SERIAL_IS_DEFINED == 1  // Native USB CDC Event
  if (USBSerialEvent && USBSerial.available()) {
    USBSerialEvent();
  }
#endif
  // UART0 is default serialEvent()
  if (serialEvent && Serial0.available()) {
    serialEvent();
  }
#if SOC_UART_NUM > 1
  if (serialEvent1 && Serial1.available()) {
    serialEvent1();
  }
#endif
#if SOC_UART_NUM > 2
  if (serialEvent2 && Serial2.available()) {
    serialEvent2();
  }
#endif
#if SOC_UART_NUM > 3
  if (serialEvent3 && Serial3.available()) {
    serialEvent3();
  }
#endif
#if SOC_UART_NUM > 4
  if (serialEvent4 && Serial4.available()) {
    serialEvent4();
  }
#endif
#if SOC_UART_NUM > 5
  if (serialEvent5 && Serial5.available()) {
    serialEvent5();
  }
#endif
}
#endif

#if !CONFIG_DISABLE_HAL_LOCKS
#define HSERIAL_MUTEX_LOCK() \
  do {                       \
  } while (xSemaphoreTake(_lock, portMAX_DELAY) != pdPASS)
#define HSERIAL_MUTEX_UNLOCK() xSemaphoreGive(_lock)
#else
#define HSERIAL_MUTEX_LOCK()
#define HSERIAL_MUTEX_UNLOCK()
#endif

HardwareSerial::HardwareSerial(uint8_t uart_nr)
  : _uart_nr(uart_nr), _uart(NULL), _rxBufferSize(256), _txBufferSize(0), _onReceiveCB(NULL), _onReceiveErrorCB(NULL), _onReceiveTimeout(false), _rxTimeout(1),
    _rxFIFOFull(0), _eventTask(NULL)
#if !CONFIG_DISABLE_HAL_LOCKS
    ,
    _lock(NULL)
#endif
{
#if !CONFIG_DISABLE_HAL_LOCKS
  if (_lock == NULL) {
    _lock = xSemaphoreCreateMutex();
    if (_lock == NULL) {
      log_e("xSemaphoreCreateMutex failed");
      return;
    }
  }
#endif
  // set deinit function in the Peripheral Manager
  uart_init_PeriMan();
}

HardwareSerial::~HardwareSerial() {
  end();  // explicit Full UART termination
#if !CONFIG_DISABLE_HAL_LOCKS
  if (_lock != NULL) {
    vSemaphoreDelete(_lock);
  }
#endif
}

void HardwareSerial::_createEventTask(void *args) {
  // Creating UART event Task
  xTaskCreateUniversal(
    _uartEventTask, "uart_event_task", ARDUINO_SERIAL_EVENT_TASK_STACK_SIZE, this, ARDUINO_SERIAL_EVENT_TASK_PRIORITY, &_eventTask,
    ARDUINO_SERIAL_EVENT_TASK_RUNNING_CORE
  );
  if (_eventTask == NULL) {
    log_e(" -- UART%d Event Task not Created!", _uart_nr);
  }
}

void HardwareSerial::_destroyEventTask(void) {
  if (_eventTask != NULL) {
    vTaskDelete(_eventTask);
    _eventTask = NULL;
  }
}

void HardwareSerial::onReceiveError(OnReceiveErrorCb function) {
  HSERIAL_MUTEX_LOCK();
  // function may be NULL to cancel onReceive() from its respective task
  _onReceiveErrorCB = function;
  // this can be called after Serial.begin(), therefore it shall create the event task
  if (function != NULL && _uart != NULL && _eventTask == NULL) {
    _createEventTask(this);
  }
  HSERIAL_MUTEX_UNLOCK();
}

void HardwareSerial::onReceive(OnReceiveCb function, bool onlyOnTimeout) {
  HSERIAL_MUTEX_LOCK();
  // function may be NULL to cancel onReceive() from its respective task
  _onReceiveCB = function;

  // setting the callback to NULL will just disable it
  if (_onReceiveCB != NULL) {
    // When Rx timeout is Zero (disabled), there is only one possible option that is callback when FIFO reaches 120 bytes
    _onReceiveTimeout = _rxTimeout > 0 ? onlyOnTimeout : false;

    // in case that onReceive() shall work only with RX Timeout, FIFO shall be high
    // this is a work around for an IDF issue with events and low FIFO Full value (< 3)
    // Not valid for the LP UART
    if (_onReceiveTimeout && _uart_nr < SOC_UART_HP_NUM) {
      uartSetRxFIFOFull(_uart, 120);
      log_w("OnReceive is set to Timeout only, thus FIFO Full is now 120 bytes.");
    }

    // this method can be called after Serial.begin(), therefore it shall create the event task
    if (_uart != NULL && _eventTask == NULL) {
      _createEventTask(this);  // Create event task
    }
  }
  HSERIAL_MUTEX_UNLOCK();
}

// This function allow the user to define how many bytes will trigger an Interrupt that will copy RX FIFO to the internal RX Ringbuffer
// ISR will also move data from FIFO to RX Ringbuffer after a RX Timeout defined in HardwareSerial::setRxTimeout(uint8_t symbols_timeout)
// A low value of FIFO Full bytes will consume more CPU time within the ISR
// A high value of FIFO Full bytes will make the application wait longer to have byte available for the Stkech in a streaming scenario
// Both RX FIFO Full and RX Timeout may affect when onReceive() will be called
bool HardwareSerial::setRxFIFOFull(uint8_t fifoBytes) {
  HSERIAL_MUTEX_LOCK();
  // in case that onReceive() shall work only with RX Timeout, FIFO shall be high
  // this is a work around for an IDF issue with events and low FIFO Full value (< 3)
  // Not valid for the LP UART
  if (_onReceiveCB != NULL && _onReceiveTimeout && _uart_nr < SOC_UART_HP_NUM) {
    fifoBytes = 120;
    log_w("OnReceive is set to Timeout only, thus FIFO Full is now 120 bytes.");
  }
  bool retCode = uartSetRxFIFOFull(_uart, fifoBytes);  // Set new timeout
  if (fifoBytes > 0 && fifoBytes < UART_HW_FIFO_LEN(_uart_nr) - 1) {
    _rxFIFOFull = fifoBytes;
  }
  HSERIAL_MUTEX_UNLOCK();
  return retCode;
}

// timeout is calculates in time to receive UART symbols at the UART baudrate.
// the estimation is about 11 bits per symbol (SERIAL_8N1)
bool HardwareSerial::setRxTimeout(uint8_t symbols_timeout) {
  HSERIAL_MUTEX_LOCK();

  // Zero disables timeout, thus, onReceive callback will only be called when RX FIFO reaches 120 bytes
  // Any non-zero value will activate onReceive callback based on UART baudrate with about 11 bits per symbol
  _rxTimeout = symbols_timeout;
  if (!symbols_timeout) {
    _onReceiveTimeout = false;  // only when RX timeout is disabled, we also must disable this flag
  }

  bool retCode = uartSetRxTimeout(_uart, _rxTimeout);  // Set new timeout

  HSERIAL_MUTEX_UNLOCK();
  return retCode;
}

void HardwareSerial::eventQueueReset() {
  QueueHandle_t uartEventQueue = NULL;
  if (_uart == NULL) {
    return;
  }
  uartGetEventQueue(_uart, &uartEventQueue);
  if (uartEventQueue != NULL) {
    xQueueReset(uartEventQueue);
  }
}

void HardwareSerial::_uartEventTask(void *args) {
  HardwareSerial *uart = (HardwareSerial *)args;
  uart_event_t event;
  QueueHandle_t uartEventQueue = NULL;
  uartGetEventQueue(uart->_uart, &uartEventQueue);
  if (uartEventQueue != NULL) {
    for (;;) {
      //Waiting for UART event.
      if (xQueueReceive(uartEventQueue, (void *)&event, (TickType_t)portMAX_DELAY)) {
        hardwareSerial_error_t currentErr = UART_NO_ERROR;
        switch (event.type) {
          case UART_DATA:
            if (uart->_onReceiveCB && uart->available() > 0 && ((uart->_onReceiveTimeout && event.timeout_flag) || !uart->_onReceiveTimeout)) {
              uart->_onReceiveCB();
            }
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
            log_v("UART%d RX break.", uart->_uart_nr);
            currentErr = UART_BREAK_ERROR;
            break;
          case UART_PARITY_ERR:
            log_v("UART%d parity error.", uart->_uart_nr);
            currentErr = UART_PARITY_ERROR;
            break;
          case UART_FRAME_ERR:
            log_v("UART%d frame error.", uart->_uart_nr);
            currentErr = UART_FRAME_ERROR;
            break;
          default: log_v("UART%d unknown event type %d.", uart->_uart_nr, event.type); break;
        }
        if (currentErr != UART_NO_ERROR) {
          if (uart->_onReceiveErrorCB) {
            uart->_onReceiveErrorCB(currentErr);
          }
        }
      }
    }
  }
  vTaskDelete(NULL);
}

void HardwareSerial::begin(unsigned long baud, uint32_t config, int8_t rxPin, int8_t txPin, bool invert, unsigned long timeout_ms, uint8_t rxfifo_full_thrhd) {
  if (_uart_nr >= SOC_UART_NUM) {
    log_e("Serial number is invalid, please use a number from 0 to %u", SOC_UART_NUM - 1);
    return;
  }

#if !CONFIG_DISABLE_HAL_LOCKS
  if (_lock == NULL) {
    log_e("MUTEX Lock failed. Can't begin.");
    return;
  }
#endif

  // map logical pins to GPIO numbers
  rxPin = digitalPinToGPIONumber(rxPin);
  txPin = digitalPinToGPIONumber(txPin);
  int8_t _rxPin = uart_get_RxPin(_uart_nr);
  int8_t _txPin = uart_get_TxPin(_uart_nr);

  rxPin = rxPin < 0 ? _rxPin : rxPin;
  txPin = txPin < 0 ? _txPin : txPin;

  HSERIAL_MUTEX_LOCK();
  // First Time or after end() --> set default Pins
  if (!uartIsDriverInstalled(_uart)) {
    // get previously used RX/TX pins, if any.
    int8_t _rxPin = uart_get_RxPin(_uart_nr);
    int8_t _txPin = uart_get_TxPin(_uart_nr);
    switch (_uart_nr) {
      case UART_NUM_0:
        if (rxPin < 0 && txPin < 0) {
          // do not change RX0/TX0 if it has already been set before
          rxPin = _rxPin < 0 ? (int8_t)SOC_RX0 : _rxPin;
          txPin = _txPin < 0 ? (int8_t)SOC_TX0 : _txPin;
        }
        break;
#if SOC_UART_HP_NUM > 1
      case UART_NUM_1:
        if (rxPin < 0 && txPin < 0) {
          // do not change RX1/TX1 if it has already been set before
          rxPin = _rxPin < 0 ? (int8_t)RX1 : _rxPin;
          txPin = _txPin < 0 ? (int8_t)TX1 : _txPin;
        }
        break;
#endif  // UART_NUM_1
#if SOC_UART_HP_NUM > 2
      case UART_NUM_2:
        if (rxPin < 0 && txPin < 0) {
          // do not change RX2/TX2 if it has already been set before
#ifdef RX2
          rxPin = _rxPin < 0 ? (int8_t)RX2 : _rxPin;
#endif
#ifdef TX2
          txPin = _txPin < 0 ? (int8_t)TX2 : _txPin;
#endif
        }
        break;
#endif  // UART_NUM_2
#if SOC_UART_HP_NUM > 3
      case UART_NUM_3:
        if (rxPin < 0 && txPin < 0) {
          // do not change RX3/TX3 if it has already been set before
#ifdef RX3
          rxPin = _rxPin < 0 ? (int8_t)RX3 : _rxPin;
#endif
#ifdef TX3
          txPin = _txPin < 0 ? (int8_t)TX3 : _txPin;
#endif
        }
        break;
#endif  // UART_NUM_3
#if SOC_UART_HP_NUM > 4
      case UART_NUM_4:
        if (rxPin < 0 && txPin < 0) {
          // do not change RX4/TX4 if it has already been set before
#ifdef RX4
          rxPin = _rxPin < 0 ? (int8_t)RX4 : _rxPin;
#endif
#ifdef TX4
          txPin = _txPin < 0 ? (int8_t)TX4 : _txPin;
#endif
        }
        break;
#endif  // UART_NUM_4
#if (SOC_UART_LP_NUM >= 1)
      case LP_UART_NUM_0:
        if (rxPin < 0 && txPin < 0) {
          // do not change RX0_LP/TX0_LP if it has already been set before
#ifdef LP_RX0
          rxPin = _rxPin < 0 ? (int8_t)LP_RX0 : _rxPin;
#endif
#ifdef LP_TX0
          txPin = _txPin < 0 ? (int8_t)LP_TX0 : _txPin;
#endif
        }
        break;
#endif  // LP_UART_NUM_0
    }
  }

  // if no RX/TX pins are defined, it will not start the UART driver
  if (rxPin < 0 && txPin < 0) {
    log_e("No RX/TX pins defined. Please set RX/TX pins.");
    HSERIAL_MUTEX_UNLOCK();
    return;
  }

  // IDF UART driver keeps Pin setting on restarting. Negative Pin number will keep it unmodified.
  // it will detach previous UART attached pins

  // indicates that uartbegin() has to initialize a new IDF driver
  if (_testUartBegin(_uart_nr, baud ? baud : 9600, config, rxPin, txPin, _rxBufferSize, _txBufferSize, invert, rxfifo_full_thrhd)) {
    _destroyEventTask();  // when IDF uart driver must be restarted, _eventTask must finish too
  }

  // IDF UART driver keeps Pin setting on restarting. Negative Pin number will keep it unmodified.
  // it will detach previous UART attached pins
  _uart = uartBegin(_uart_nr, baud ? baud : 9600, config, rxPin, txPin, _rxBufferSize, _txBufferSize, invert, rxfifo_full_thrhd);
  if (_uart == NULL) {
    log_e("UART driver failed to start. Please check the logs.");
    HSERIAL_MUTEX_UNLOCK();
    return;
  }
  if (!baud) {
    // using baud rate as zero, forces it to try to detect the current baud rate in place
    uartStartDetectBaudrate(_uart);
    time_t startMillis = millis();
    unsigned long detectedBaudRate = 0;
    while (millis() - startMillis < timeout_ms && !(detectedBaudRate = uartDetectBaudrate(_uart))) {
      yield();
    }

    if (detectedBaudRate) {
      delay(100);  // Give some time...
      _uart = uartBegin(_uart_nr, detectedBaudRate, config, rxPin, txPin, _rxBufferSize, _txBufferSize, invert, rxfifo_full_thrhd);
      if (_uart == NULL) {
        log_e("UART driver failed to start. Please check the logs.");
        HSERIAL_MUTEX_UNLOCK();
        return;
      }
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
  if (!_rxFIFOFull) {  // it has not being changed before calling begin()
    //  set a default FIFO Full value for the IDF driver
    uint8_t fifoFull = 1;
    // if baud rate is higher than 57600 or onReceive() is set, it will set FIFO Full to 120 bytes, except for LP UART
    if (_uart_nr < SOC_UART_HP_NUM && (baud > 57600 || (_onReceiveCB != NULL && _onReceiveTimeout))) {
      fifoFull = 120;
    }
    uartSetRxFIFOFull(_uart, fifoFull);
    _rxFIFOFull = fifoFull;
  }

  HSERIAL_MUTEX_UNLOCK();
}

void HardwareSerial::updateBaudRate(unsigned long baud) {
  uartSetBaudRate(_uart, baud);
}

void HardwareSerial::end() {
  // default Serial.end() will completely disable HardwareSerial,
  // including any tasks or debug message channel (log_x()) - but not for IDF log messages!
  _onReceiveCB = NULL;
  _onReceiveErrorCB = NULL;
  if (uartGetDebug() == _uart_nr) {
    uartSetDebug(0);
  }
  _rxFIFOFull = 0;
  uartEnd(_uart_nr);    // fully detach all pins and delete the UART driver
  _destroyEventTask();  // when IDF uart driver is deleted, _eventTask must finish too
  _uart = NULL;
}

void HardwareSerial::setDebugOutput(bool en) {
  if (_uart == 0) {
    return;
  }
#if (SOC_UART_LP_NUM >= 1)
  if (_uart_nr >= SOC_UART_HP_NUM) {
    log_e("LP UART does not support Debug Output.");
    return;
  }
#endif
  if (en) {
    uartSetDebug(_uart);
  } else {
    if (uartGetDebug() == _uart_nr) {
      uartSetDebug(NULL);
    }
  }
}

int HardwareSerial::available(void) {
  return uartAvailable(_uart);
}
int HardwareSerial::availableForWrite(void) {
  return uartAvailableForWrite(_uart);
}

int HardwareSerial::peek(void) {
  if (available()) {
    return uartPeek(_uart);
  }
  return -1;
}

int HardwareSerial::read(void) {
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
size_t HardwareSerial::read(uint8_t *buffer, size_t size) {
  return uartReadBytes(_uart, buffer, size, 0);
}

// Overrides Stream::readBytes() to be faster using IDF
size_t HardwareSerial::readBytes(uint8_t *buffer, size_t length) {
  return uartReadBytes(_uart, buffer, length, (uint32_t)getTimeout());
}

void HardwareSerial::flush(void) {
  uartFlush(_uart);
}

void HardwareSerial::flush(bool txOnly) {
  uartFlushTxOnly(_uart, txOnly);
}

size_t HardwareSerial::write(uint8_t c) {
  uartWrite(_uart, c);
  return 1;
}

size_t HardwareSerial::write(const uint8_t *buffer, size_t size) {
  uartWriteBuf(_uart, buffer, size);
  return size;
}

uint32_t HardwareSerial::baudRate() {
  return uartGetBaudRate(_uart);
}
HardwareSerial::operator bool() const {
  return uartIsDriverInstalled(_uart);
}

void HardwareSerial::setRxInvert(bool invert) {
  uartSetRxInvert(_uart, invert);
}

// negative Pin value will keep it unmodified
// can be called after or before begin()
bool HardwareSerial::setPins(int8_t rxPin, int8_t txPin, int8_t ctsPin, int8_t rtsPin) {
  // map logical pins to GPIO numbers
  rxPin = digitalPinToGPIONumber(rxPin);
  txPin = digitalPinToGPIONumber(txPin);
  ctsPin = digitalPinToGPIONumber(ctsPin);
  rtsPin = digitalPinToGPIONumber(rtsPin);

  // uartSetPins() checks if pins are valid and, if necessary, detaches the previous ones
  return uartSetPins(_uart_nr, rxPin, txPin, ctsPin, rtsPin);
}

// Enables or disables Hardware Flow Control using RTS and/or CTS pins
// must use setAllPins() in order to set RTS/CTS pins
// SerialHwFlowCtrl = UART_HW_FLOWCTRL_DISABLE, UART_HW_FLOWCTRL_RTS,
//                    UART_HW_FLOWCTRL_CTS, UART_HW_FLOWCTRL_CTS_RTS
bool HardwareSerial::setHwFlowCtrlMode(SerialHwFlowCtrl mode, uint8_t threshold) {
  return uartSetHwFlowCtrlMode(_uart, mode, threshold);
}

// Sets the uart mode in the esp32 uart for use with RS485 modes
// HwFlowCtrl must be disabled and RTS pin set
// SerialMode = UART_MODE_UART, UART_MODE_RS485_HALF_DUPLEX, UART_MODE_IRDA,
// or testing mode: UART_MODE_RS485_COLLISION_DETECT, UART_MODE_RS485_APP_CTRL
bool HardwareSerial::setMode(SerialMode mode) {
  return uartSetMode(_uart, mode);
}

// Sets the UART Clock Source based on the compatible SoC options
// This method must be called before starting UART using begin(), otherwise it won't have any effect.
// Clock Source Options are:
// UART_CLK_SRC_DEFAULT      :: any SoC - it will set whatever IDF defines as the default UART Clock Source
// UART_CLK_SRC_APB          :: ESP32, ESP32-S2, ESP32-C3 and ESP32-S3
// UART_CLK_SRC_PLL          :: ESP32-C2, ESP32-C5, ESP32-C6, ESP32-C61, ESP32-H2 and ESP32-P4
// UART_CLK_SRC_XTAL         :: ESP32-C2, ESP32-C3, ESP32-C5, ESP32-C6, ESP32-C61, ESP32-H2, ESP32-S3 and ESP32-P4
// UART_CLK_SRC_RTC          :: ESP32-C2, ESP32-C3, ESP32-C5, ESP32-C6, ESP32-C61, ESP32-H2, ESP32-S3 and ESP32-P4
// UART_CLK_SRC_REF_TICK     :: ESP32 and ESP32-S2
// Note: CLK_SRC_PLL Freq depends on the SoC - ESP32-C2 has 40MHz, ESP32-H2 has 48MHz and ESP32-C5, C6, C61 and P4 has 80MHz
// Note: ESP32-C6, C61, ESP32-P4 and ESP32-C5 have LP UART that will use only RTC_FAST or XTAL/2 as Clock Source
bool HardwareSerial::setClockSource(SerialClkSrc clkSrc) {
  if (_uart) {
    log_e("No Clock Source change was done. This function must be called before beginning UART%d.", _uart_nr);
    return false;
  }
  return uartSetClockSource(_uart_nr, (uart_sclk_t)clkSrc);
}
// minimum total RX Buffer size is the UART FIFO space (128 bytes for most SoC) + 1. IDF imposition.
// LP UART has FIFO of 16 bytes
size_t HardwareSerial::setRxBufferSize(size_t new_size) {

  if (_uart) {
    log_e("RX Buffer can't be resized when Serial is already running. Set it before calling begin().");
    return 0;
  }
  uint8_t FIFOLen = UART_HW_FIFO_LEN(_uart_nr);
  // Valid value is higher than the FIFO length
  if (new_size <= FIFOLen) {
    new_size = FIFOLen + 1;
    log_w("RX Buffer set to minimum value: %d.", new_size);
  }

  _rxBufferSize = new_size;
  return _rxBufferSize;
}

// minimum total TX Buffer size is the UART FIFO space (128 bytes for most SoC) + 1.
// LP UART has FIFO of 16 bytes
size_t HardwareSerial::setTxBufferSize(size_t new_size) {

  if (_uart) {
    log_e("TX Buffer can't be resized when Serial is already running. Set it before calling begin().");
    return 0;
  }
  uint8_t FIFOLen = UART_HW_FIFO_LEN(_uart_nr);
  // Valid values are zero or higher than the FIFO length
  if (new_size > 0 && new_size <= FIFOLen) {
    new_size = FIFOLen + 1;
    log_w("TX Buffer set to minimum value: %d.", new_size);
  }
  // if new_size is higher than SOC_UART_FIFO_LEN, TX Ringbuffer will be active and it will be used to report back "availableToWrite()"
  _txBufferSize = new_size;
  return new_size;
}
