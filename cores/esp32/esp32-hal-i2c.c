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
#include "freertos/event_groups.h"
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
	
struct i2c_struct_t {
    i2c_dev_t * dev;
#if !CONFIG_DISABLE_HAL_LOCKS
    xSemaphoreHandle lock;
#endif
    uint8_t num;
		I2C_MODE_t mode;
    I2C_STAGE_t stage;
		I2C_ERROR_t error;
		EventGroupHandle_t i2c_event; // a way to monitor ISR process
		// maybe use it to trigger callback for OnRequest()
		intr_handle_t intr_handle;       /*!< I2C interrupt handle*/
    I2C_DATA_QUEUE_t * dq;
    uint16_t queueCount;
    uint16_t queuePos;
    uint16_t byteCnt;
		
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
    {(volatile i2c_dev_t *)(DR_REG_I2C_EXT_BASE_FIXED), 0,I2C_NONE,I2C_NONE,I2C_ERROR_OK,NULL,NULL,NULL,0,0,0},
    {(volatile i2c_dev_t *)(DR_REG_I2C1_EXT_BASE_FIXED), 1,I2C_NONE,I2C_NONE,I2C_ERROR_OK,NULL,NULL,NULL,0,0,0}
};
#else
#define I2C_MUTEX_LOCK()    do {} while (xSemaphoreTake(i2c->lock, portMAX_DELAY) != pdPASS)
#define I2C_MUTEX_UNLOCK()  xSemaphoreGive(i2c->lock)

static i2c_t _i2c_bus_array[2] = {
    {(volatile i2c_dev_t *)(DR_REG_I2C_EXT_BASE_FIXED), NULL, 0,I2C_NONE,I2C_NONE,I2C_ERROR_OK,NULL,NULL,NULL,0,0,0},
    {(volatile i2c_dev_t *)(DR_REG_I2C1_EXT_BASE_FIXED), NULL, 1,I2C_NONE,I2C_NONE,I2C_ERROR_OK,NULL,NULL,NULL,0,0,0}
};
#endif

  
//forward for ISR, added IRAM_ATTR?
static void IRAM_ATTR i2c_isr_handler_default(void* arg);

static i2c_err_t i2cAddQueue(i2c_t * i2c,uint8_t mode, uint16_t i2cDeviceAddr, uint8_t *dataPtr, uint16_t dataLen,bool sendStop){

  I2C_DATA_QUEUE_t dqx;
  dqx.data = dataPtr;
  dqx.length = dataLen;
  dqx.position = 0;
  dqx.cmdBytesNeeded = dataLen;
  dqx.ctrl.val = 0;
  dqx.ctrl.addr = i2cDeviceAddr;
  dqx.ctrl.mode = mode;
  dqx.ctrl.stop= sendStop;
  dqx.ctrl.addrReq = ((i2cDeviceAddr&0x7800)==0x7800)?2:1; // 10bit or 7bit address
  dqx.queueLength = dataLen + dqx.ctrl.addrReq;
 
if(i2c->dq!=NULL){ // expand
  I2C_DATA_QUEUE_t* tq =(I2C_DATA_QUEUE_t*)realloc(i2c->dq,sizeof(I2C_DATA_QUEUE_t)*(i2c->queueCount +1));
  if(tq!=NULL){// ok
    i2c->dq = tq;
    memmove(&i2c->dq[i2c->queueCount++],&dqx,sizeof(I2C_DATA_QUEUE_t));
    }
  else { // bad stuff, unable to allocate more memory!
    log_e("realloc Failure");
    return I2C_ERROR_MEMORY;
    }
  }
else { // first Time
  i2c->queueCount=0;
  i2c->dq =(I2C_DATA_QUEUE_t*)malloc(sizeof(I2C_DATA_QUEUE_t));
  if(i2c->dq!=NULL){
    memmove(&i2c->dq[i2c->queueCount++],&dqx,sizeof(I2C_DATA_QUEUE_t));
    }
  else {
    log_e("malloc failure");
    return I2C_ERROR_MEMORY;
    }
  }
return I2C_ERROR_OK;
}

i2c_err_t i2cFreeQueue(i2c_t * i2c){
if(i2c->dq!=NULL){
  free(i2c->dq);
  i2c->dq = NULL;
  }
i2c->queueCount=0;
i2c->queuePos=0;
}

i2c_err_t i2cAddQueueWrite(i2c_t * i2c, uint16_t i2cDeviceAddr, uint8_t *dataPtr, uint16_t dataLen,bool sendStop){
  
  return i2cAddQueue(i2c,0,i2cDeviceAddr,dataPtr,dataLen,sendStop); 
}

i2c_err_t i2cAddQueueRead(i2c_t * i2c, uint16_t i2cDeviceAddr, uint8_t *dataPtr, uint16_t dataLen,bool sendStop){
  
  return i2cAddQueue(i2c,1,i2cDeviceAddr,dataPtr,dataLen,sendStop); 
}

uint16_t i2cQueueReadPendingCount(i2c_t * i2c){
uint16_t cnt=0;
uint16_t a=i2c->queuePos;
while(a<i2c->queueCount){
  if(i2c->dq[a].ctrl.mode==1){
    cnt += i2c->dq[a].length - i2c->dq[a].position;
    }
  a++;
  }
return cnt;
}
  
uint16_t i2cQueueReadCount(i2c_t * i2c){
uint16_t cnt=0;
uint16_t a=0;
while( a < i2c->queueCount){
  if(i2c->dq[a].ctrl.mode==1){
    cnt += i2c->dq[a].position; //if postion is > 0 then some reads have happened
    }
  a++;
  }
return cnt;
}


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
	  I2C_COMMAND_t cmd;
		cmd.val=0;
		cmd.ack_en = ack_check;
		cmd.ack_exp = ack_exp;
		cmd.ack_val = ack_val;
		cmd.byte_num = byte_num;
		cmd.op_code = op_code;
    i2c->dev->command[index].val = cmd.val;
/*
    i2c->dev->command[index].ack_en = ack_check;
    i2c->dev->command[index].ack_exp = ack_exp;
    i2c->dev->command[index].ack_val = ack_val;
    i2c->dev->command[index].byte_num = byte_num;
    i2c->dev->command[index].op_code = op_code;
*/
}

