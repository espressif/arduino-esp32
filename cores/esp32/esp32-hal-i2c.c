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
#include "rom/ets_sys.h"
#include "driver/periph_ctrl.h"
#include "soc/i2c_reg.h"
#include "soc/i2c_struct.h"
#include "soc/dport_reg.h"

//#define I2C_DEV(i)   (volatile i2c_dev_t *)((i)?DR_REG_I2C1_EXT_BASE:DR_REG_I2C_EXT_BASE)
//#define I2C_DEV(i)   ((i2c_dev_t *)(REG_I2C_BASE(i)))
#define I2C_SCL_IDX(p)  ((p==0)?I2CEXT0_SCL_OUT_IDX:((p==1)?I2CEXT1_SCL_OUT_IDX:0))
#define I2C_SDA_IDX(p) ((p==0)?I2CEXT0_SDA_OUT_IDX:((p==1)?I2CEXT1_SDA_OUT_IDX:0))

#define DR_REG_I2C_EXT_BASE_FIXED               0x60013000
#define DR_REG_I2C1_EXT_BASE_FIXED              0x60027000

#define COMMAND_BUFFER_LENGTH                   16

struct i2c_struct_t {
    i2c_dev_t * dev;
#if !CONFIG_DISABLE_HAL_LOCKS
    xSemaphoreHandle lock;
#endif
    uint8_t num;
};

enum {
    I2C_CMD_RSTART,
    I2C_CMD_WRITE,
    I2C_CMD_READ,
    I2C_CMD_STOP,
    I2C_CMD_END
};

#if CONFIG_DISABLE_HAL_LOCKS
#define I2C_MUTEX_LOCK()
#define I2C_MUTEX_UNLOCK()

static i2c_t _i2c_bus_array[2] = {
    {(volatile i2c_dev_t *)(DR_REG_I2C_EXT_BASE_FIXED), 0},
    {(volatile i2c_dev_t *)(DR_REG_I2C1_EXT_BASE_FIXED), 1}
};
#else
#define I2C_MUTEX_LOCK()    do {} while (xSemaphoreTake(i2c->lock, portMAX_DELAY) != pdPASS)
#define I2C_MUTEX_UNLOCK()  xSemaphoreGive(i2c->lock)

static i2c_t _i2c_bus_array[2] = {
    {(volatile i2c_dev_t *)(DR_REG_I2C_EXT_BASE_FIXED), NULL, 0},
    {(volatile i2c_dev_t *)(DR_REG_I2C1_EXT_BASE_FIXED), NULL, 1}
};
#endif

i2c_err_t i2cAttachSCL(i2c_t * i2c, int8_t scl)
{
    if(i2c == NULL){
        return I2C_ERROR_DEV;
    }
    pinMode(scl, OUTPUT_OPEN_DRAIN | PULLUP);
    pinMatrixOutAttach(scl, I2C_SCL_IDX(i2c->num), false, false);
    pinMatrixInAttach(scl, I2C_SCL_IDX(i2c->num), false);
    return I2C_ERROR_OK;
}

i2c_err_t i2cDetachSCL(i2c_t * i2c, int8_t scl)
{
    if(i2c == NULL){
        return I2C_ERROR_DEV;
    }
    pinMatrixOutDetach(scl, false, false);
    pinMatrixInDetach(I2C_SCL_IDX(i2c->num), false, false);
    pinMode(scl, INPUT);
    return I2C_ERROR_OK;
}

i2c_err_t i2cAttachSDA(i2c_t * i2c, int8_t sda)
{
    if(i2c == NULL){
        return I2C_ERROR_DEV;
    }
    pinMode(sda, OUTPUT_OPEN_DRAIN | PULLUP);
    pinMatrixOutAttach(sda, I2C_SDA_IDX(i2c->num), false, false);
    pinMatrixInAttach(sda, I2C_SDA_IDX(i2c->num), false);
    return I2C_ERROR_OK;
}

