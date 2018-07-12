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
#include "esp_attr.h"

//#define I2C_DEV(i)   (volatile i2c_dev_t *)((i)?DR_REG_I2C1_EXT_BASE:DR_REG_I2C_EXT_BASE)
//#define I2C_DEV(i)   ((i2c_dev_t *)(REG_I2C_BASE(i)))
#define I2C_SCL_IDX(p)  ((p==0)?I2CEXT0_SCL_OUT_IDX:((p==1)?I2CEXT1_SCL_OUT_IDX:0))
#define I2C_SDA_IDX(p) ((p==0)?I2CEXT0_SDA_OUT_IDX:((p==1)?I2CEXT1_SDA_OUT_IDX:0))

#define DR_REG_I2C_EXT_BASE_FIXED               0x60013000
#define DR_REG_I2C1_EXT_BASE_FIXED              0x60027000

// start from tools/sdk/include/soc/soc/i2c_struct.h

typedef union {
    struct {
        uint32_t byte_num:      8;              /*Byte_num represent the number of data need to be send or data need to be received.*/
        uint32_t ack_en:        1;              /*ack_check_en  ack_exp and ack value are used to control  the ack bit.*/
        uint32_t ack_exp:       1;              /*ack_check_en  ack_exp and ack value are used to control  the ack bit.*/
        uint32_t ack_val:       1;              /*ack_check_en  ack_exp and ack value are used to control  the ack bit.*/
        uint32_t op_code:       3;              /*op_code is the command  0：RSTART   1：WRITE  2：READ  3：STOP . 4:END.*/
        uint32_t reserved14:   17;
        uint32_t done:  1;                      /*When command0 is done in I2C Master mode  this bit changes to high level.*/
    };
    uint32_t val;
} I2C_COMMAND_t;

typedef union {
    struct {
        uint32_t rx_fifo_full_thrhd: 5;
        uint32_t tx_fifo_empty_thrhd:5;         //Config tx_fifo empty threhd value when using apb fifo access * /
        uint32_t nonfifo_en:         1;         //Set this bit to enble apb nonfifo access. * /
        uint32_t fifo_addr_cfg_en:   1;         //When this bit is set to 1 then the byte after address represent the offset address of I2C Slave's ram. * /
        uint32_t rx_fifo_rst:        1;         //Set this bit to reset rx fifo when using apb fifo access. * /
        // chuck while this bit is 1, the RX fifo is held in REST, Toggle it * /
        uint32_t tx_fifo_rst:        1;         //Set this bit to reset tx fifo when using apb fifo access. * /
        // chuck while this bit is 1, the TX fifo is held in REST, Toggle it * /
        uint32_t nonfifo_rx_thres:   6;         //when I2C receives more than nonfifo_rx_thres data  it will produce rx_send_full_int_raw interrupt and update the current offset address of the receiving data.* /
        uint32_t nonfifo_tx_thres:   6;         //when I2C sends more than nonfifo_tx_thres data  it will produce tx_send_empty_int_raw interrupt and update the current offset address of the sending data. * /
        uint32_t reserved26:         6;
    };
    uint32_t val;
} I2C_FIFO_CONF_t;

// end from tools/sdk/include/soc/soc/i2c_struct.h

// sync between dispatch(i2cProcQueue) and worker(i2c_isr_handler_default)
typedef enum {
    //I2C_NONE=0,
    I2C_STARTUP=1,
    I2C_RUNNING,
    I2C_DONE
} I2C_STAGE_t;

typedef enum {
    I2C_NONE=0,
    I2C_MASTER,
    I2C_SLAVE,
    I2C_MASTERSLAVE
} I2C_MODE_t;

// internal Error condition
typedef enum {
    //  I2C_NONE=0,
    I2C_OK=1,
    I2C_ERROR,
    I2C_ADDR_NAK,
    I2C_DATA_NAK,
    I2C_ARBITRATION,
    I2C_TIMEOUT
} I2C_ERROR_t;

// i2c_event bits for EVENTGROUP bits
// needed to minimize change events, FreeRTOS Daemon overload, so ISR will only set values
// on Exit.  Dispatcher will set bits for each dq before/after ISR completion
#define EVENT_ERROR_NAK (BIT(0))
#define EVENT_ERROR     (BIT(1))
#define EVENT_ERROR_BUS_BUSY  (BIT(2))
#define EVENT_RUNNING   (BIT(3))
#define EVENT_DONE      (BIT(4))
#define EVENT_IN_END    (BIT(5))
#define EVENT_ERROR_PREV  (BIT(6))
#define EVENT_ERROR_TIMEOUT   (BIT(7))
#define EVENT_ERROR_ARBITRATION (BIT(8))
#define EVENT_ERROR_DATA_NAK  (BIT(9))
#define EVENT_MASK 0x3F

// control record for each dq entry
typedef union {
    struct {
        uint32_t  addr: 16; // I2C address, if 10bit must have 0x7800 mask applied, else 8bit
        uint32_t  mode:         1; // transaction direction 0 write, 1 read
        uint32_t  stop:         1; // sendStop 0 no, 1 yes
        uint32_t  startCmdSent: 1; // START cmd has been added to command[]
        uint32_t  addrCmdSent:  1; // addr WRITE cmd has been added to command[]
        uint32_t  dataCmdSent:  1; // all necessary DATA(READ/WRITE) cmds added to command[]
        uint32_t  stopCmdSent:  1; // completed all necessary commands
        uint32_t  addrReq:      2; // number of addr bytes need to send address
        uint32_t  addrSent:     2; // number of addr bytes added to FIFO
        uint32_t  reserved_31:  6;
    };
    uint32_t val;
} I2C_DATA_CTRL_t;

// individual dq element
typedef struct {
    uint8_t *data;           // datapointer for read/write buffer
    uint16_t length;         // size of data buffer
    uint16_t position;       // current position for next char in buffer (<length)
    uint16_t cmdBytesNeeded; // used to control number of I2C_COMMAND_t blocks added to queu
    uint16_t queueLength;    // number of data bytes needing moved, used to control
    // current queuePos for fifo fills
    I2C_DATA_CTRL_t ctrl;
    EventGroupHandle_t queueEvent;  // optional user supplied for Async feedback EventBits
} I2C_DATA_QUEUE_t;

struct i2c_struct_t {
    i2c_dev_t * dev;
#if !CONFIG_DISABLE_HAL_LOCKS
    xSemaphoreHandle lock;
#endif
    uint8_t num;
    int8_t sda;
    int8_t scl;
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
    uint32_t exitCode;
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
    {(volatile i2c_dev_t *)(DR_REG_I2C_EXT_BASE_FIXED), 0, -1, -1,I2C_NONE,I2C_NONE,I2C_ERROR_OK,NULL,NULL,NULL,0,0,0,0},
    {(volatile i2c_dev_t *)(DR_REG_I2C1_EXT_BASE_FIXED), 1, -1, -1,I2C_NONE,I2C_NONE,I2C_ERROR_OK,NULL,NULL,NULL,0,0,0,0}
};
#else
#define I2C_MUTEX_LOCK()    do {} while (xSemaphoreTake(i2c->lock, portMAX_DELAY) != pdPASS)
#define I2C_MUTEX_UNLOCK()  xSemaphoreGive(i2c->lock)

static i2c_t _i2c_bus_array[2] = {
    {(volatile i2c_dev_t *)(DR_REG_I2C_EXT_BASE_FIXED), NULL, 0, -1, -1, I2C_NONE,I2C_NONE,I2C_ERROR_OK,NULL,NULL,NULL,0,0,0,0},
    {(volatile i2c_dev_t *)(DR_REG_I2C1_EXT_BASE_FIXED), NULL, 1, -1, -1,I2C_NONE,I2C_NONE,I2C_ERROR_OK,NULL,NULL,NULL,0,0,0,0}
};
#endif