void i2cResetCmd(i2c_t * i2c){
    int i;
    for(i=0;i<16;i++){
        i2c->dev->command[i].val = 0;
    }
}

void i2cResetFiFo(i2c_t * i2c)
{
	I2C_FIFO_CONF_t f;
	f.val = i2c->dev->fifo_conf.val;
	f.tx_fifo_rst = 1;
	f.rx_fifo_rst = 1;
	i2c->dev->fifo_conf.val = f.val;
	f.tx_fifo_rst = 0;
	f.rx_fifo_rst = 0;
	i2c->dev->fifo_conf.val = f.val;
/*	
    i2c->dev->fifo_conf.tx_fifo_rst = 1;
    i2c->dev->fifo_conf.tx_fifo_rst = 0;
    i2c->dev->fifo_conf.rx_fifo_rst = 1;
    i2c->dev->fifo_conf.rx_fifo_rst = 0;
*/
}

i2c_err_t i2cWrite(i2c_t * i2c, uint16_t address, bool addr_10bit, uint8_t * data, uint8_t len, bool sendStop)
{
    int i;
    uint8_t index = 0;
    uint8_t dataLen = len + (addr_10bit?2:1);
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
            i2c->dev->fifo_data.data = address & 0xFF;
            dataSend--;
            if(addr_10bit) {
                i2c->dev->fifo_data.data = (address >> 8) & 0xFF;
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
								i2c->dev->int_clr.ack_err = 1; // clear it
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

i2c_err_t i2cRead(i2c_t * i2c, uint16_t address, bool addr_10bit, uint8_t * data, uint8_t len, bool sendStop)
{
    address = (address << 1) | 1;
    uint8_t addrLen = (addr_10bit?2:1);
    uint8_t index = 0;
    uint8_t cmdIdx;
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
    i2cSetCmd(i2c, 0, I2C_CMD_RSTART, 0, false, false, false);

    //CMD WRITE ADDRESS
    i2c->dev->fifo_data.val = address & 0xFF;
    if(addr_10bit) {
        i2c->dev->fifo_data.val = (address >> 8) & 0xFF;
    }
    i2cSetCmd(i2c, 1, I2C_CMD_WRITE, addrLen, false, false, true);

    while(len) {
        cmdIdx = (index)?0:2;
        willRead = (len > 32)?32:(len-1);
        if(cmdIdx){
            i2cResetFiFo(i2c);
        }

        if(willRead){
            i2cSetCmd(i2c, cmdIdx++, I2C_CMD_READ, willRead, false, false, false);
        }

        if((len - willRead) > 1) {
            i2cSetCmd(i2c, cmdIdx++, I2C_CMD_END, 0, false, false, false);
        } else {
            willRead++;
            i2cSetCmd(i2c, cmdIdx++, I2C_CMD_READ, 1, true, false, false);
            if(sendStop) {
                i2cSetCmd(i2c, cmdIdx++, I2C_CMD_STOP, 0, false, false, false);
            }
        }

        //Clear Interrupts
        i2c->dev->int_clr.val = 0xFFFFFFFF;

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
                I2C_MUTEX_UNLOCK();
                return I2C_ERROR_ACK;
            }

            if(i2c->dev->command[cmdIdx-1].done) {
                break;
            }
        }

        int i = 0;
        while(i<willRead) {
            i++;
            data[index++] = i2c->dev->fifo_data.val & 0xFF;
        }
        len -= willRead;
    }
    I2C_MUTEX_UNLOCK();
    return I2C_ERROR_OK;
}

void dumpCmdQueue(i2c_t *i2c){
uint8_t i=0;
while(i<16){
	I2C_COMMAND_t c;
	c.val=i2c->dev->command[i].val;
	
	log_e("[%2d] %c op[%d] val[%d] exp[%d] en[%d] bytes[%d]",i,(c.done?'Y':'N'),
	c.op_code,
	c.ack_val,
	c.ack_exp,
	c.ack_en,
	c.byte_num);
	i++;
	}
}
uint32_t emptyCnt=0;
uint32_t maxRead=0;

void	emptyFifo(i2c_t* i2c,char* data, uint16_t len,uint16_t* index){
uint32_t d;
uint32_t max=0;
while(i2c->dev->status_reg.rx_fifo_cnt>0){
	d = i2c->dev->fifo_data.val;
	data[*index] = (d&0xff);
	if(((*index)+1)<len) (*index)++;
	max++;
	}
if(max>maxRead) maxRead=max;
d = I2C_RXFIFO_FULL_INT_ST;
i2c->dev->int_clr.val =d; // processed!
//log_e("emptied %d",*index);
emptyCnt++;
}

void	fillFifo(i2c_t* i2c,uint8_t* data, uint16_t len,uint16_t* index){
uint32_t d;
while((i2c->dev->status_reg.tx_fifo_cnt<0x1F)&&(*index<len)){
	d = data[*index];
	i2c->dev->fifo_data.val = d;
	*index++;
	}
d = I2C_TXFIFO_EMPTY_INT_ST;
i2c->dev->int_clr.val =d; // processed!
//log_e("Filled %d",*index);
}

typedef enum{
	Stage_waitForBusIdle,
	Stage_init,
	Stage_StartMachine,
	Stage_process,
	Stage_done,
	Stage_abort
	}STAGES;

