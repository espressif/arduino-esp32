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

#include "esp32-hal-i2c.h"
#include "esp32-hal.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "driver/i2c.h"
#include "soc/soc_caps.h"

struct i2c_struct_t {

#if !CONFIG_DISABLE_HAL_LOCKS
    xSemaphoreHandle lock;
#endif

    uint8_t num;                // identification helper for the IDF I2C Driver 
    TickType_t i2c_timeout;     // i2c transaction timeout in FreeRTOS ticks
    uint8_t *i2c_cmd_buffer;    // pointer to the i2c buffer used in master mode
};

#define I2C_MASTER_TIMEOUT_MS           1000
#define I2C_MASTER_TIMEOUT_TICKS        (I2C_MASTER_TIMEOUT_MS / portTICK_RATE_MS)
#define I2C_MAXIMUM_TRANSACTION_SIZE    (I2C_LINK_RECOMMENDED_SIZE(5))
//I2C master doesn't need buffer
#define I2C_MASTER_TX_BUF_DISABLE       0
#define I2C_MASTER_RX_BUF_DISABLE       0



#if CONFIG_DISABLE_HAL_LOCKS
#define I2C_MUTEX_LOCK()
#define I2C_MUTEX_UNLOCK()

static i2c_t _i2c_bus_array[] = {
    {I2C_NUM_0, I2C_MASTER_TIMEOUT_TICKS, NULL},
#if SOC_I2C_NUM > 1
    {I2C_NUM_1, I2C_MASTER_TIMEOUT_TICKS, NULL},
#endif
};
#else
#define I2C_MUTEX_LOCK()    do {} while (xSemaphoreTakeRecursive(i2c->lock, portMAX_DELAY) != pdPASS)
#define I2C_MUTEX_UNLOCK()  xSemaphoreGiveRecursive(i2c->lock)

static i2c_t _i2c_bus_array[] = {
    {NULL, I2C_NUM_0, I2C_MASTER_TIMEOUT_TICKS, NULL},
#if SOC_I2C_NUM > 1
    {NULL, I2C_NUM_1, I2C_MASTER_TIMEOUT_TICKS, NULL},
#endif
};
#endif

bool i2c_is_driver_installed(i2c_t *i2c)
{
    return i2c->i2c_cmd_buffer == NULL ? false : true;
}

i2c_t* i2cMasterInit(int8_t i2c_num, int8_t sda, int8_t scl, uint32_t clk_speed) 
{
    if (0 > i2c_num || i2c_num > (I2C_NUM_MAX - 1)) {
        log_e("I2C number is invalid, please use numbers from 0 to %u\n", I2C_NUM_MAX - 1);
        return NULL;
    }

    
    if(sda < 0 || scl < 0 || sda > (SOC_GPIO_PIN_COUNT - 1) || scl > (SOC_GPIO_PIN_COUNT - 1)) {
        log_e("Some I2C pin number is invalid, please use pin numbers from 0 to %u\n", SOC_GPIO_PIN_COUNT - 1);
        return NULL;
    }

	if(clk_speed == 0){
        log_e("I2C bus clock can not be zero.\n");
        return NULL;
	}

    i2c_t* i2c = &_i2c_bus_array[i2c_num];

    if (i2c_is_driver_installed(i2c)) {
        i2cRelease(i2c);
    }

#if !CONFIG_DISABLE_HAL_LOCKS
    if(i2c->lock == NULL) {
        i2c->lock = xSemaphoreCreateMutex();
        if(i2c->lock == NULL) {
            return NULL;
        }
    }
#endif

    I2C_MUTEX_LOCK();

    if (i2c->i2c_cmd_buffer == NULL) {
        i2c->i2c_cmd_buffer = (uint8_t *) calloc(1, I2C_MAXIMUM_TRANSACTION_SIZE);
        if (i2c->i2c_cmd_buffer == NULL) {
            log_e("Not enough heap memory to start i2c driver.\n");
            if (i2c->lock != NULL) {
                vSemaphoreDelete(i2c->lock);
                i2c->lock = NULL;
            }
            return NULL;
        }
    }

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = scl,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = clk_speed,
        // .clk_flags = 0,          /*!< Optional, you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here. */
    };

    ESP_ERROR_CHECK(i2c_param_config(i2c_num, &conf));    
    ESP_ERROR_CHECK(i2c_driver_install(i2c_num, I2C_MODE_MASTER, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0));

    I2C_MUTEX_UNLOCK();

    i2cFlush(i2c);
    return i2c;
}