/*
 * index     - command index (0 to 15)
 * op_code   - is the command
 * byte_num  - This register is to store the amounts of data that is read and written. byte_num in RSTART, STOP, END is null.
 * ack_val   - Each data byte is terminated by an ACK bit used to set the bit level.
 * ack_exp   - This bit is to set an expected ACK value for the transmitter.
 * ack_check - This bit is to decide whether the transmitter checks ACK bit. 1 means yes and 0 means no.
 * */
static void IRAM_ATTR i2cSetCmd(i2c_t * i2c, uint8_t index, uint8_t op_code, uint8_t byte_num, bool ack_val, bool ack_exp, bool ack_check)
{
    I2C_COMMAND_t cmd;
    cmd.val=0;
    cmd.ack_en = ack_check;
    cmd.ack_exp = ack_exp;
    cmd.ack_val = ack_val;
    cmd.byte_num = byte_num;
    cmd.op_code = op_code;
    i2c->dev->command[index].val = cmd.val;
}

/* Stickbreaker ISR mode debug support
 */
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
#define INTBUFFMAX 64
static uint32_t intBuff[INTBUFFMAX][3][2];
static uint32_t intPos[2]= {0,0};
#endif

/* Stickbreaker ISR mode debug support
 */
void IRAM_ATTR dumpCmdQueue(i2c_t *i2c)
{
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_ERROR
    uint8_t i=0;
    while(i<16) {
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
#endif
}

/* Stickbreaker ISR mode support
 */
static void IRAM_ATTR fillCmdQueue(i2c_t * i2c, bool INTS)
{
    /* this function is call on initial i2cProcQueue()
    or when a I2C_END_DETECT_INT occures
     */
    uint16_t cmdIdx = 0;
    uint16_t qp = i2c->queuePos;
    bool done;
    bool needMoreCmds = false;
    bool ena_rx=false; // if we add a read op, better enable Rx_Fifo IRQ
    bool ena_tx=false; // if we add a Write op, better enable TX_Fifo IRQ

    while(!needMoreCmds&&(qp < i2c->queueCount)) { // check if more possible cmds
        if(i2c->dq[qp].ctrl.stopCmdSent) {
            qp++;
        } else {
            needMoreCmds=true;
        }
    }
    //log_e("needMoreCmds=%d",needMoreCmds);
    done=(!needMoreCmds)||(cmdIdx>14);

    while(!done) { // fill the command[] until either 0..14 filled or out of cmds and last cmd is STOP
        //CMD START
        I2C_DATA_QUEUE_t *tdq=&i2c->dq[qp]; // simpler coding

        if((!tdq->ctrl.startCmdSent) && (cmdIdx < 14)) { // has this dq element's START command been added?
            // <14 testing if ReSTART END is causeing the Timeout
            i2cSetCmd(i2c, cmdIdx++, I2C_CMD_RSTART, 0, false, false, false);
            tdq->ctrl.startCmdSent=1;
            done = (cmdIdx>14);
        }

        //CMD WRITE ADDRESS
        if((!done)&&(tdq->ctrl.startCmdSent)) { // have to leave room for continue, and START must have been sent!
            if(!tdq->ctrl.addrCmdSent) {
                i2cSetCmd(i2c, cmdIdx++, I2C_CMD_WRITE, tdq->ctrl.addrReq, false, false, true); //load address in cmdlist, validate (low) ack
                tdq->ctrl.addrCmdSent=1;
                done =(cmdIdx>14);
                ena_tx=true; // tx Data necessary
            }
        }

        /* Can I have another Sir?
        ALL CMD queues must be terminated with either END or STOP.

        If END is used, when refilling the cmd[] next time, no entries from END to [15] can be used.
        AND the cmd[] must be filled starting at [0] with commands. Either fill all 15 [0]..[14] and leave the
        END in [15] or include a STOP in one of the positions [0]..[14].  Any entries after a STOP are IGNORED byte the StateMachine.
        The END operation does not complete until ctr->trans_start=1 has been issued.

        So, only refill from [0]..[14], leave [15] for a continuation if necessary.
        As a corrilary, once END exists in [15], you do not need to overwrite it for the
        next continuation. It is never modified. But, I update it every time because it might
        actually be the first time!

        23NOV17  START cannot proceed END.  if START is in[14], END cannot be in [15].
        so, AND if END is moved to [14], [14] and [15] can nolonger be use for anything other than END.
        If a START is found in [14] then a prior READ or WRITE must be expanded so that there is no START element in [14].


         */
        if((!done)&&(tdq->ctrl.addrCmdSent)) { //room in command[] for at least One data (read/Write) cmd
            uint8_t blkSize=0; // max is 255? does numBytes =0 actually mean 256? haven't tried it.
            //log_e("needed=%2d index=%d",*neededRead,cmdIdx);
            while(( tdq->cmdBytesNeeded > tdq->ctrl.mode )&&(!done )) { // more bytes needed and room in cmd queue, leave room for END
                blkSize = (tdq->cmdBytesNeeded > 255)?255:(tdq->cmdBytesNeeded - tdq->ctrl.mode); // Last read cmd needs different ACK setting, so leave 1 byte remainer on reads
                tdq->cmdBytesNeeded -= blkSize; //
                if(tdq->ctrl.mode==1) { //read mode
                    i2cSetCmd(i2c, (cmdIdx)++, I2C_CMD_READ, blkSize,false,false,false); // read    cmd, this can't be the last read.
                    ena_rx=true; // need to enable rxFifo IRQ
                } else { // write
                    i2cSetCmd(i2c, cmdIdx++, I2C_CMD_WRITE, blkSize, false, false, true); // check for Nak
                    ena_tx=true; // need to enable txFifo IRQ
                }
                done = cmdIdx>14; //have to leave room for END
            }

            if(!done) { // buffer is not filled completely
                if((tdq->ctrl.mode==1)&&(tdq->cmdBytesNeeded==1)) { //special last read byte NAK
                    i2cSetCmd(i2c, (cmdIdx)++, I2C_CMD_READ, 1,true,false,false);
                    // send NAK to mark end of read
                    tdq->cmdBytesNeeded=0;
                    done = cmdIdx > 14;
                    ena_rx=true;
                }
            }

            tdq->ctrl.dataCmdSent=(tdq->cmdBytesNeeded==0); // enough command[] to send all data

            if((!done)&&(tdq->ctrl.dataCmdSent)) { // possibly add stop
                if(tdq->ctrl.stop) { //send a stop
                    i2cSetCmd(i2c, (cmdIdx)++,I2C_CMD_STOP,0,false,false,false);
                    done = cmdIdx > 14;
                    tdq->ctrl.stopCmdSent = 1;
                } else { // dummy a stop because this is a restart
                    tdq->ctrl.stopCmdSent = 1;
                }
            }
        }

        if((cmdIdx==14)&&(!tdq->ctrl.startCmdSent)) {
            // START would have preceded END, causes SM TIMEOUT
            // need to stretch out a prior WRITE or READ to two Command[] elements
            done = false; // reuse it
            uint16_t i = 13; // start working back until a READ/WRITE has >1 numBytes
            cmdIdx =15;
            //   log_e("before Stretch");
            //   dumpCmdQueue(i2c);
            while(!done) {
                i2c->dev->command[i+1].val = i2c->dev->command[i].val; // push it down
                if (((i2c->dev->command[i].op_code == 1)||(i2c->dev->command[i].op_code==2))) {
                    /* just try a num_bytes =0;
                       &&(i2c->dev->command[i].byte_num>1)){ // found the one to expand
                        i2c->dev->command[i+1].byte_num =1;
                        // the -= in the following statment caused unintential consequences.
                        //  The op_code field value changed from 2 to 4, so the manual cludge was needed
                        //      i2c->dev->command[i].byte_num -= 1;
                        uint32_t temp = i2c->dev->command[i].val;
                        temp =  (temp&0xFFFFFF00) | ((temp & 0xFF)-1);
                        i2c->dev->command[i].val = temp;
                     */
                    i2c->dev->command[i].byte_num = 0;
                    done = true;

                } else {
                    if(i > 0) {
                        i--;
                    } else { // unable to stretch, fatal
                        log_e("invalid CMD[] layout Stretch Failed");
                        dumpCmdQueue(i2c);
                        done = true;
                    }
                }
            }
            //   log_e("after Stretch");
            //   dumpCmdQueue(i2c);

        }


        if(cmdIdx==15) { //need continuation, even if STOP is in 14, it will not matter
            // cmd buffer is almost full, Add END as a continuation feature
            //          log_e("END at %d, left=%d",cmdIdx,neededRead);
            i2cSetCmd(i2c, (cmdIdx)++,I2C_CMD_END, 0,false,false,false);
            i2c->dev->int_ena.end_detect=1; //maybe?
            i2c->dev->int_clr.end_detect=1; //maybe?
            done = true;
        }

        if(!done) {
            if(tdq->ctrl.stopCmdSent) { // this queue element has been completely added to command[] buffer
                qp++;
                if(qp < i2c->queueCount) {
                    tdq = &i2c->dq[qp];
                    //        log_e("inc to next queue=%d",qp);
                } else {
                    done = true;
                }
            }
        }

    }// while(!done)
    if(INTS) { // don't want to prematurely enable fifo ints until ISR is ready to handle it.
        if(ena_rx) {
            i2c->dev->int_ena.rx_fifo_full = 1;
        }
        if(ena_tx) {
            i2c->dev->int_ena.tx_fifo_empty = 1;
        }
    }
}

/* Stickbreaker ISR mode support
 */
static void IRAM_ATTR fillTxFifo(i2c_t * i2c)
{
    /* need to test overlapping RX->TX fifo operations,
    Currently, this function attempts to queue all possible tx elements into the Fifo.
    What happens when WRITE 10, READ 20, Write 10?
    (Write Addr, Write 10),(Write addr, Read 20) (Write addr, Write 10).
    I know everything will work up to the End of the Read 20, but I am unsure
    what will happen to the third command, will the Read 20 overwrite the previously
    queued (write addr, write 10) of the Third command?  I need to test!
     */
    /*11/15/2017 will assume that I cannot queue tx after a READ until READ completes
    11/23/2017 Seems to be a TX fifo problem, the SM sends 0x40 for last rxbyte, I
    enable txEmpty, filltx fires, but the SM has already sent a bogus byte out the BUS.
    I am going so see if I can overlap Tx/Rx/Tx in the fifo
    12/01/2017 The Fifo's are independent, 32 bytes of tx and 32 bytes of Rx.
    overlap is not an issue, just keep them full/empty the status_reg.xx_fifo_cnt
    tells the truth. And the INT's fire correctly
     */
    uint16_t a=i2c->queuePos; // currently executing dq,
    bool full=!(i2c->dev->status_reg.tx_fifo_cnt<31);
    uint8_t cnt;
    while((a < i2c->queueCount) && !full) {
        I2C_DATA_QUEUE_t *tdq = &i2c->dq[a];
        cnt=0;
        // add to address to fifo ctrl.addr already has R/W bit positioned correctly
        if(tdq->ctrl.addrSent < tdq->ctrl.addrReq) { // need to send address bytes
            if(tdq->ctrl.addrReq==2) { //10bit
                if(tdq->ctrl.addrSent==0) { //10bit highbyte needs sent
                    if(!full) { // room in fifo
                        i2c->dev->fifo_data.val = ((tdq->ctrl.addr>>8)&0xFF);
                        cnt++;
                        tdq->ctrl.addrSent=1; //10bit highbyte sent
                    }
                }
                full=!(i2c->dev->status_reg.tx_fifo_cnt<31);

                if(tdq->ctrl.addrSent==1) { //10bit Lowbyte needs sent
                    if(!full) { // room in fifo
                        i2c->dev->fifo_data.val = tdq->ctrl.addr&0xFF;
                        cnt++;
                        tdq->ctrl.addrSent=2; //10bit lowbyte sent
                    }
                }
            } else { // 7bit}
                if(tdq->ctrl.addrSent==0) { // 7bit Lowbyte needs sent
                    if(!full) { // room in fifo
                        i2c->dev->fifo_data.val = tdq->ctrl.addr&0xFF;
                        cnt++;
                        tdq->ctrl.addrSent=1; // 7bit lowbyte sent
                    }
                }
            }
        }
        full=!(i2c->dev->status_reg.tx_fifo_cnt<31);
        // add write data to fifo
        //21NOV2017 might want to look into using local capacity counter instead of reading status_reg
        //  a double while loop, like emptyRxFifo()
        if(tdq->ctrl.mode==0) { // write
            if(tdq->ctrl.addrSent == tdq->ctrl.addrReq) { //address has been sent, is Write Mode!
                while((!full)&&(tdq->position < tdq->length)) {
                    i2c->dev->fifo_data.val = tdq->data[tdq->position++];
                    cnt++;
                    full=!(i2c->dev->status_reg.tx_fifo_cnt<31);
                }
            }
        }

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO

        // update debug buffer tx counts
        cnt += intBuff[intPos[i2c->num]][1][i2c->num]>>16;
        intBuff[intPos[i2c->num]][1][i2c->num] = (intBuff[intPos[i2c->num]][1][i2c->num]&0xFFFF)|(cnt<<16);

#endif

        if(!full) {
            a++;    // check next buffer for tx
        }
    }

    if(!full || (a >= i2c->queueCount)) { // disable IRQ, the next dq will re-enable it
        i2c->dev->int_ena.tx_fifo_empty=0;
        
        if(!full) { // add a byte to keep spurious tx_empty int
          uint8_t filler=i2c->dev->fifo_conf.tx_fifo_empty_thrhd;
          if(filler >( 31 - i2c->dev->status_reg.tx_fifo_cnt)){
            filler = ( 31 - i2c->dev->status_reg.tx_fifo_cnt);
          }
          
          while(filler > 0){
            i2c->dev->fifo_data.val = 0xFE; // Just a dummy byte
            filler--;
          }
        }
    }

    i2c->dev->int_clr.tx_fifo_empty=1;
}

/* Stickbreaker ISR mode support
 */
static void IRAM_ATTR emptyRxFifo(i2c_t * i2c)
{
    uint32_t d, cnt=0, moveCnt;
    I2C_DATA_QUEUE_t *tdq =&i2c->dq[i2c->queuePos];

    moveCnt = i2c->dev->status_reg.rx_fifo_cnt;//no need to check the reg until this many are read
    if(moveCnt > (tdq->length - tdq->position)) { //makesure they go in this dq
        // part of these reads go into the next dq
        moveCnt = (tdq->length - tdq->position);
    }

    if(tdq->ctrl.mode==1) { // read
        while(moveCnt > 0) {
            while(moveCnt > 0) {
                d = i2c->dev->fifo_data.val;
                moveCnt--;
                cnt++;
                tdq->data[tdq->position++] = (d&0xFF);
            }
            // see if any more chars showed up while empting Fifo.
            moveCnt = i2c->dev->status_reg.rx_fifo_cnt;
            if(moveCnt > (tdq->length - tdq->position)) { //makesure they go in this dq
                // part of these reads go into the next dq
                moveCnt = (tdq->length - tdq->position);
            }
        }
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
        // update Debug rxCount
        cnt += (intBuff[intPos[i2c->num]][1][i2c->num])&&0xffFF;
        intBuff[intPos[i2c->num]][1][i2c->num] = (intBuff[intPos[i2c->num]][1][i2c->num]&0xFFFF0000)|cnt;
#endif
    } else {
        log_e("RxEmpty(%d) call on TxBuffer? dq=%d",moveCnt,i2c->queuePos);
        //  dumpI2c(i2c);
    }
    //log_e("emptied %d",*index);
}

static void IRAM_ATTR i2cIsrExit(i2c_t * i2c,const uint32_t eventCode,bool Fatal)
{

    switch(eventCode) {
    case EVENT_DONE:
        i2c->error = I2C_OK;
        break;
    case EVENT_ERROR_NAK :
        i2c->error =I2C_ADDR_NAK;
        break;
    case EVENT_ERROR_DATA_NAK :
        i2c->error =I2C_DATA_NAK;
        break;
    case EVENT_ERROR_TIMEOUT :
        i2c->error = I2C_TIMEOUT;
        break;
    case EVENT_ERROR_ARBITRATION:
        i2c->error = I2C_ARBITRATION;
        break;
    default :
        i2c->error = I2C_ERROR;
    }
    uint32_t exitCode = EVENT_DONE | eventCode |(Fatal?EVENT_ERROR:0);

    if(i2c->dq[i2c->queuePos].ctrl.mode == 1) {
        emptyRxFifo(i2c);    // grab last few characters
    }
//    log_d("raw=0x%05x status=0x%05x",i2c->dev->int_raw.val,i2c->dev->int_status.val);
    i2c->dev->int_ena.val = 0; // shutdown interrupts
    i2c->dev->int_clr.val = 0x1FFF;
    i2c->stage = I2C_DONE;
    i2c->exitCode = exitCode; //true eventcode

    portBASE_TYPE HPTaskAwoken = pdFALSE,xResult;
    // try to notify Dispatch we are done,
    // else the 50ms timeout will recover the APP, just alittle slower
    HPTaskAwoken = pdFALSE;
    xResult = xEventGroupSetBitsFromISR(i2c->i2c_event, exitCode, &HPTaskAwoken);
    if(xResult == pdPASS) {
        if(HPTaskAwoken==pdTRUE) {
            portYIELD_FROM_ISR();
            //      log_e("Yield to Higher");
        }
    }

}

static void IRAM_ATTR i2c_isr_handler_default(void* arg)
{
    i2c_t* p_i2c = (i2c_t*) arg; // recover data
    uint32_t activeInt = p_i2c->dev->int_status.val&0x7FF;

    if(!activeInt){ //spurious interrupt, possibly bus relate 20180711
       log_d("raw=0x%05x status=0x%05x",p_i2c->dev->int_raw.val,p_i2c->dev->int_status.val);
    }
    if(p_i2c->stage==I2C_DONE) { //get Out, can't service, not configured
      // this error is some kind of a race condition at high clock >400khz
      // see #1588. it does not compromise i2c communications though, just
      // a poke in the eye 
        log_d("eject raw=%p, int=%p",p_i2c->dev->int_raw.val,activeInt);
        p_i2c->dev->int_ena.val = 0;
        p_i2c->dev->int_clr.val = activeInt; //0x1FFF;
        return;
    }
    while (activeInt != 0) { // Ordering of 'if(activeInt)' statements is important, don't change
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
        if(activeInt==(intBuff[intPos[p_i2c->num]][0][p_i2c->num]&0x1fff)) {
            intBuff[intPos[p_i2c->num]][0][p_i2c->num] = (((intBuff[intPos[p_i2c->num]][0][p_i2c->num]>>16)+1)<<16)|activeInt;
        } else {
            intPos[p_i2c->num]++;
            intPos[p_i2c->num] %= INTBUFFMAX;
            intBuff[intPos[p_i2c->num]][0][p_i2c->num] = (1<<16) | activeInt;
            intBuff[intPos[p_i2c->num]][1][p_i2c->num] = 0;
        }

        intBuff[intPos[p_i2c->num]][2][p_i2c->num] = xTaskGetTickCountFromISR(); // when IRQ fired

#endif
 
        if (activeInt & I2C_TRANS_START_INT_ST_M) {
            //   p_i2c->byteCnt=0;
            if(p_i2c->stage==I2C_STARTUP) {
                p_i2c->stage=I2C_RUNNING;
            }

            activeInt &=~I2C_TRANS_START_INT_ST_M;
            p_i2c->dev->int_ena.trans_start = 1; // already enabled? why Again?
            p_i2c->dev->int_clr.trans_start = 1; // so that will trigger after next 'END'
        }

        if (activeInt & I2C_TXFIFO_EMPTY_INT_ST) {//should this be before Trans_start?
            fillTxFifo(p_i2c); //fillTxFifo will enable/disable/clear interrupt
            activeInt&=~I2C_TXFIFO_EMPTY_INT_ST;
        }

        if(activeInt & I2C_RXFIFO_FULL_INT_ST) {
            emptyRxFifo(p_i2c);
            p_i2c->dev->int_clr.rx_fifo_full=1;
            p_i2c->dev->int_ena.rx_fifo_full=1; //why?

            activeInt &=~I2C_RXFIFO_FULL_INT_ST;
        }

        if(activeInt & I2C_MASTER_TRAN_COMP_INT_ST) { // each byte the master sends/recv
            p_i2c->dev->int_clr.master_tran_comp = 1;

            p_i2c->byteCnt++;

            if(p_i2c->byteCnt > p_i2c->dq[p_i2c->queuePos].queueLength) { // simulate Trans_start

                p_i2c->byteCnt -= p_i2c->dq[p_i2c->queuePos].queueLength;

                if(p_i2c->dq[p_i2c->queuePos].ctrl.mode==1) { // grab last characters for this dq
                    emptyRxFifo(p_i2c);
                    p_i2c->dev->int_clr.rx_fifo_full=1;
                    p_i2c->dev->int_ena.rx_fifo_full=1;
                }

                p_i2c->queuePos++; //inc to next dq

                if(p_i2c->queuePos < p_i2c->queueCount) { // load next dq address field + data
                    p_i2c->dev->int_ena.tx_fifo_empty=1;
                }

            }
            activeInt &=~I2C_MASTER_TRAN_COMP_INT_ST;
        }

        if (activeInt & I2C_ACK_ERR_INT_ST_M) {//fatal error, abort i2c service
            if (p_i2c->mode == I2C_MASTER) {
                //     log_e("AcK Err byteCnt=%d, queuepos=%d",p_i2c->byteCnt,p_i2c->queuePos);
                if(p_i2c->byteCnt==1) {
                    i2cIsrExit(p_i2c,EVENT_ERROR_NAK,true);
                } else if((p_i2c->byteCnt == 2) && (p_i2c->dq[p_i2c->queuePos].ctrl.addrReq == 2)) {
                    i2cIsrExit(p_i2c,EVENT_ERROR_NAK,true);
                } else {
                    i2cIsrExit(p_i2c,EVENT_ERROR_DATA_NAK,true);
                }
            }
            return;
        }

        if (activeInt & I2C_TIME_OUT_INT_ST_M) {
            // let Gross timeout occur, Slave may release SCL before the configured timeout expires
            // the Statemachine only has a 13.1ms max timout, some Devices >500ms
            p_i2c->dev->int_clr.time_out =1;
            activeInt &=~I2C_TIME_OUT_INT_ST;
        }

        if (activeInt & I2C_TRANS_COMPLETE_INT_ST_M) {
            i2cIsrExit(p_i2c,EVENT_DONE,false);
            return; // no more work to do
            /*
            // how does slave mode act?
            if (p_i2c->mode == I2C_SLAVE) { // STOP detected
            // empty fifo
            // dispatch callback
             */
        }

        if (activeInt & I2C_ARBITRATION_LOST_INT_ST_M) { //fatal
            i2cIsrExit(p_i2c,EVENT_ERROR_ARBITRATION,true);
            return; // no more work to do
        }

        if (activeInt & I2C_SLAVE_TRAN_COMP_INT_ST_M) {
            p_i2c->dev->int_clr.slave_tran_comp = 1;
            // need to complete this !
        }

        if (activeInt & I2C_END_DETECT_INT_ST_M) {
            p_i2c->dev->int_ena.end_detect = 0;
            p_i2c->dev->int_clr.end_detect = 1;
            p_i2c->dev->ctr.trans_start=0;
            fillCmdQueue(p_i2c,true); // enable interrupts
            p_i2c->dev->ctr.trans_start=1; // go for it
            activeInt&=~I2C_END_DETECT_INT_ST_M;
        }

        if(activeInt) { // clear unhandled if possible?  What about Disabling interrupt?
            p_i2c->dev->int_clr.val = activeInt;
            log_e("unknown int=%x",activeInt);
            // disable unhandled IRQ,
            p_i2c->dev->int_ena.val = p_i2c->dev->int_ena.val & (~activeInt);
        }

        activeInt = p_i2c->dev->int_status.val; // start all over if another interrupt happened
    }
}

/* Stickbreaker added for ISR 11/2017
functional with Silicon date=0x16042000
 */
static i2c_err_t i2cAddQueue(i2c_t * i2c,uint8_t mode, uint16_t i2cDeviceAddr, uint8_t *dataPtr, uint16_t dataLen,bool sendStop, EventGroupHandle_t event)
{
    // need to grab a MUTEX for exclusive Queue,
    // what out if ISR is running?

    if(i2c==NULL) {
        return I2C_ERROR_DEV;
    }

    I2C_DATA_QUEUE_t dqx;
    dqx.data = dataPtr;
    dqx.length = dataLen;
    dqx.position = 0;
    dqx.cmdBytesNeeded = dataLen;
    dqx.ctrl.val = 0;
    dqx.ctrl.addr = i2cDeviceAddr;
    dqx.ctrl.mode = mode;
    dqx.ctrl.stop= sendStop;
    dqx.ctrl.addrReq = ((i2cDeviceAddr&0xFC00)==0x7800)?2:1; // 10bit or 7bit address
    dqx.queueLength = dataLen + dqx.ctrl.addrReq;
    dqx.queueEvent = event;

    if(event) { // an eventGroup exist, so, initialize it
        xEventGroupClearBits(event, EVENT_MASK); // all of them
    }

    if(i2c->dq!=NULL) { // expand
        //log_i("expand");
        I2C_DATA_QUEUE_t* tq =(I2C_DATA_QUEUE_t*)realloc(i2c->dq,sizeof(I2C_DATA_QUEUE_t)*(i2c->queueCount +1));
        if(tq!=NULL) { // ok
            i2c->dq = tq;
            memmove(&i2c->dq[i2c->queueCount++],&dqx,sizeof(I2C_DATA_QUEUE_t));
        } else { // bad stuff, unable to allocate more memory!
            log_e("realloc Failure");
            return I2C_ERROR_MEMORY;
        }
    } else { // first Time
        //log_i("new");
        i2c->queueCount=0;
        i2c->dq =(I2C_DATA_QUEUE_t*)malloc(sizeof(I2C_DATA_QUEUE_t));
        if(i2c->dq!=NULL) {
            memmove(&i2c->dq[i2c->queueCount++],&dqx,sizeof(I2C_DATA_QUEUE_t));
        } else {
            log_e("malloc failure");
            return I2C_ERROR_MEMORY;
        }
    }
    return I2C_ERROR_OK;
}

i2c_err_t i2cAddQueueWrite(i2c_t * i2c, uint16_t i2cDeviceAddr, uint8_t *dataPtr, uint16_t dataLen,bool sendStop,EventGroupHandle_t event)
{
    return i2cAddQueue(i2c,0,i2cDeviceAddr,dataPtr,dataLen,sendStop,event);
}

i2c_err_t i2cAddQueueRead(i2c_t * i2c, uint16_t i2cDeviceAddr, uint8_t *dataPtr, uint16_t dataLen,bool sendStop,EventGroupHandle_t event)
{
    //10bit read is kind of weird, first you do a 0byte Write with 10bit
    //  address, then a ReSTART then a 7bit Read using the the upper 7bit +
    // readBit.

    // this might cause an internal register pointer problem with 10bit
    // devices, But, Don't have any to test agains.
    // this is the Industry Standard specification.

    if((i2cDeviceAddr &0xFC00)==0x7800) { // ten bit read
        i2c_err_t err = i2cAddQueue(i2c,0,i2cDeviceAddr,NULL,0,false,event);
        if(err==I2C_ERROR_OK) {
            return i2cAddQueue(i2c,1,(i2cDeviceAddr>>8),dataPtr,dataLen,sendStop,event);
        } else {
            return err;
        }
    }
    return i2cAddQueue(i2c,1,i2cDeviceAddr,dataPtr,dataLen,sendStop,event);
}
// Stickbreaker

i2c_err_t i2cProcQueue(i2c_t * i2c, uint32_t *readCount, uint16_t timeOutMillis)
{
    /* do the hard stuff here
    install ISR if necessary
    setup EventGroup
    handle bus busy?
     */
    //log_e("procQueue i2c=%p",&i2c);
    if(readCount){ //total reads accomplished in all queue elements
        *readCount = 0;
    }
    if(i2c == NULL) {
        return I2C_ERROR_DEV;
    }
    if (i2c->dev->status_reg.bus_busy) { // return error, let TwoWire() handle resetting the hardware.
        /* if multi master then this if should be changed to this 03/12/2018
        if(multiMaster){// try to let the bus clear by its self
            uint32_t timeOutTick = millis();
            while((i2c->dev->status_reg.bus_busy)&&(millis()-timeOutTick<timeOutMillis())){
              delay(2); // allow task switch
            }
        }
        if(i2c->dev->status_reg.bus_busy){ // still busy, so die
             */
        log_i("Bus busy, reinit");
        return I2C_ERROR_BUSY;
    }

    I2C_MUTEX_LOCK();
    /* what about co-existence with SLAVE mode?
    Should I check if a slaveMode xfer is in progress and hang
    until it completes?
    if i2c->stage == I2C_RUNNING or I2C_SLAVE_ACTIVE
     */
    i2c->stage = I2C_DONE; // until ready

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    for(uint16_t i=0; i<INTBUFFMAX; i++) {
        intBuff[i][0][i2c->num] = 0;
        intBuff[i][1][i2c->num] = 0;
        intBuff[i][2][i2c->num] = 0;
    }
    intPos[i2c->num] = 0;
#endif
    // EventGroup is used to signal transmission completion from ISR
    // not always reliable. Sometimes, the FreeRTOS scheduler is maxed out and refuses request
    // if that happens, this call hangs until the timeout period expires, then it continues.
    if(!i2c->i2c_event) {
        i2c->i2c_event = xEventGroupCreate();
    }
    if(i2c->i2c_event) {
        xEventGroupClearBits(i2c->i2c_event, 0xFF);
    } else { // failed to create EventGroup
        log_e("eventCreate failed=%p",i2c->i2c_event);
        I2C_MUTEX_UNLOCK();
        return I2C_ERROR_MEMORY;
    }

    i2c_err_t reason = I2C_ERROR_OK;
    i2c->mode = I2C_MASTER;

    i2c->dev->ctr.trans_start=0; // Pause Machine
    i2c->dev->timeout.tout = 0xFFFFF; // max 13ms
    I2C_FIFO_CONF_t f;
    f.val = i2c->dev->fifo_conf.val;
    f.rx_fifo_rst = 1; // fifo in reset
    f.tx_fifo_rst = 1; // fifo in reset
    f.nonfifo_en = 0; // use fifo mode
    f.nonfifo_tx_thres =63;
    // need to adjust threshold based on I2C clock rate, at 100k, 30 usually works,
    // sometimes the emptyRx() actually moves 31 bytes
    // it hasn't overflowed yet, I cannot tell if the new byte is added while
    // emptyRX() is executing or before?
  // let i2cSetFrequency() set thrhds  
  //   f.rx_fifo_full_thrhd = 30; // 30 bytes before INT is issued
  //  f.tx_fifo_empty_thrhd = 0;
    f.fifo_addr_cfg_en = 0; // no directed access
    i2c->dev->fifo_conf.val = f.val; // post them all

    f.rx_fifo_rst = 0; // release fifo
    f.tx_fifo_rst = 0;
    i2c->dev->fifo_conf.val = f.val; // post them all
    
    i2c->dev->int_clr.val = 0xFFFFFFFF; // kill them All!
    i2c->dev->ctr.ms_mode = 1; // master!
    i2c->queuePos=0;
    i2c->byteCnt=0;
    uint32_t totalBytes=0; // total number of bytes to be Moved!
    // convert address field to required I2C format
    while(i2c->queuePos < i2c->queueCount) { // need to push these address modes upstream, to AddQueue
        I2C_DATA_QUEUE_t *tdq = &i2c->dq[i2c->queuePos++];
        uint16_t taddr=0;
        if(tdq->ctrl.addrReq ==2) { // 10bit address
            taddr =((tdq->ctrl.addr >> 7) & 0xFE)
                   |tdq->ctrl.mode;
            taddr = (taddr <<8) || (tdq->ctrl.addr&0xFF);
        } else { // 7bit address
            taddr =  ((tdq->ctrl.addr<<1)&0xFE)
                     |tdq->ctrl.mode;
        }
        tdq->ctrl.addr = taddr; // all fixed with R/W bit
        totalBytes += tdq->queueLength; // total number of byte to be moved!
    }
    i2c->queuePos=0;

    fillCmdQueue(i2c,false); // don't enable Tx/RX irq's
    // start adding command[], END irq will keep it full
    //Data Fifo will be filled after trans_start is issued

    i2c->exitCode=0;
    i2c->stage = I2C_STARTUP; // everything configured, now start the I2C StateMachine, and
    // As soon as interrupts are enabled, the ISR will start handling them.
    // it should receive a TXFIFO_EMPTY immediately, even before it
    // receives the TRANS_START

    i2c->dev->int_ena.val =
        I2C_ACK_ERR_INT_ENA | // (BIT(10))  Causes Fatal Error Exit
        I2C_TRANS_START_INT_ENA | // (BIT(9))  Triggered by trans_start=1, initial,END
        I2C_TIME_OUT_INT_ENA  | //(BIT(8))  Trigger by SLAVE SCL stretching, NOT an ERROR
        I2C_TRANS_COMPLETE_INT_ENA | // (BIT(7))  triggered by STOP, successful exit
        I2C_MASTER_TRAN_COMP_INT_ENA | // (BIT(6))  counts each byte xfer'd, inc's queuePos
        I2C_ARBITRATION_LOST_INT_ENA | // (BIT(5))  cause fatal error exit
        I2C_SLAVE_TRAN_COMP_INT_ENA  | // (BIT(4))  unhandled
        I2C_END_DETECT_INT_ENA  | // (BIT(3))   refills cmd[] list
        I2C_RXFIFO_OVF_INT_ENA  | //(BIT(2))  unhandled
        I2C_TXFIFO_EMPTY_INT_ENA | // (BIT(1))    triggers fillTxFifo()
        I2C_RXFIFO_FULL_INT_ENA;  // (BIT(0))     trigger emptyRxFifo()

    if(!i2c->intr_handle) { // create ISR for either peripheral
        // log_i("create ISR %d",i2c->num);
        uint32_t ret = 0;
        uint32_t flags = ESP_INTR_FLAG_EDGE |  //< Edge-triggered interrupt
          ESP_INTR_FLAG_IRAM |  //< ISR can be called if cache is disabled
          ESP_INTR_FLAG_LOWMED;   //< Low and medium prio interrupts. These can be handled in C.

        if(i2c->num) {
            ret = esp_intr_alloc_intrstatus(ETS_I2C_EXT1_INTR_SOURCE, flags, (uint32_t)&i2c->dev->int_status.val, 0x7FF, &i2c_isr_handler_default,i2c, &i2c->intr_handle);
        } else {
            ret = esp_intr_alloc_intrstatus(ETS_I2C_EXT0_INTR_SOURCE, flags, (uint32_t)&i2c->dev->int_status.val, 0x7FF, &i2c_isr_handler_default,i2c, &i2c->intr_handle);
        }

        if(ret!=ESP_OK) {
            log_e("install interrupt handler Failed=%d",ret);
            I2C_MUTEX_UNLOCK();
            return I2C_ERROR_MEMORY;
        }
    }

    //hang until it completes.

    // how many ticks should it take to transfer totalBytes thru the I2C hardware,
    // add user supplied timeOutMillis to Calc Value

    portTickType ticksTimeOut = ((totalBytes*10*1000)/(i2cGetFrequency(i2c))+timeOutMillis)/portTICK_PERIOD_MS;

    //log_e("before startup @tick=%d will wait=%d",xTaskGetTickCount(),ticksTimeOut);

    i2c->dev->ctr.trans_start=1; // go for it

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_ERROR
    portTickType tBefore=xTaskGetTickCount();
#endif
    
    uint32_t eBits = xEventGroupWaitBits(i2c->i2c_event,EVENT_DONE,pdFALSE,pdTRUE,ticksTimeOut);

    //log_e("after WaitBits=%x @tick=%d",eBits,xTaskGetTickCount());

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_ERROR
    portTickType tAfter=xTaskGetTickCount();
#endif

    uint32_t b;
    // if xEventGroupSetBitsFromISR() failed, the ISR could have succeeded but never been
    // able to mark the success

    if(i2c->exitCode!=eBits) { // try to recover from O/S failure
        //  log_e("EventGroup Failed:%p!=%p",eBits,i2c->exitCode);
        eBits=i2c->exitCode;
    }

    if(!(eBits==EVENT_DONE)&&(eBits&~(EVENT_ERROR_NAK|EVENT_ERROR_DATA_NAK|EVENT_ERROR|EVENT_DONE))) { // not only Done, therefore error, exclude ADDR NAK, DATA_NAK
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
        i2cDumpI2c(i2c);
        i2cDumpInts(i2c->num);
#else
        log_n("I2C exitCode=0x%x",eBits);
#endif
    }

    if(eBits&EVENT_DONE) { // no gross timeout
        switch(i2c->error) {
        case I2C_OK :
            reason = I2C_ERROR_OK;
            break;
        case I2C_ERROR :
            reason = I2C_ERROR_DEV;
            break;
        case I2C_ADDR_NAK:
            reason = I2C_ERROR_ACK;
            break;
        case I2C_DATA_NAK:
            reason = I2C_ERROR_ACK;
            break;
        case I2C_ARBITRATION:
            reason = I2C_ERROR_BUS;
            break;
        case I2C_TIMEOUT:
            reason = I2C_ERROR_TIMEOUT;
            break;
        default :
            reason = I2C_ERROR_DEV;
        }
    } else { // GROSS timeout, shutdown ISR , report Timeout
        i2c->stage = I2C_DONE;
        i2c->dev->int_ena.val =0;
        i2c->dev->int_clr.val = 0x1FFF;
        if((i2c->queuePos==0)&&(i2c->byteCnt==0)) { // Bus Busy no bytes Moved
            reason = I2C_ERROR_BUSY;
            eBits = eBits | EVENT_ERROR_BUS_BUSY|EVENT_ERROR|EVENT_DONE;
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_ERROR
            log_e(" Busy Timeout start=0x%x, end=0x%x, =%d, max=%d error=%d",tBefore,tAfter,(tAfter-tBefore),ticksTimeOut,i2c->error);
            i2cDumpI2c(i2c);
            i2cDumpInts(i2c->num);
#endif
        } else { // just a timeout, some data made it out or in.
            reason = I2C_ERROR_TIMEOUT;
            eBits = eBits | EVENT_ERROR_TIMEOUT|EVENT_ERROR|EVENT_DONE;

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_ERROR
            log_e(" Gross Timeout Dead start=0x%x, end=0x%x, =%d, max=%d error=%d",tBefore,tAfter,(tAfter-tBefore),ticksTimeOut,i2c->error);
            i2cDumpI2c(i2c);
            i2cDumpInts(i2c->num);
#endif
        }
    }

    // offloading all EventGroups to dispatch, EventGroups in ISR is not always successful
    // 11/20/2017
    // if error, need to trigger all succeeding dataQueue events with the EVENT_ERROR_PREV

    b = 0;

    while(b < i2c->queueCount) {
        if(i2c->dq[b].ctrl.mode==1 && readCount) {
            *readCount += i2c->dq[b].position; // number of data bytes received
        }
        if(b < i2c->queuePos) { // before any error
            if(i2c->dq[b].queueEvent) { // this data queue element has an EventGroup
                xEventGroupSetBits(i2c->dq[b].queueEvent,EVENT_DONE);
            }
        } else if(b == i2c->queuePos) { // last processed queue
            if(i2c->dq[b].queueEvent) { // this data queue element has an EventGroup
                xEventGroupSetBits(i2c->dq[b].queueEvent,eBits);
            }
        } else { // never processed queues
            if(i2c->dq[b].queueEvent) { // this data queue element has an EventGroup
                xEventGroupSetBits(i2c->dq[b].queueEvent,eBits|EVENT_ERROR_PREV);
            }
        }
        b++;
    }

    I2C_MUTEX_UNLOCK();
    return reason;
}

static void i2cReleaseISR(i2c_t * i2c)
{
    if(i2c->intr_handle) {
        esp_intr_free(i2c->intr_handle);
        i2c->intr_handle=NULL;
    }
}

static bool i2cCheckLineState(int8_t sda, int8_t scl){
    if(sda < 0 || scl < 0){
        return true;//return true since there is nothing to do
    }
    // if the bus is not 'clear' try the recommended recovery sequence, START, 9 Clocks, STOP
    digitalWrite(sda, HIGH);
    digitalWrite(scl, HIGH);
    pinMode(sda, PULLUP|OPEN_DRAIN|OUTPUT|INPUT);
    pinMode(scl, PULLUP|OPEN_DRAIN|OUTPUT|INPUT);

    if(!digitalRead(sda) || !digitalRead(scl)) { // bus in busy state
        log_w("invalid state sda=%d, scl=%d\n", digitalRead(sda), digitalRead(scl));
        digitalWrite(sda, HIGH);
        digitalWrite(scl, HIGH);
        delayMicroseconds(5);
        digitalWrite(sda, LOW);
        for(uint8_t a=0; a<9; a++) {
            delayMicroseconds(5);
            digitalWrite(scl, LOW);
            delayMicroseconds(5);
            digitalWrite(scl, HIGH);
        }
        delayMicroseconds(5);
        digitalWrite(sda, HIGH);
    }

    if(!digitalRead(sda) || !digitalRead(scl)) { // bus in busy state
        log_e("Bus Invalid State, TwoWire() Can't init");
        return false; // bus is busy
    }
    return true;
}

i2c_err_t i2cAttachSCL(i2c_t * i2c, int8_t scl)
{
    if(i2c == NULL) {
        return I2C_ERROR_DEV;
    }
    digitalWrite(scl, HIGH);
    pinMode(scl, OPEN_DRAIN | PULLUP | INPUT | OUTPUT);
    pinMatrixOutAttach(scl, I2C_SCL_IDX(i2c->num), false, false);
    pinMatrixInAttach(scl, I2C_SCL_IDX(i2c->num), false);
    return I2C_ERROR_OK;
}

i2c_err_t i2cDetachSCL(i2c_t * i2c, int8_t scl)
{
    if(i2c == NULL) {
        return I2C_ERROR_DEV;
    }
    pinMatrixOutDetach(scl, false, false);
    pinMatrixInDetach(I2C_SCL_IDX(i2c->num), false, false);
    pinMode(scl, INPUT | PULLUP);
    return I2C_ERROR_OK;
}

i2c_err_t i2cAttachSDA(i2c_t * i2c, int8_t sda)
{
    if(i2c == NULL) {
        return I2C_ERROR_DEV;
    }
    digitalWrite(sda, HIGH);
    pinMode(sda, OPEN_DRAIN | PULLUP | INPUT | OUTPUT );
    pinMatrixOutAttach(sda, I2C_SDA_IDX(i2c->num), false, false);
    pinMatrixInAttach(sda, I2C_SDA_IDX(i2c->num), false);
    return I2C_ERROR_OK;
}

i2c_err_t i2cDetachSDA(i2c_t * i2c, int8_t sda)
{
    if(i2c == NULL) {
        return I2C_ERROR_DEV;
    }
    pinMatrixOutDetach(sda, false, false);
    pinMatrixInDetach(I2C_SDA_IDX(i2c->num), false, false);
    pinMode(sda, INPUT | PULLUP);
    return I2C_ERROR_OK;
}

/*
 * PUBLIC API
 * */
// 24Nov17 only supports Master Mode
i2c_t * i2cInit(uint8_t i2c_num, int8_t sda, int8_t scl, uint32_t frequency) //before this is called, pins should be detached, else glitch
{
    if(i2c_num > 1) {
        return NULL;
    }

    i2c_t * i2c = &_i2c_bus_array[i2c_num];

    if(i2c->sda >= 0){
        i2cDetachSDA(i2c, i2c->sda);
    }
    if(i2c->scl >= 0){
        i2cDetachSCL(i2c, i2c->scl);
    }
    i2c->sda = sda;
    i2c->scl = scl;

#if !CONFIG_DISABLE_HAL_LOCKS
    if(i2c->lock == NULL) {
        i2c->lock = xSemaphoreCreateMutex();
        if(i2c->lock == NULL) {
            return NULL;
        }
    }
#endif
    I2C_MUTEX_LOCK();

    i2cReleaseISR(i2c); // ISR exists, release it before disabling hardware

    if(frequency == 0) {// don't change existing frequency
        frequency = i2cGetFrequency(i2c);
        if(frequency == 0) {
            frequency = 100000L;    // default to 100khz
        }
    }

    if(i2c_num == 0) {
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG,DPORT_I2C_EXT0_RST); //reset hardware
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG,DPORT_I2C_EXT0_CLK_EN);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG,DPORT_I2C_EXT0_RST);//  release reset
    } else {
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG,DPORT_I2C_EXT1_RST); //reset Hardware
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG,DPORT_I2C_EXT1_CLK_EN);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG,DPORT_I2C_EXT1_RST);
    }
    i2c->dev->ctr.val = 0;
    i2c->dev->ctr.ms_mode = 1;
    i2c->dev->ctr.sda_force_out = 1 ;
    i2c->dev->ctr.scl_force_out = 1 ;
    i2c->dev->ctr.clk_en = 1;

    //the max clock number of receiving  a data
    i2c->dev->timeout.tout = 400000;//clocks max=1048575
    //disable apb nonfifo access
    i2c->dev->fifo_conf.nonfifo_en = 0;

    i2c->dev->slave_addr.val = 0;
    I2C_MUTEX_UNLOCK();

    i2cSetFrequency(i2c, frequency);    // reconfigure

    if(!i2cCheckLineState(i2c->sda, i2c->scl)){
        return NULL;
    }

    if(i2c->sda >= 0){
        i2cAttachSDA(i2c, i2c->sda);
    }
    if(i2c->scl >= 0){
        i2cAttachSCL(i2c, i2c->scl);
    }
    return i2c;
}