void fillReadQueue(i2c_t *i2c, uint16_t *neededRead, uint8_t cmdIdx){
/* Can I have another Sir?
 ALL CMD queues must be terminated with either END or STOP.
 if END is used, when refilling it next time, no entries from END to [15] can be used.
 AND END must exit. The END command does not complete until the the 
 ctr->trans_start=1 has executed.
 So, only refill from [0]..[14], leave [15] for a continuation if necessary.
 As a corrilary, once END exists in [15], you do not need to overwrite it for the
 next continuation. It is never modified. But, I update it every time because!
 
*/
uint8_t blkSize=0; // max is 255
//log_e("needed=%2d index=%d",*neededRead,cmdIdx);
while((*neededRead>1)&&((cmdIdx)<15))	{ // more bytes needed and room in cmd queue, leave room for END 
		// lets start with 3 bytes at a time, but may 255 is possible
	blkSize = (*neededRead>35)?35:(*neededRead-1); // each read CMD can only read 32?
	*neededRead -= blkSize; // 
	i2cSetCmd(i2c, (cmdIdx)++, I2C_CMD_READ, blkSize,false,false,false); // read cmd, this can't be the last read.
	}
if((*neededRead==1)&&(cmdIdx==14)){ // stretch because Stop cannot be after or at END spot.
	I2C_COMMAND_t cmdx;
  cmdx.val =i2c->dev->command[cmdIdx-1].val;
  if(cmdx.byte_num>1){
		cmdx.byte_num--;
		i2c->dev->command[cmdIdx-1].val = cmdx.val;
		}
	else { // go back two
		cmdx.val =i2c->dev->command[cmdIdx-2].val;
		cmdx.byte_num--;
		i2c->dev->command[cmdIdx-2].val = cmdx.val;
		}
	cmdx.byte_num=1;
	i2c->dev->command[cmdIdx++].val = cmdx.val;
	// now cmdIdx is 15 and a contuation will be added
	}
	
if(cmdIdx==15){ //need continuation 
// cmd buffer is almost full, Add END as a continuation feature
//					log_e("END at %d, left=%d",cmdIdx,neededRead);
	i2cSetCmd(i2c, (cmdIdx)++,I2C_CMD_END, 0,false,false,false);
	i2c->dev->int_ena.end_detect=1; //maybe?
	i2c->dev->int_clr.end_detect=1; //maybe?
	}

if((*neededRead==1)&&((cmdIdx)<15)){// last Read cmd needs a NAK
	//			log_e("last Read");
	i2cSetCmd(i2c, (cmdIdx)++, I2C_CMD_READ, 1,true,false,false);
				// send NAK to mark end of read
	*neededRead=0;
	}					
if((*neededRead==0)&&((cmdIdx)<16)){// add Stop command
			  // What about SEND_STOP? If no Stop is sent, the Bus Buzy will Error on
				// next call? Need bus ownership?
//				log_e("STOP at %d",cmdIdx);
	i2cSetCmd(i2c, (cmdIdx)++,I2C_CMD_STOP,0,false,false,false);
	}
//			log_e("loaded %d cmds",cmdIdx);
//			dumpCmdQueue(i2c);


}
	