void i2cRelease(i2c_t *i2c)
{
    if (i2c == NULL) {
        return;
    }

    I2C_MUTEX_LOCK();
    ESP_ERROR_CHECK(i2c_driver_delete(i2c->num));
    if (i2c->i2c_cmd_buffer != NULL) {
        free(i2c->i2c_cmd_buffer);
        i2c->i2c_cmd_buffer = NULL;
    }
    I2C_MUTEX_UNLOCK();

}

void i2cFlush(i2c_t *i2c){

    if (i2c == NULL) {
        return;
    }

    I2C_MUTEX_LOCK();
	i2c_reset_tx_fifo(i2c->num);
    i2c_reset_rx_fifo(i2c->num);
    I2C_MUTEX_UNLOCK();
}

void i2cSetTimeOut(i2c_t *i2c, uint16_t timeOutMillis) 
{
    if (i2c == NULL) {
        return;
    }
    I2C_MUTEX_LOCK();
    // this timeout in ticks will be used for every i2c bus operation
    i2c->i2c_timeout = timeOutMillis / portTICK_RATE_MS;
    I2C_MUTEX_UNLOCK();
}

int i2cTranslateError(int err) {
    switch (err) {
        case ESP_OK:
            return I2C_ERROR_OK;
        case ESP_ERR_NO_MEM:
            return I2C_ERROR_MEMORY;
        default:
            return I2C_ERROR_I2C_OTHER;
    }
}


/* i2c_cmd_buffer must have been initialized correctly before using these functions 
 *
 * @return for i2cWriteByte(), i2cWriteBuffer(), i2cRead(), i2cWriteSlaveAddr() and i2cStartTransaction()
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_ERR_NO_MEM The static buffer used to create `cmd_handler` is too small
 *     - ESP_FAIL No more memory left on the heap
 */