void i2cRelease(i2c_t *i2c)  // release all resources, power down peripheral
{
    I2C_MUTEX_LOCK();

    if(i2c->sda >= 0){
        i2cDetachSDA(i2c, i2c->sda);
    }
    if(i2c->scl >= 0){
        i2cDetachSCL(i2c, i2c->scl);
    }

    i2cReleaseISR(i2c);

    if(i2c->i2c_event) {
        vEventGroupDelete(i2c->i2c_event);
        i2c->i2c_event = NULL;
    }

    i2cFlush(i2c);

    // reset the I2C hardware and shut off the clock, power it down.
    if(i2c->num == 0) {
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG,DPORT_I2C_EXT0_RST); //reset hardware
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG,DPORT_I2C_EXT0_CLK_EN); // shutdown hardware
    } else {
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG,DPORT_I2C_EXT1_RST); //reset Hardware
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG,DPORT_I2C_EXT1_CLK_EN); // shutdown Hardware
    }

    I2C_MUTEX_UNLOCK();
}

i2c_err_t i2cFlush(i2c_t * i2c)
{
    if(i2c==NULL) {
        return I2C_ERROR_DEV;
    }
    // need to grab a MUTEX for exclusive Queue,
    // what out if ISR is running?
    i2c_err_t rc=I2C_ERROR_OK;
    if(i2c->dq!=NULL) {
        //  log_i("free");
        // what about EventHandle?
        free(i2c->dq);
        i2c->dq = NULL;
    }
    i2c->queueCount=0;
    i2c->queuePos=0;
    // release Mutex
    return rc;
}