i2c_err_t pollI2cRead(i2c_t * i2c, uint16_t address, bool addr_10bit, uint8_t * data, uint16_t len, bool sendStop)
{
address = (address << 1) | 1;
uint8_t addrLen = (addr_10bit?2:1);
uint16_t index = 0; // pos in data for next byte
uint8_t cmdIdx=0; // next available entry in cmd buffer8
uint16_t neededRead = len; // bytes left
uint32_t intbuf[32];
uint16_t intIndex=0;
for(intIndex=0;intIndex<32;intIndex++){
	intbuf[intIndex]=intIndex;
	}
intIndex = 0;

if(i2c == NULL){
	return I2C_ERROR_DEV;
  }

I2C_MUTEX_LOCK();

emptyCnt = 0;
STAGES stage=Stage_init; // init
bool finished = false;
bool abort = false;
bool started = false;
uint8_t blkSize = 0;
uint32_t startAt = millis(),lastInt=0,activeInt=0;
i2c_err_t reason = I2C_ERROR_OK;
//uint8_t priorMode=i2c->dev->ctr.ms_mode;
while(!finished){
	if((millis()-startAt)>5000){ // timeout
	  if(reason == I2C_ERROR_OK) reason = I2C_ERROR_TIMEOUT;
		// else leave reason alone
	   log_e("Stage[%d] millis(%ld) Timeout(%ld)! Addr: %x",stage,millis(),startAt, address >> 1);
		stage= Stage_abort;
 	}
	switch(stage){
		case Stage_waitForBusIdle : // bus busy on entry
		  // if last call had SEND_STOP=FALSE, this will always Error Out?
			if (!i2c->dev->status_reg.bus_busy){
				stage = Stage_StartMachine;
				reason = I2C_ERROR_OK;
				}
			else reason = I2C_ERROR_BUSY;
			break;

		case Stage_init: // set up address
			i2c->dev->ctr.trans_start=0; // Pause Machine
			i2c->dev->timeout.tout = 0xFFFFF; // max 1M
			I2C_FIFO_CONF_t f;
			f.val = i2c->dev->fifo_conf.val;
			f.rx_fifo_rst = 1; // fifo in reset
			f.tx_fifo_rst = 1; // fifo in reset
			f.nonfifo_en = 0; // use fifo mode
			f.rx_fifo_full_thrhd = 6; // six bytes before INT is issued
			f.fifo_addr_cfg_en = 0; // no directed access
			i2c->dev->fifo_conf.val = f.val; // post them all
			
			f.rx_fifo_rst = 0; // release fifo
			f.tx_fifo_rst = 0;
			i2c->dev->fifo_conf.val = f.val; // post them all
			
			i2c->dev->int_ena.val = 
				I2C_ACK_ERR_INT_ENA | // (BIT(10))
				I2C_TRANS_START_INT_ENA | // (BIT(9))
				I2C_TIME_OUT_INT_ENA  | //(BIT(8))
				I2C_TRANS_COMPLETE_INT_ENA | // (BIT(7))
				I2C_MASTER_TRAN_COMP_INT_ENA | // (BIT(6))
				I2C_ARBITRATION_LOST_INT_ENA | // (BIT(5))
				I2C_SLAVE_TRAN_COMP_INT_ENA  | // (BIT(4))
				I2C_END_DETECT_INT_ENA  | // (BIT(3))
				I2C_RXFIFO_OVF_INT_ENA  | //(BIT(2))
				I2C_RXFIFO_FULL_INT_ENA;  // (BIT(0))
			
			
			i2c->dev->int_clr.val = 0xFFFFFFFF; // kill them All!
			i2c->dev->ctr.ms_mode = 1; // master!
		  //i2cResetFiFo(i2c);
			//i2cResetCmd(i2c);

			//CMD START
			i2cSetCmd(i2c, cmdIdx++, I2C_CMD_RSTART, 0, false, false, false);

    //CMD WRITE ADDRESS
			if(addr_10bit){
				i2c->dev->fifo_data.data = ((address>>7)&0xff)|0x1; // high 2 bits plus read
				i2c->dev->fifo_data.data = (address>>1)&0xff; // low 8 bits of address
				}
			else i2c->dev->fifo_data.data = address & 0xFF; // low 7bits plus read

			i2cSetCmd(i2c, cmdIdx++, I2C_CMD_WRITE, addrLen, false, false, true); //load address in cmdlist, validate (low) ack
		//	log_e("init(%d)",millis());
			fillReadQueue(i2c,&neededRead,cmdIdx);
			stage = Stage_StartMachine;
			break;
		case Stage_StartMachine:
		  if((!started)&& (i2c->dev->status_reg.bus_busy)){ 
			// should not hang on END? because we own the bus!
			  log_e("bus_busy");
				stage = Stage_waitForBusIdle;
				}
			else {
				started = true; 
//				i2c->dev->int_clr.val = 0xFFFFFFFF; // kill them All!
//				log_e("started");
				i2c->dev->ctr.trans_start=1; // go for it
				stage = Stage_process;
				}
			break;
		case Stage_process: // Ok, Get to Work
			activeInt = i2c->dev->int_status.val; // ok lets play!
			if(activeInt==0) break; // nothing to do!
			if(lastInt!=activeInt){
				intIndex++;
				intIndex %= 32;
				intbuf[intIndex] = activeInt;
//				log_e("int=%08X",activeInt);
				lastInt = activeInt;
				}
			else { // same int as last time, probably rx_fifo_full!
				uint32_t x=(intbuf[intIndex]>>20); //12 bits should be enough!
				x++;
				x = x<<20;
				intbuf[intIndex] = x | (intbuf[intIndex]&0x00ffffff);
				}
				
			if(activeInt&I2C_RXFIFO_FULL_INT_ST){// rx has reached thrhd
//				log_e("process");
				emptyFifo(i2c,data,len,&index);
				startAt=millis(); // reset timeout
				}
				
			if(activeInt&I2C_END_DETECT_INT_ST){
				i2c->dev->ctr.trans_start=0;
				cmdIdx=0;
				fillReadQueue(i2c,&neededRead,cmdIdx);
				i2c->dev->int_ena.end_detect = 1;
				i2c->dev->int_clr.end_detect = 1;
				
				stage = Stage_StartMachine;
//				log_e("end");
				}
				
			if(activeInt&I2C_ARBITRATION_LOST_INT_ST){// abort transaction
				// reset State Machine, Release Bus
				stage = Stage_abort;
				reason = I2C_ERROR_BUS;
        log_e("Arbitration Lost Addr: %x", address >> 1);
				}
			else if(activeInt&I2C_TRANS_COMPLETE_INT_ST){// stop
				stage = Stage_done;
				reason = I2C_ERROR_OK;
//				log_e("trans_Complete");
				}
			else if(activeInt&I2C_TIME_OUT_INT_ST){// 
			  stage = Stage_abort;
				reason = I2C_ERROR_TIMEOUT;

        log_e("bit Timeout! int(%lx) stat(%lx) Addr: %x",activeInt,
					i2c->dev->status_reg.val,address >> 1);
				}
			else if(activeInt&I2C_ACK_ERR_INT_ST){
				stage = Stage_abort;
				reason = I2C_ERROR_ACK;
        log_e("Ack Error Addr: %x", address >> 1);
				}
			i2c->dev->int_clr.val = activeInt;
		  break;
		case Stage_done: // quiting Time! More Beer!
//		  log_e("done");
			emptyFifo(i2c,data,len,&index);
		  finished = true;
			i2c->dev->int_clr.val = 0xFFFFFFFF; // kill them All!
			break;
		case Stage_abort : // on the Job accident, Call 911
			emptyFifo(i2c,data,len,&index);
			i2c->dev->int_clr.val = 0xFFFFFFFF; // kill them All!
			finished = true;
			abort = true;
		  log_e("abort len=%d",index);
			break;
		default :
			stage = Stage_abort; // should not happen
			reason = I2C_ERROR_DEV;
      log_e("Unknown Stage! Addr: %x", address >> 1);
		}
	}

//i2c->dev->ctr.trans_start=0; // Pause Machine
i2c->dev->int_ena.val = 0; // disable all interrups
//i2c->dev->ctr.ms_mode=priorMode; // maybe also a slave?
if(reason!=I2C_ERROR_OK) {
	log_e("exit =%d, cnt=%d got %d maxRead%d",reason,emptyCnt,index,maxRead);
	uint16_t a=intIndex;
	a = (a +1)%32;
	while(a!=intIndex){
		if(intbuf[a] != a)log_e("[%02d]=0x%08lx",a,intbuf[a]);
		a++;
		a %=32;
		}
	if(intbuf[a] != a)log_e("[%02d]=0x%08lx",a,intbuf[a]);
	dumpCmdQueue(i2c);
	}
I2C_MUTEX_UNLOCK();

return reason;
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
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG,DPORT_I2C_EXT0_CLK_EN);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG,DPORT_I2C_EXT0_RST);
    } else {
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG,DPORT_I2C_EXT1_CLK_EN);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG,DPORT_I2C_EXT1_RST);
    }
    
    I2C_MUTEX_LOCK();
    i2c->dev->ctr.val = 0;
    i2c->dev->ctr.ms_mode = (slave_addr == 0);
    i2c->dev->ctr.sda_force_out = 1 ;
    i2c->dev->ctr.scl_force_out = 1 ;
    i2c->dev->ctr.clk_en = 1;

    //the max clock number of receiving  a data
    i2c->dev->timeout.tout = 400000;//clocks max=1048575
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

//** 11/2017 Stickbreaker attempt at ISR for I2C hardware
// liberally stolen from ESP_IDF /drivers/i2c.c

esp_err_t i2c_isr_free(intr_handle_t handle){

return esp_intr_free(handle);
}

#define INTBUFFMAX 64
static uint32_t intBuff[INTBUFFMAX][2];
static uint32_t intPos=0;