int i2cWriteByte(i2c_t *i2c, uint8_t data)
{
    if (i2c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = ESP_OK;
    I2C_MUTEX_LOCK();
    err = i2c_master_write_byte((i2c_cmd_handle_t) i2c->i2c_cmd_buffer, data, true);
    I2C_MUTEX_UNLOCK();

    log_d("master_write byte = %d return = %d ", data, err);
    if (err != ESP_OK) log_w("Could not write Byte I2C transaction. Err = %d", err);

    return err;
}

int i2cWriteBuffer(i2c_t *i2c, uint8_t *write_buffer, size_t write_size)
{
    if (i2c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = ESP_OK;
    I2C_MUTEX_LOCK();
    err = i2c_master_write((i2c_cmd_handle_t) i2c->i2c_cmd_buffer, write_buffer, write_size, true);
    I2C_MUTEX_UNLOCK();

    log_d("master_write with write size = %d return = %d ", write_size, err);
    if (err != ESP_OK) log_w("Could not write I2C transaction. Err = %d", err);

    return err;
}

int i2cRead(i2c_t *i2c, uint8_t* read_buffer, size_t read_size){
    if (i2c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = ESP_OK;
    I2C_MUTEX_LOCK();
    err = i2c_master_read((i2c_cmd_handle_t) i2c->i2c_cmd_buffer, read_buffer, read_size, I2C_MASTER_LAST_NACK);
    I2C_MUTEX_UNLOCK();

    log_d("master_read with read size = %d return = %d ", read_size, err);
    if (err != ESP_OK) log_w("Could not read I2C transaction. Err = %d", err);

    return err;
}

int i2cWriteSlaveAddr(i2c_t *i2c, uint16_t address, bool readOp)
{
    if (i2c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = ESP_OK;
    uint8_t device_address = address;
    I2C_MUTEX_LOCK();
    if (readOp) {
        err = i2c_master_write_byte((i2c_cmd_handle_t) i2c->i2c_cmd_buffer, device_address << 1 | I2C_MASTER_READ, true);
    } else {
        err = i2c_master_write_byte((i2c_cmd_handle_t) i2c->i2c_cmd_buffer, device_address << 1 | I2C_MASTER_WRITE, true);
    }
    I2C_MUTEX_UNLOCK();

    log_d("write_byte slave addr %d with operation = %s return = %d ", address, readOp ? "READ" : "WRITE", err);
    if (err != ESP_OK) log_w("Could not read/write slave ADDR. Err = %d", err);

    return err;
}

int i2cStartTransaction(i2c_t *i2c, bool init_cmd_link)
{
    if (i2c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = ESP_OK;
    I2C_MUTEX_LOCK();
    if (init_cmd_link) {
        i2c_cmd_link_create_static(i2c->i2c_cmd_buffer, I2C_MAXIMUM_TRANSACTION_SIZE); 
    }
    err = i2c_master_start((i2c_cmd_handle_t) i2c->i2c_cmd_buffer);
    I2C_MUTEX_UNLOCK();

    log_d("master_start with init_cmd_link = %s return = %d ", init_cmd_link ? "TRUE" : "FALSE", err);
    if (err != ESP_OK) log_w("Could not start I2C transaction. Err = %d", err);

    return err;
}

/* i2cCloseTransaction() initiates I2C transaction sending 
 * data and also reading data from Slave.
 *
 * @return Arduino Codes (i2c_err_t) based on the return from IDF
 *     IDF API return:
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_FAIL Sending command error, slave hasn't ACK the transfer. -- i2c_master_cmd_begin()
 *     - ESP_FAIL No more memory left on the heap -- i2c_master_stop()
 *     - ESP_ERR_INVALID_STATE I2C driver not installed or not in master mode.
 *     - ESP_ERR_TIMEOUT Operation timeout because the bus is busy.
 *     - ESP_ERR_NO_MEM The static buffer used to create `cmd_handler` is too small
 */
int i2cCloseTransaction(i2c_t *i2c) 
{
    if (i2c == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = ESP_OK;

    I2C_MUTEX_LOCK();
    err = i2c_master_stop((i2c_cmd_handle_t) i2c->i2c_cmd_buffer);

    log_d("master_stop. return = %d ", err);
    if (err != ESP_OK) log_w("Could not stop I2C transaction. Err = %d", err);

    bool sentStopOK = (err == ESP_OK);
    if (sentStopOK) {
        err = i2c_master_cmd_begin(i2c->num, (i2c_cmd_handle_t) i2c->i2c_cmd_buffer, i2c->i2c_timeout);

        log_d("master_cmd_begin. return = %d ", err);
        if (err != ESP_OK) log_w("Could not SEND/EXEC I2C transaction. Err = %d", err);

        i2c_cmd_link_delete_static((i2c_cmd_handle_t) i2c->i2c_cmd_buffer);
    } else {
        log_w("Could not close I2C transaction due to not sending stop. Err = %d", err);
    }
    I2C_MUTEX_UNLOCK();

    // translation of IDF error codes to Arduino Wire error codes:
    switch(err) {
        case ESP_OK:
            return I2C_ERROR_OK;            // ARDUINO CODE for success (0)
        case ESP_ERR_NO_MEM:
            return I2C_ERROR_MEMORY;        // ARDUINO CODE for length to long for buffer - memory issues (1)
        case ESP_FAIL:
            if (sentStopOK) {
                return I2C_ERROR_ADDR_NACK; // ARDUINO CODE for Slave NACK on Slave ADDR (2) --> (3) is for  NACK on data : can't tell it from IDF I2C dirver
            } else {
                return I2C_ERROR_MEMORY;    // ARDUINO CODE for length to long for buffer - memory issues (1)
            }
        case ESP_ERR_TIMEOUT:
            return I2C_ERROR_TIMEOUT;        // ARDUINO CODE for timeout (5)
        default:
            return I2C_ERROR_I2C_OTHER;     // ARDUINO CODE for other twi error (lost bus arbitration, bus error, bad argument..) (5)
    }
 
    return err;
}