i2c_err_t i2cWrite(i2c_t * i2c, uint16_t address, uint8_t* buff, uint16_t size, bool sendStop, uint16_t timeOutMillis){
    i2c_err_t last_error = i2cAddQueueWrite(i2c, address, buff, size, sendStop, NULL);

    if(last_error == I2C_ERROR_OK) { //queued
        if(sendStop) { //now actually process the queued commands, including READs
            last_error = i2cProcQueue(i2c, NULL, timeOutMillis);
            if(last_error == I2C_ERROR_BUSY) { // try to clear the bus
                if(i2cInit(i2c->num, i2c->sda, i2c->scl, 0)) {
                    last_error = i2cProcQueue(i2c, NULL, timeOutMillis);
                }
            }
            i2cFlush(i2c);
        } else { // stop not received, so wait for I2C stop,
            last_error = I2C_ERROR_CONTINUE;
        }
    }
    return last_error;
}

i2c_err_t i2cRead(i2c_t * i2c, uint16_t address, uint8_t* buff, uint16_t size, bool sendStop, uint16_t timeOutMillis, uint32_t *readCount){
    i2c_err_t last_error=i2cAddQueueRead(i2c, address, buff, size, sendStop, NULL);

    if(last_error == I2C_ERROR_OK) { //queued
        if(sendStop) { //now actually process the queued commands, including READs
            last_error = i2cProcQueue(i2c, readCount, timeOutMillis);
            if(last_error == I2C_ERROR_BUSY) { // try to clear the bus
                if(i2cInit(i2c->num, i2c->sda, i2c->scl, 0)) {
                    last_error = i2cProcQueue(i2c, readCount, timeOutMillis);
                }
            }
            i2cFlush(i2c);
        } else { // stop not received, so wait for I2C stop,
            last_error = I2C_ERROR_CONTINUE;
        }
    }
    return last_error;
}