static void IRAM_ATTR fillCmdQueue(i2c_t * i2c, uint8_t cmdIdx){
/* this function is call on initial i2cProcQueue()
  or when a I2C_END_DETECT_INT occures
*/
  uint16_t qp = i2c->queuePos;
  bool done;
  bool needMoreCmds = false;
  bool ena_rx=false; // if we add a read op, better enable Rx_Fifo
  bool ena_tx=false; // if we add a Write op, better enable TX_Fifo

while(!needMoreCmds&&(qp < i2c->queueCount)){ // check if more possible cmds
  if(i2c->dq[qp].ctrl.stopCmdSent) {
    qp++;
    }
  else needMoreCmds=true;
  }
log_e("needMoreCmds=%d",needMoreCmds);
done=(!needMoreCmds)||(cmdIdx>14);

while(!done){ // fill the command[] until either 0..14 or out of cmds
//CMD START
  I2C_DATA_QUEUE_t *tdq=&i2c->dq[qp]; // simpler

  if(!tdq->ctrl.startCmdSent){
    i2cSetCmd(i2c, cmdIdx++, I2C_CMD_RSTART, 0, false, false, false);
    tdq->ctrl.startCmdSent=1;
    done = (cmdIdx>14);
    }

//CMD WRITE ADDRESS
  if((!done)&&(tdq->ctrl.startCmdSent)){// have to leave room for continue
    if(!tdq->ctrl.addrCmdSent){
      i2cSetCmd(i2c, cmdIdx++, I2C_CMD_WRITE, tdq->ctrl.addrReq, false, false, true); //load address in cmdlist, validate (low) ack
      tdq->ctrl.addrCmdSent=1;
      done =(cmdIdx>14);
      ena_tx=true;
      }
    }

/* Can I have another Sir?
 ALL CMD queues must be terminated with either END or STOP.
 if END is used, when refilling it next time, no entries from END to [15] can be used.
 AND END must exit. The END command does not complete until the the 
 ctr->trans_start=1 has executed.
 So, only refill from [0]..[14], leave [15] for a continuation if necessary.
 As a corrilary, once END exists in [15], you do not need to overwrite it for the
 next continuation. It is never modified. But, I update it every time because!
 
*/
  if((!done)&&(tdq->ctrl.addrCmdSent)){ //room in command[] for at least One data (read/Write) cmd
    uint8_t blkSize=0; // max is 255
    //log_e("needed=%2d index=%d",*neededRead,cmdIdx);
    while(( tdq->cmdBytesNeeded > tdq->ctrl.mode )&&(!done ))	{ // more bytes needed and room in cmd queue, leave room for END 
        // lets start with 3 bytes at a time, but may 255 is possible
      blkSize = (tdq->cmdBytesNeeded > 255)?255:(tdq->cmdBytesNeeded - tdq->ctrl.mode); // Last read cmd needs different ACK setting, so leave 1 byte remainer on reads
      tdq->cmdBytesNeeded -= blkSize; // 
      if(tdq->ctrl.mode==1){
        i2cSetCmd(i2c, (cmdIdx)++, I2C_CMD_READ, blkSize,false,false,false); // read    cmd, this can't be the last read.
        ena_rx=true;
        }
      else {// write
        i2cSetCmd(i2c, cmdIdx++, I2C_CMD_WRITE, blkSize, false, false, true); // check for Nak
        ena_tx=true;
        }        
      done = cmdIdx>14;
      }
      
    if(!done){ // buffer is not filled completely
      if((tdq->ctrl.mode==1)&&(tdq->cmdBytesNeeded==1)){  
        i2cSetCmd(i2c, (cmdIdx)++, I2C_CMD_READ, 1,true,false,false);
            // send NAK to mark end of read
        tdq->cmdBytesNeeded=0;
        done = cmdIdx > 14;
        ena_rx=true;
        }
      tdq->ctrl.dataCmdSent=(tdq->cmdBytesNeeded==0); // enough command[] to send all data
      }
      
    if((!done)&&(tdq->ctrl.dataCmdSent)){ // possibly add stop
      if(tdq->ctrl.stop){ //send a stop
        i2cSetCmd(i2c, (cmdIdx)++,I2C_CMD_STOP,0,false,false,false);
        done = cmdIdx > 14;
        tdq->ctrl.stopCmdSent = 1;
        }
      else {// dummy a stop because this is a restart 
        tdq->ctrl.stopCmdSent = 1;
        }
      }
    }

  if(cmdIdx==15){ //need continuation, even if STOP is in 14, it will not matter
  // cmd buffer is almost full, Add END as a continuation feature
  //					log_e("END at %d, left=%d",cmdIdx,neededRead);
    i2cSetCmd(i2c, (cmdIdx)++,I2C_CMD_END, 0,false,false,false);
    i2c->dev->int_ena.end_detect=1; //maybe?
    i2c->dev->int_clr.end_detect=1; //maybe?
    }
  
  if(!done){
    if(tdq->ctrl.stopCmdSent){ // this queue element has been completely added to command[] buffer
      qp++;
      if(qp < i2c->queueCount){
        tdq = &i2c->dq[qp];
        log_e("inc to next queue=%d",qp);
        }
      else done = true;
      }
    }
      
  }// while(!done)
if(ena_rx) i2c->dev->int_ena.rx_fifo_full = 1;
if(ena_tx) i2c->dev->int_ena.tx_fifo_empty = 1;
}  


static void IRAM_ATTR dumpI2c(i2c_t * i2c){
log_e("i2c=%p",i2c);
log_e("dev=%p",i2c->dev);
log_e("lock=%p",i2c->lock);
log_e("num=%d",i2c->num);
log_e("mode=%d",i2c->mode);
log_e("stage=%d",i2c->stage);
log_e("error=%d",i2c->error);
log_e("event=%p",i2c->i2c_event);
log_e("intr_handle=%p",i2c->intr_handle);
log_e("dq=%p",i2c->dq);
log_e("queueCount=%d",i2c->queueCount);
log_e("queuePos=%d",i2c->queuePos);
log_e("byteCnt=%d",i2c->byteCnt);
uint16_t a=0;
I2C_DATA_QUEUE_t *tdq;
while(a<i2c->queueCount){
  tdq=&i2c->dq[a];
  log_e("[%d] %x %c %s buf@=%p, len=%d, pos=%d",a,tdq->ctrl.addr,(tdq->ctrl.mode)?'R':'W',(tdq->ctrl.stop)?"STOP":"",tdq->data,tdq->length,tdq->position);
  a++;
  }
}