i2c_err_t i2cDetachSDA(i2c_t * i2c, int8_t sda)
{
    if(i2c == NULL){
        return I2C_ERROR_DEV;
    }
    pinMatrixOutDetach(sda, false, false);
    pinMatrixInDetach(I2C_SDA_IDX(i2c->num), false, false);
    pinMode(sda, INPUT);
    return I2C_ERROR_OK;
}

/*
 * index     - command index (0 to 15)
 * op_code   - is the command
 * byte_num  - This register is to store the amounts of data that is read and written. byte_num in RSTART, STOP, END is null.
 * ack_val   - Each data byte is terminated by an ACK bit used to set the bit level.
 * ack_exp   - This bit is to set an expected ACK value for the transmitter.
 * ack_check - This bit is to decide whether the transmitter checks ACK bit. 1 means yes and 0 means no.
 * */
void i2cSetCmd(i2c_t * i2c, uint8_t index, uint8_t op_code, uint8_t byte_num, bool ack_val, bool ack_exp, bool ack_check)
{
    i2c->dev->command[index].val = 0;
    i2c->dev->command[index].ack_en = ack_check;
    i2c->dev->command[index].ack_exp = ack_exp;
    i2c->dev->command[index].ack_val = ack_val;
    i2c->dev->command[index].byte_num = byte_num;
    i2c->dev->command[index].op_code = op_code;
}

void i2cResetCmd(i2c_t * i2c) {
    uint8_t i;
    for(i=0;i<16;i++){
        i2c->dev->command[i].val = 0;
    }
}

void i2cResetFiFo(i2c_t * i2c) {
    i2c->dev->fifo_conf.tx_fifo_rst = 1;
    i2c->dev->fifo_conf.tx_fifo_rst = 0;
    i2c->dev->fifo_conf.rx_fifo_rst = 1;
    i2c->dev->fifo_conf.rx_fifo_rst = 0;
}

i2c_err_t i2cWrite(i2c_t * i2c, uint16_t address, bool addr_10bit, uint8_t * data, uint16_t len, bool sendStop)
{
    int i;
    uint16_t index = 0;
    uint16_t dataLen = len + (addr_10bit?2:1);
    address = (address << 1);

    if(i2c == NULL){
        return I2C_ERROR_DEV;
    }

    I2C_MUTEX_LOCK();

    if (i2c->dev->status_reg.bus_busy == 1)
    {
        log_e( "Busy Timeout! Addr: %x", address >> 1 );
        I2C_MUTEX_UNLOCK();
        return I2C_ERROR_BUSY;
    }

    while(dataLen) {
        uint8_t willSend = (dataLen > 32)?32:dataLen;
        uint8_t dataSend = willSend;

        i2cResetFiFo(i2c);
        i2cResetCmd(i2c);
        //Clear Interrupts
        i2c->dev->int_clr.val = 0xFFFFFFFF;

        //CMD START
        i2cSetCmd(i2c, 0, I2C_CMD_RSTART, 0, false, false, false);

        //CMD WRITE(ADDRESS + DATA)
        if(!index) {
            if(addr_10bit){// address is leftshifted with Read/Write bit set
                i2c->dev->fifo_data.data = (((address >> 8) & 0x6) | 0xF0); // send a9:a8 plus 1111 0xxW mask
                i2c->dev->fifo_data.data = ((address >> 1) & 0xFF); // send a7:a0, remove W bit (7bit address style)
                dataSend -= 2;
            }
            else { // 7bit address
                i2c->dev->fifo_data.data = address & 0xFF;
                dataSend--;
            }
        }
        i = 0;
        while(i<dataSend) {
            i++;
            i2c->dev->fifo_data.val = data[index++];
            while(i2c->dev->status_reg.tx_fifo_cnt < i);
        }
        i2cSetCmd(i2c, 1, I2C_CMD_WRITE, willSend, false, false, true);
        dataLen -= willSend;

        //CMD STOP or CMD END if there is more data
        if(dataLen || !sendStop) {
            i2cSetCmd(i2c, 2, I2C_CMD_END, 0, false, false, false);
        } else if(sendStop) {
            i2cSetCmd(i2c, 2, I2C_CMD_STOP, 0, false, false, false);
        }

        //START Transmission
        i2c->dev->ctr.trans_start = 1;

        //WAIT Transmission
        uint32_t startAt = millis();
        while(1) {
            //have been looping for too long
            if((millis() - startAt)>50){
                log_e("Timeout! Addr: %x", address >> 1);
                I2C_MUTEX_UNLOCK();
                return I2C_ERROR_BUS;
            }

            //Bus failed (maybe check for this while waiting?
            if(i2c->dev->int_raw.arbitration_lost) {
                log_e("Bus Fail! Addr: %x", address >> 1);
                I2C_MUTEX_UNLOCK();
                return I2C_ERROR_BUS;
            }

            //Bus timeout
            if(i2c->dev->int_raw.time_out) {
                log_e("Bus Timeout! Addr: %x", address >> 1);
                I2C_MUTEX_UNLOCK();
                return I2C_ERROR_TIMEOUT;
            }

            //Transmission did not finish and ACK_ERR is set
            if(i2c->dev->int_raw.ack_err) {
                log_w("Ack Error! Addr: %x", address >> 1);
                while((i2c->dev->status_reg.bus_busy) && ((millis() - startAt)<50));
                I2C_MUTEX_UNLOCK();
                return I2C_ERROR_ACK;
            }

            if((sendStop && i2c->dev->command[2].done) || !i2c->dev->status_reg.bus_busy){
                break;
            }
        }

    }
    I2C_MUTEX_UNLOCK();
    return I2C_ERROR_OK;
}