i2c_err_t i2cSetFrequency(i2c_t * i2c, uint32_t clk_speed)
{
    if(i2c == NULL) {
        return I2C_ERROR_DEV;
    }
    I2C_FIFO_CONF_t f;
  
    uint32_t period = (APB_CLK_FREQ/clk_speed) / 2;
    uint32_t halfPeriod = period/2;
    uint32_t quarterPeriod = period/4;

    I2C_MUTEX_LOCK();

    // Adjust Fifo thresholds based on frequency
    f.val    = i2c->dev->fifo_conf.val;
    uint32_t a = (clk_speed / 50000L )+1; 
    if (a > 24) a=24;   
    f.rx_fifo_full_thrhd = 32 - a;
    f.tx_fifo_empty_thrhd = a;
    i2c->dev->fifo_conf.val = f.val; // set thresholds
    log_v("threshold=%d",a);

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
    if(i2c == NULL) {
        return 0;
    }
    uint32_t result = 0;
    uint32_t old_count = (i2c->dev->scl_low_period.period+i2c->dev->scl_high_period.period);
    if(old_count>0) {
        result = APB_CLK_FREQ / old_count;
    } else {
        result = 0;
    }
    return result;
}


/* Stickbreaker ISR mode debug support
 */
void i2cDumpDqData(i2c_t * i2c)
{
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_ERROR
    uint16_t a=0;
    char buff[140];
    I2C_DATA_QUEUE_t *tdq;
    while(a<i2c->queueCount) {
        tdq=&i2c->dq[a];
        log_e("[%d] %x %c %s buf@=%p, len=%d, pos=%d, eventH=%p bits=%x",a,tdq->ctrl.addr,(tdq->ctrl.mode)?'R':'W',(tdq->ctrl.stop)?"STOP":"",tdq->data,tdq->length,tdq->position,tdq->queueEvent,(tdq->queueEvent)?xEventGroupGetBits(tdq->queueEvent):0);
        uint16_t offset = 0;
        while(offset<tdq->length) {
            memset(buff,' ',140);
            buff[139]='\0';
            uint16_t i = 0,j;
            j=sprintf(buff,"0x%04x: ",offset);
            while((i<32)&&(offset < tdq->length)) {
                char ch = tdq->data[offset];
                sprintf((char*)&buff[(i*3)+41],"%02x ",ch);
                if((ch<32)||(ch>126)) {
                    ch='.';
                }
                j+=sprintf((char*)&buff[j],"%c",ch);
                buff[j]=' ';
                i++;
                offset++;
            }
            log_e("%s",buff);
        }
        a++;
    }
#else
    log_n("Enable Core Debug Level \"Error\"");
#endif
}