static void IRAM_ATTR fillTxFifo(i2c_t * i2c){
uint16_t a=i2c->queuePos;
bool full=!(i2c->dev->status_reg.tx_fifo_cnt<31);
uint8_t cnt;
while((a < i2c->queueCount)&&(!full)){
  I2C_DATA_QUEUE_t *tdq = &i2c->dq[a];
  cnt=0; 
// add to address to fifo
  if(tdq->ctrl.addrSent < tdq->ctrl.addrReq){ // need to send address bytes
    if(tdq->ctrl.addrReq==2){ //10bit
      if(tdq->ctrl.addrSent==0){ //10bit highbyte needs sent
        if(!full){ // room in fifo
          i2c->dev->fifo_data.val = ((tdq->ctrl.addr>>8)&0xFF);
          cnt++;
          tdq->ctrl.addrSent=1; //10bit highbyte sent
          }     
        }
      full=!(i2c->dev->status_reg.tx_fifo_cnt<31);

      if(tdq->ctrl.addrSent==1){ //10bit Lowbyte needs sent
        if(!full){ // room in fifo
          i2c->dev->fifo_data.val = tdq->ctrl.addr&0xFF;
          cnt++;
          tdq->ctrl.addrSent=2; //10bit lowbyte sent
          }
        }
      }
    else { // 7bit}
      if(tdq->ctrl.addrSent==0){ // 7bit Lowbyte needs sent
        if(!full){ // room in fifo
          i2c->dev->fifo_data.val = tdq->ctrl.addr&0xFF;
          cnt++;
          tdq->ctrl.addrSent=1; // 7bit lowbyte sent
          }
        }
      }
    }
  full=!(i2c->dev->status_reg.tx_fifo_cnt<31);
// add write data to fifo
  if(tdq->ctrl.mode==0){ // write
    if(tdq->ctrl.addrSent == tdq->ctrl.addrReq){ //address has been sent, is Write Mode!
      while((!full)&&(tdq->position < tdq->length)){
        i2c->dev->fifo_data.val = tdq->data[tdq->position++];
        cnt++;
        full=!(i2c->dev->status_reg.tx_fifo_cnt<31);
        }
      }
    }
  cnt += intBuff[intPos][1]>>16;
  intBuff[intPos][1] = (intBuff[intPos][1]&0xFFFF)|(cnt<<16);
  if(!full) a++; // check next buffer for tx
  }

if(a < i2c->queueCount){
  i2c->dev->int_ena.tx_fifo_empty=1;
  }  
else { // no more data available, disable INT
  i2c->dev->int_ena.tx_fifo_empty=0;
  }
i2c->dev->int_clr.tx_fifo_empty=1;
}

static void IRAM_ATTR emptyRxFifo(i2c_t * i2c){
uint32_t d,cnt=0;
I2C_DATA_QUEUE_t *tdq =&i2c->dq[i2c->queuePos];
if(tdq->ctrl.mode==1) { // read
  while(i2c->dev->status_reg.rx_fifo_cnt>0){
    d = i2c->dev->fifo_data.val;
    cnt++;
    if(tdq->position < tdq->length) tdq->data[tdq->position++] = (d&0xFF);
    }
//  d = I2C_RXFIFO_FULL_INT_ST;
//  i2c->dev->int_clr.val =d; // processed!
  cnt += (intBuff[intPos][1])&&0xffFF;
  intBuff[intPos][1] = (intBuff[intPos][1]&0xFFFF0000)|cnt;
  }
else {
  log_e("RxEmpty on TxBuffer? %d",i2c->queuePos);
//  dumpI2c(i2c);
  }
//log_e("emptied %d",*index);
}

static void IRAM_ATTR nextQueue(i2c_t * i2c){
 /* This function is call when I2C_TRANS_START_INT is encountred
 */
 // handle first queue entry correctly
if(i2c->stage != I2C_STARTUP){
  
//empty RXFIFO for current queue, what about First Time?
  emptyRxFifo(i2c);
//roll to next queue entry
  i2c->queuePos++;
  }
// stuff txFifo with Addr
fillTxFifo(i2c); 
}
 