uint8_t inc( uint8_t* index )
{
    uint8_t i = index[ 0 ];
    if (++index[ 0 ] == COMMAND_BUFFER_LENGTH)
    {
        index[ 0  ] = 0;
    }

    return i;
}

i2c_err_t i2cRead(i2c_t * i2c, uint16_t address, bool addr_10bit, uint8_t * data, uint16_t len, bool sendStop)
{
    address = (address << 1) | 1;
    uint8_t addrLen = (addr_10bit?2:1);
    uint8_t amountRead[16];
    uint16_t index = 0;
    uint8_t cmdIdx = 0, currentCmdIdx = 0, nextCmdCount;
    bool stopped = false, isEndNear = false;
    uint8_t willRead;

    if(i2c == NULL){
        return I2C_ERROR_DEV;
    }

    I2C_MUTEX_LOCK();

    if (i2c->dev->status_reg.bus_busy == 1)
    {
        log_w( "Busy Timeout! Addr: %x", address >> 1 );
        I2C_MUTEX_UNLOCK();
        return I2C_ERROR_BUSY;
    }

    i2cResetFiFo(i2c);
    i2cResetCmd(i2c);

    //CMD START
    i2cSetCmd(i2c, cmdIdx++, I2C_CMD_RSTART, 0, false, false, false);

    //CMD WRITE ADDRESS
    if (addr_10bit) { // address is left-shifted with Read/Write bit set
       i2c->dev->fifo_data.data = (((address >> 8) & 0x6) | 0xF1); // send a9:a8 plus 1111 0xxR mask
       i2c->dev->fifo_data.data = ((address >> 1) & 0xFF); // send a7:a0, remove R bit (7bit address style)
    }
    else { // 7bit address
       i2c->dev->fifo_data.data = address & 0xFF;
    }
    i2cSetCmd(i2c, cmdIdx++, I2C_CMD_WRITE, addrLen, false, false, true);
    nextCmdCount = cmdIdx;

    //Clear Interrupts
    i2c->dev->int_clr.val = 0x00001FFF;

    //START Transmission
    i2c->dev->ctr.trans_start = 1;
    while (!stopped) {
        //WAIT Transmission
        uint32_t startAt = millis();
        while(1) {
            //have been looping for too long
            if((millis() - startAt)>50) {
                log_e("Timeout! Addr: %x, index %d", (address >> 1), index);
                I2C_MUTEX_UNLOCK();
                return I2C_ERROR_BUS;
            }

            //Bus failed (maybe check for this while waiting?
            if(i2c->dev->int_raw.arbitration_lost) {
                log_e("Bus Fail! Addr: %x", (address >> 1));
                I2C_MUTEX_UNLOCK();
                return I2C_ERROR_BUS;
            }

            //Bus timeout
            if(i2c->dev->int_raw.time_out) {
                log_e("Bus Timeout! Addr: %x, index %d", (address >> 1), index );
                I2C_MUTEX_UNLOCK();
                return I2C_ERROR_TIMEOUT;
            }

            //Transmission did not finish and ACK_ERR is set
            if(i2c->dev->int_raw.ack_err) {
                log_w("Ack Error! Addr: %x", address >> 1);
                while((i2c->dev->status_reg.bus_busy) && ((millis() - startAt)<50));
                I2C_MUTEX_UNLOCK();
                return I2C_ERROR_ACK;
            }

            // Save bytes from the buffer as they arrive instead of doing them at the end of the loop since there is no
            // pause from an END operation in this approach.
            if((!isEndNear) && (nextCmdCount < 2)) {
                willRead = ((len>32)?32:len);
                if (willRead > 0) {
                    if (willRead > 1) {
                        i2cSetCmd(i2c, cmdIdx, I2C_CMD_READ, (amountRead[ inc( &cmdIdx ) ] = willRead -1), false, false, false);
                        nextCmdCount++;
                    }
                    i2cSetCmd(i2c, cmdIdx, I2C_CMD_READ, (amountRead[ inc( &cmdIdx ) ] = 1), (len<=32), false, false);
                    nextCmdCount++;
                    len -= willRead;
                } else {
                    i2cSetCmd(i2c, inc( &cmdIdx ), I2C_CMD_STOP, 0, false, false, false);
                    isEndNear = true;
                    nextCmdCount++;
                }
            }

            if(i2c->dev->command[currentCmdIdx].done) {
                nextCmdCount--;
                if (i2c->dev->command[currentCmdIdx].op_code == I2C_CMD_READ) {
                    while(amountRead[currentCmdIdx]>0) {
                        data[index++] = i2c->dev->fifo_data.val & 0xFF;
                        amountRead[currentCmdIdx]--;
                    }
                    i2cResetFiFo(i2c);
                } else if (i2c->dev->command[currentCmdIdx].op_code == I2C_CMD_STOP) {
                    stopped = true;
                }
                inc( &currentCmdIdx );
                break;
            }
        }
    }
    I2C_MUTEX_UNLOCK();

    return I2C_ERROR_OK;
}