void i2cDumpI2c(i2c_t * i2c)
{
    log_e("i2c=%p",i2c);
    log_e("dev=%p date=%p",i2c->dev,i2c->dev->date);
#if !CONFIG_DISABLE_HAL_LOCKS
    log_e("lock=%p",i2c->lock);
#endif
    log_e("num=%d",i2c->num);
    log_e("mode=%d",i2c->mode);
    log_e("stage=%d",i2c->stage);
    log_e("error=%d",i2c->error);
    log_e("event=%p bits=%x",i2c->i2c_event,(i2c->i2c_event)?xEventGroupGetBits(i2c->i2c_event):0);
    log_e("intr_handle=%p",i2c->intr_handle);
    log_e("dq=%p",i2c->dq);
    log_e("queueCount=%d",i2c->queueCount);
    log_e("queuePos=%d",i2c->queuePos);
    log_e("byteCnt=%d",i2c->byteCnt);
    if(i2c->dq) {
        i2cDumpDqData(i2c);
    }
}

void i2cDumpInts(uint8_t num)
{
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    uint32_t b;
    log_e("%u row  count   INTR    TX     RX",num);
    for(uint32_t a=1; a<=INTBUFFMAX; a++) {
        b=(a+intPos[num])%INTBUFFMAX;
        if(intBuff[b][0][num]!=0) {
            log_e("[%02d] 0x%04x 0x%04x 0x%04x 0x%04x 0x%08x",b,((intBuff[b][0][num]>>16)&0xFFFF),(intBuff[b][0][num]&0xFFFF),((intBuff[b][1][num]>>16)&0xFFFF),(intBuff[b][1][num]&0xFFFF),intBuff[b][2][num]);
        }
    }
#else
    log_n("enable Core Debug Level \"Error\"");
#endif
}

/* todo
  24Nov17
  Need to think about not usings I2C_MASTER_TRAN_COMP_INT_ST to adjust queuePos.  This
    INT triggers every byte.  The only reason to know which byte is being transfered is
    the status_reg.tx_fifo_cnt and a .txQueued to do this in the fillRxFifo().  The
    same mechanism could work if an error occured in i2cErrorExit().
 */