static void IRAM_ATTR i2c_isr_handler_default(void* arg){
//log_e("isr Entry=%p",arg);
i2c_t* p_i2c = (i2c_t*) arg; // recover data
uint32_t activeInt = p_i2c->dev->int_status.val&0x1FFF;
//log_e("int=%x",activeInt);
//dumpI2c(p_i2c);
 
portBASE_TYPE HPTaskAwoken = pdFALSE; 
// be polite if someone more important wakes up.
//log_e("setbits(%p)=%p",p_i2c->i2c_event);
 
xEventGroupSetBitsFromISR(p_i2c->i2c_event, EVENT_IN_IRQ, &HPTaskAwoken);
if (HPTaskAwoken == pdTRUE) {//higher pri task has awoken, jump to it
  portYIELD_FROM_ISR();
  }
//log_e("stage");

if(p_i2c->stage==I2C_DONE){ //get Out
  p_i2c->dev->int_ena.val = 0;
  p_i2c->dev->int_clr.val = 0x1FFF;
  log_e("eject int=%p",activeInt);
  xEventGroupClearBitsFromISR(p_i2c->i2c_event, EVENT_IN_IRQ);
  return;
  }
while (activeInt != 0) { // pending
  if(activeInt==(intBuff[intPos][0]&0x1fff)){
    intBuff[intPos][0] = (((intBuff[intPos][0]>>16)+1)<<16)|activeInt;
    }
  else{
    intPos++;
    intPos %= INTBUFFMAX;
    intBuff[intPos][0]=(1<<16)|activeInt;
    }

  uint32_t oldInt =activeInt;

  if (activeInt & I2C_TRANS_START_INT_ST_M) {
 //   p_i2c->byteCnt=0;
 //   nextQueue(p_i2c);
    if(p_i2c->stage==I2C_STARTUP){
      p_i2c->stage=I2C_RUNNING;
      xEventGroupClearBitsFromISR(p_i2c->i2c_event, EVENT_DONE);
      xEventGroupSetBitsFromISR(p_i2c->i2c_event, EVENT_RUNNING, &HPTaskAwoken);
      if (HPTaskAwoken == pdTRUE) {//higher pri task has awoken, jump to it
        portYIELD_FROM_ISR();
        }
      }
    activeInt &=~I2C_TRANS_START_INT_ST_M;
    p_i2c->dev->int_ena.trans_start = 1;
    p_i2c->dev->int_clr.trans_start = 1;
    } 
    
  if (activeInt & I2C_TXFIFO_EMPTY_INT_ST) {//should this be before Trans_start?
    fillTxFifo(p_i2c);
    activeInt&=~I2C_TXFIFO_EMPTY_INT_ST;
    }
  
  if(activeInt & I2C_RXFIFO_FULL_INT_ST){
    emptyRxFifo(p_i2c);
    p_i2c->dev->int_clr.rx_fifo_full=1;
    p_i2c->dev->int_ena.rx_fifo_full=1;
 
    activeInt &=~I2C_RXFIFO_FULL_INT_ST;
    }
  
  if(activeInt & I2C_MASTER_TRAN_COMP_INT_ST){ // each byte the master sends/recv
    p_i2c->dev->int_clr.master_tran_comp = 1;
    p_i2c->byteCnt++;
    if(p_i2c->byteCnt > p_i2c->dq[p_i2c->queuePos].queueLength){// simulate Trans_start
      p_i2c->byteCnt -= p_i2c->dq[p_i2c->queuePos].queueLength;
      if(p_i2c->dq[p_i2c->queuePos].ctrl.mode==1){
        emptyRxFifo(p_i2c);
        p_i2c->dev->int_clr.rx_fifo_full=1;
        p_i2c->dev->int_ena.rx_fifo_full=1;
        }
      p_i2c->queuePos++;
      p_i2c->dev->int_ena.tx_fifo_empty=1;
      
//      nextQueue(p_i2c);
      
      }
    activeInt &=~I2C_MASTER_TRAN_COMP_INT_ST;
    }
  
  if (activeInt & I2C_ACK_ERR_INT_ST_M) {
    p_i2c->dev->int_clr.ack_err = 1;
    if (p_i2c->mode == I2C_MASTER) {
			p_i2c->error = I2C_ERROR;
      log_e("AcK Err byteCnt=%d, queuepos=%d",p_i2c->byteCnt,p_i2c->queuePos);
      if(p_i2c->byteCnt<2) p_i2c->error = I2C_ADDR_ACK;
			else if((p_i2c->byteCnt == 2) && (p_i2c->dq[p_i2c->queuePos].ctrl.addrReq == 2)) p_i2c->error = I2C_ADDR_ACK;
      else p_i2c->error = I2C_DATA_ACK;
      }
    p_i2c->stage = I2C_DONE;
    p_i2c->dev->int_ena.val = 0; // kill them all
    p_i2c->dev->int_clr.val = 0xFFFFFF;
    xEventGroupClearBitsFromISR(p_i2c->i2c_event, EVENT_RUNNING|EVENT_IN_IRQ);
    xEventGroupSetBitsFromISR(p_i2c->i2c_event, EVENT_DONE, &HPTaskAwoken);
    if (HPTaskAwoken == pdTRUE) {//higher pri task has awoken, jump to it
      portYIELD_FROM_ISR();
      }
    return;
    activeInt &=~I2C_ACK_ERR_INT_ST_M;
    }
	
  if (activeInt & I2C_TIME_OUT_INT_ST_M) {//death Happens Here
    p_i2c->dev->int_clr.time_out = 1;
		p_i2c->error = I2C_TIMEOUT;
    p_i2c->stage = I2C_DONE;
    p_i2c->dev->int_ena.val = 0; // kill them all
    p_i2c->dev->int_clr.val = 0xFFFFFF;
    xEventGroupClearBitsFromISR(p_i2c->i2c_event, EVENT_RUNNING|EVENT_IN_IRQ);
    xEventGroupSetBitsFromISR(p_i2c->i2c_event, EVENT_DONE, &HPTaskAwoken);
    if (HPTaskAwoken == pdTRUE) {//higher pri task has awoken, jump to it
      portYIELD_FROM_ISR();
      }
    return;
    activeInt &=~I2C_TIME_OUT_INT_ST_M;
    } 
	
  if (activeInt & I2C_TRANS_COMPLETE_INT_ST_M) { 
    if(p_i2c->dq[p_i2c->queuePos].ctrl.mode == 1) emptyRxFifo(p_i2c); // grab last few characters
    p_i2c->stage = I2C_DONE;
    p_i2c->dev->int_ena.val = 0; // shutdown interrupts
    p_i2c->dev->int_clr.val = 0xFFFFFF;
    xEventGroupClearBitsFromISR(p_i2c->i2c_event, EVENT_RUNNING);
    xEventGroupSetBitsFromISR(p_i2c->i2c_event, EVENT_DONE, &HPTaskAwoken);
    if (HPTaskAwoken == pdTRUE) {//higher pri task has awoken, jump to it
      portYIELD_FROM_ISR();
      }
    return; // no more work to do
/*    
		// how does a restart appear?
    // how does slave mode act?
    
    if (p_i2c->mode == I2C_SLAVE) { // STOP detected
		// empty fifo
		// dispatch callback
*/
    activeInt &=~I2C_TRANS_COMPLETE_INT_ST_M;
    } 
  
  if (activeInt & I2C_ARBITRATION_LOST_INT_ST_M) {
    p_i2c->dev->int_clr.arbitration_lost = 1;
    p_i2c->error = I2C_ARBITRATION;
    p_i2c->stage = I2C_DONE;
    p_i2c->dev->int_ena.val = 0; // shutdown interrupts
    p_i2c->dev->int_clr.val = 0xFFFFFF;
    xEventGroupClearBitsFromISR(p_i2c->i2c_event, EVENT_RUNNING);
    xEventGroupSetBitsFromISR(p_i2c->i2c_event, EVENT_DONE, &HPTaskAwoken);
    if (HPTaskAwoken == pdTRUE) {//higher pri task has awoken, jump to it
      portYIELD_FROM_ISR();
      }
    return; // no more work to do
    activeInt &=~I2C_ARBITRATION_LOST_INT_ST_M;
    } 
	
  if (activeInt & I2C_SLAVE_TRAN_COMP_INT_ST_M) {
    p_i2c->dev->int_clr.slave_tran_comp = 1;
  
    }
  
  if (activeInt & I2C_END_DETECT_INT_ST_M) {
    xEventGroupSetBitsFromISR(p_i2c->i2c_event, EVENT_IN_END, &HPTaskAwoken);
    if (HPTaskAwoken == pdTRUE) {//higher pri task has awoken, jump to it
      portYIELD_FROM_ISR();
      }
    p_i2c->dev->int_ena.end_detect = 0;
    p_i2c->dev->int_clr.end_detect = 1;
		p_i2c->dev->ctr.trans_start=0;
    fillCmdQueue(p_i2c,0);
    p_i2c->dev->ctr.trans_start=1; // go for it
    activeInt&=~I2C_END_DETECT_INT_ST_M;
    // What about Tx, RX fifo fill?
    xEventGroupClearBitsFromISR(p_i2c->i2c_event, EVENT_IN_END);
    }
  
  if (activeInt & I2C_RXFIFO_OVF_INT_ST_M) {
    p_i2c->dev->int_clr.rx_fifo_ovf = 1;
    } 
	
  if(activeInt){ // clear unhandled if possible?  What about Disableing interrupt?
    p_i2c->dev->int_clr.val = activeInt;
    log_e("unknown int=%x",activeInt);
    }

  activeInt = p_i2c->dev->int_status.val; // start all over
 /* if((activeInt&oldInt)==oldInt){
    log_e("dup int old=%p, new=%p dup=%p",oldInt,activeInt,(oldInt&activeInt));
    }
*/
  }

xEventGroupClearBitsFromISR(p_i2c->i2c_event, EVENT_IN_IRQ);
// and we're out!  
}
 