i2c_err_t i2cSetFrequency(i2c_t * i2c, uint32_t clk_speed)
{
    if(i2c == NULL){
        return I2C_ERROR_DEV;
    }

    uint32_t period = (APB_CLK_FREQ/clk_speed) / 2;
    uint32_t halfPeriod = period/2;
    uint32_t quarterPeriod = period/4;

    I2C_MUTEX_LOCK();
    //the clock num during SCL is low level
    i2c->dev->scl_low_period.period = period;
    //the clock num during SCL is high level
    i2c->dev->scl_high_period.period = period;

    //the clock num between the negedge of SDA and negedge of SCL for start mark
    i2c->dev->scl_start_hold.time = halfPeriod;
    //the clock num between the posedge of SCL and the negedge of SDA for restart mark
    i2c->dev->scl_rstart_setup.time = halfPeriod;

    //the clock num after the STOP bit's posedge
    i2c->dev->scl_stop_hold.time = halfPeriod;
    //the clock num between the posedge of SCL and the posedge of SDA
    i2c->dev->scl_stop_setup.time = halfPeriod;

    //the clock num I2C used to hold the data after the negedge of SCL.
    i2c->dev->sda_hold.time = quarterPeriod;
    //the clock num I2C used to sample data on SDA after the posedge of SCL
    i2c->dev->sda_sample.time = quarterPeriod;
    I2C_MUTEX_UNLOCK();
    return I2C_ERROR_OK;
}