i2c_err_t i2cProcQueue(i2c_t * i2c){
/* do the hard stuff here
  install ISR if necessary
  setup EventGroup
  handle bus busy?
  do I load command[] or just pass that off to the ISR
*/
log_e("procQueue i2c=%p",&i2c);

if(i2c == NULL){
	return I2C_ERROR_DEV;
  }

I2C_MUTEX_LOCK();
i2c->stage = I2C_DONE; // until ready

for(intPos=0;intPos<INTBUFFMAX;intPos++){
  intBuff[intPos][0]=0;
  intBuff[intPos][1]=0;
  }
intPos=0;
  
if(!i2c->i2c_event){
  log_e("create Event");
  i2c->i2c_event = xEventGroupCreate();
  log_e("event=%p",i2c->i2c_event);
  }
if(i2c->i2c_event) {
  uint32_t ret=xEventGroupClearBits(i2c->i2c_event, EVENT_IN_IRQ|EVENT_RUNNING|EVENT_DONE);
  log_e("after clearBits(%p)=%p",i2c->i2c_event,ret);
  }
uint32_t startAt = millis();

i2c_err_t reason = I2C_ERROR_OK;
i2c->mode = I2C_MASTER;

i2c->dev->ctr.trans_start=0; // Pause Machine
i2c->dev->timeout.tout = 0xFFFFF; // max 1M
I2C_FIFO_CONF_t f;
f.val = i2c->dev->fifo_conf.val;
f.rx_fifo_rst = 1; // fifo in reset
f.tx_fifo_rst = 1; // fifo in reset
f.nonfifo_en = 0; // use fifo mode
f.rx_fifo_full_thrhd = 30; // six bytes before INT is issued
f.fifo_addr_cfg_en = 0; // no directed access
i2c->dev->fifo_conf.val = f.val; // post them all

f.rx_fifo_rst = 0; // release fifo
f.tx_fifo_rst = 0;
i2c->dev->fifo_conf.val = f.val; // post them all

i2c->dev->int_clr.val = 0xFFFFFFFF; // kill them All!
i2c->dev->ctr.ms_mode = 1; // master!
i2c->queuePos=0;
i2c->byteCnt=0;
// convert address field to required I2C format
while(i2c->queuePos<i2c->queueCount){
  I2C_DATA_QUEUE_t *tdq = &i2c->dq[i2c->queuePos++];
  uint16_t taddr=0;
  if(tdq->ctrl.addrReq ==2){ // 10bit address
    taddr =((tdq->ctrl.addr>>7)&0xFE)
      |tdq->ctrl.mode;
    taddr = (taddr <<8)||(tdq->ctrl.addr&0xFF);
    }
  else { // 7bit address
    taddr =  ((tdq->ctrl.addr<<1)&0xFE)
      |tdq->ctrl.mode;
    }
  tdq->ctrl.addr = taddr; // all fixed with R/W bit
  }
i2c->queuePos=0;

fillCmdQueue(i2c,0); // start adding command[], END irq will keep it full

//fillTxFifo(i2c); // I2C_TRANS_START_INT will do this

i2c->stage = I2C_STARTUP; // 

i2c->dev->int_ena.val = 
  I2C_ACK_ERR_INT_ENA | // (BIT(10))
  I2C_TRANS_START_INT_ENA | // (BIT(9))
  I2C_TIME_OUT_INT_ENA  | //(BIT(8))
  I2C_TRANS_COMPLETE_INT_ENA | // (BIT(7))
  I2C_MASTER_TRAN_COMP_INT_ENA | // (BIT(6))
  I2C_ARBITRATION_LOST_INT_ENA | // (BIT(5))
  I2C_SLAVE_TRAN_COMP_INT_ENA  | // (BIT(4))
  I2C_END_DETECT_INT_ENA  | // (BIT(3))
  I2C_RXFIFO_OVF_INT_ENA  | //(BIT(2))
  I2C_TXFIFO_EMPTY_INT_ENA | // (BIT(1))
  I2C_RXFIFO_FULL_INT_ENA;  // (BIT(0))


dumpI2c(i2c);
dumpCmdQueue(i2c);  
if(!i2c->intr_handle){ // create ISR
//  log_e("create ISR");
  uint32_t ret = esp_intr_alloc(ETS_I2C_EXT0_INTR_SOURCE, 0, &i2c_isr_handler_default, i2c, &i2c->intr_handle);
  if(ret!=ESP_OK){
    log_e("install interrupt=%d",ret);
    }
  }
  
log_e("before startup");
i2c->dev->ctr.trans_start=1; // go for it
uint32_t eBits = xEventGroupWaitBits(i2c->i2c_event,EVENT_DONE,pdTRUE,pdTRUE,0xFF);
log_e("after Wait");
dumpI2c(i2c);
uint32_t b;
for(uint32_t a=1;a<=INTBUFFMAX;a++){
  b=(a+intPos)%INTBUFFMAX;
  if(intBuff[b][0]!=0) log_e("[%02d] 0x%08x 0x%08x",b,intBuff[b][0],intBuff[b][1]);
  }
  
if(eBits&EVENT_DONE){ // no gross timeout
  reason = i2c->error; // not correct
  }
else { // GROSS timeout, shutdown ISR , report Timeout
  log_e(" Gross Timeout Dead");
  }
I2C_MUTEX_UNLOCK();

return reason;
}