uint32_t i2cGetFrequency(i2c_t * i2c)
{
    if(i2c == NULL){
        return 0;
    }

    return APB_CLK_FREQ/(i2c->dev->scl_low_period.period+i2c->dev->scl_high_period.period);
}

/*
 * mode          - 0 = Slave, 1 = Master
 * slave_addr    - I2C Address
 * addr_10bit_en - enable slave 10bit address mode.
 * */

i2c_t * i2cInit(uint8_t i2c_num, uint16_t slave_addr, bool addr_10bit_en)
{
    if(i2c_num > 1){
        return NULL;
    }

    i2c_t * i2c = &_i2c_bus_array[i2c_num];

#if !CONFIG_DISABLE_HAL_LOCKS
    if(i2c->lock == NULL){
        i2c->lock = xSemaphoreCreateMutex();
        if(i2c->lock == NULL) {
            return NULL;
        }
    }
#endif

    if(i2c_num == 0) {
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG,DPORT_I2C_EXT0_RST); // I2C0 peripheral into Reset mode
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG,DPORT_I2C_EXT0_CLK_EN);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG,DPORT_I2C_EXT0_RST); // Release Reset Mode
    } else {
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG,DPORT_I2C_EXT1_RST); // I2C1 peripheral into Reset Mode
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG,DPORT_I2C_EXT1_CLK_EN);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG,DPORT_I2C_EXT1_RST); // Release Reset Mode
    }

    I2C_MUTEX_LOCK();
    i2c->dev->ctr.val = 0;
    i2c->dev->ctr.ms_mode = (slave_addr == 0);
    i2c->dev->ctr.sda_force_out = 1 ;
    i2c->dev->ctr.scl_force_out = 1 ;
    i2c->dev->ctr.clk_en = 1;

    //the max clock number of receiving  a data
    i2c->dev->timeout.tout = 1048575;//clocks max=1048575
    //disable apb nonfifo access
    i2c->dev->fifo_conf.nonfifo_en = 0;

    i2c->dev->slave_addr.val = 0;
    if (slave_addr) {
        i2c->dev->slave_addr.addr = slave_addr;
        i2c->dev->slave_addr.en_10bit = addr_10bit_en;
    }
    I2C_MUTEX_UNLOCK();

    return i2c;
}

void i2cInitFix(i2c_t * i2c){
    if(i2c == NULL){
        return;
    }
    I2C_MUTEX_LOCK();
    i2cResetFiFo(i2c);
    i2cResetCmd(i2c);
    i2c->dev->int_clr.val = 0xFFFFFFFF;
    i2cSetCmd(i2c, 0, I2C_CMD_RSTART, 0, false, false, false);
    i2c->dev->fifo_data.data = 0;
    i2cSetCmd(i2c, 1, I2C_CMD_WRITE, 1, false, false, false);
    i2cSetCmd(i2c, 2, I2C_CMD_STOP, 0, false, false, false);
    if (i2c->dev->status_reg.bus_busy) // If this condition is true, the while loop will timeout as done will not be set
    {
        log_e("Busy at initialization!");
    }
    i2c->dev->ctr.trans_start = 1;
    uint16_t count = 50000;
    while ((!i2c->dev->command[2].done) && (--count > 0));
    I2C_MUTEX_UNLOCK();
}

void i2cReset(i2c_t* i2c){
    if(i2c == NULL){
        return;
    }
    I2C_MUTEX_LOCK();
    periph_module_t moduleId = (i2c == &_i2c_bus_array[0])?PERIPH_I2C0_MODULE:PERIPH_I2C1_MODULE;
    periph_module_disable( moduleId );
    delay( 20 ); // Seems long but delay was chosen to ensure system teardown and setup without core generation
    periph_module_enable( moduleId );
    I2C_MUTEX_UNLOCK();
}

