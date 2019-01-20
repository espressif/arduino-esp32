# Debugging I2C

With the release of Arduino-ESP32 V1.0.1 the I2C subsystem contains code to exhaustively report communication errors.  
* Basic debugging can be enable by setting the *CORE DEBUG LEVEL* at or above *ERROR*. All errors will be directed the the *DEBUG OUTPUT* normally connected to `Serial()`.  
* Enhanced debugging can be used to generate specified information at specific positions during the i2c communication sequence. Increase *CORE DEBUG LEVEL* to ***DEBUG***

## Enable Debug Buffer
The Enhanced debug features are enabled by uncommenting the `\\#define ENABLE_I2C_DEBUG_BUFFER` at line 45 of `esp32-hal-i2c.c`.  
* When Arduino-Esp32 is installed in Windows with Arduino Boards Manager, `esp32-hal-i2c.c` can be found in:
`C:\Users\{user}\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.1\cores\esp32\`
* When Arduino-Esp32 Development version is installed from GitHub, `esp32-hal-i2c.c` can be found in:
`{arduino Sketch}\hardware\espressif\esp32\cores\esp32\`


```c++ 
//#define ENABLE_I2C_DEBUG_BUFFER
```
Change it to:
```c++ 
#define ENABLE_I2C_DEBUG_BUFFER
```
and recompile/upload the resulting code to your ESP32.

Enabling this `#define` will consume an additional 2570 bytes of RAM and include a commensurate amount of code FLASH. If you see the message `"Debug Buffer not Enabled"` in your console log I would suggest you un-comment the line and regenerate the error.  Additional information will be supplied on the log console.

## Manually controlled Debugging
Manual logging of the i2c control data buffers can be accomplished by using the debug control function of `Wire()`:
```c++
    uint32_t setDebugFlags( uint32_t setBits, uint32_t resetBits);
```
`setBits`, and `resetBits` manually cause output of the control structures to the log console.   They are bit fields that enable/disable the reporting of individual control structures during specific phases of the i2c communications sequence.  The 32bit values are divided into four 8bit fields.  Currently only five bits are defined. ***If an error is detected during normal operations, the relevant control structure will bit added to the log irrespective of the current debug flags.***

* **bit 0** causes DumpI2c to execute 
header information about current communications event,
and the dataQueue elements showing the logical i2c transaction commands
* **bit 1** causes DumpInts to execute
Actual sequence of interrupts handled during last communications event, cleared on entry into `ProcQueue()`.
* **bit 2** causes DumpCmdqueue to execute
The last block of commands to the i2c peripheral.
* **bit 3** causes DumpStatus to execute
A descriptive display of the 32bit i2c peripheral status word.
* **bit 4** causes DumpFifo to execute
A buffer listing the sequence of data added to the txFifo of the i2c peripheral.

Of the four division, only three are currently implemented:
* 0xXX - - - - - - : at entry of ProcQueue (`bitFlags << 24`)
* 0x - - XX - - - - : at exit of ProcQueue (`bitFlags << 16`)
* 0x - - - - - - XX : at entry of Flush    (`bitFlags`)
 
For example, to display the sequence of Interrupts processed during the i2c communication transaction, **bit 1** would be set, and, since this information on Interrupt usage would only be valid after the communications have completed, the locus would be *at exit of ProcQueue*.  The following code would be necessary.
### code
```c++
uint8_t flag = 1 << 1; // turn on bit 1
uint32_t debugFlag = flag << 16; // correctly position the 8bits of flag as the second byte of setBits.
Wire.setDebugFlags(debugFlag,0);// resetBits=0 says leave all current setBits as is.

Wire.requestFrom(id,byteCount); // read byteCount bytes from slave at id 

Wire.setDebugFlags(0,debugFlag); // don't add any new debug, remove debugFlag
``` 
### output of log console
```
[I][esp32-hal-i2c.c:437] i2cTriggerDumps(): after ProcQueue
[I][esp32-hal-i2c.c:346] i2cDumpInts(): 0 row	count	INTR	TX	RX	Tick 
[I][esp32-hal-i2c.c:350] i2cDumpInts(): [01]	0x0001	0x0002	0x0003	0x0000	0x005baac5
[I][esp32-hal-i2c.c:350] i2cDumpInts(): [02]	0x0001	0x0200	0x0000	0x0000	0x005baac5
[I][esp32-hal-i2c.c:350] i2cDumpInts(): [03]	0x0001	0x0080	0x0000	0x0008	0x005baac6
```    
# Debug Log example
### Code
To read eight bytes of data from a DS1307 RTCC
```
    uint32_t debugFlag = 0x001F0000;
    uint8_t ID = 0x68;
    uint8_t block=8;
    
    if(debugFlag >0){
        Wire.setDebugFlags(debugFlag,0);
    }

    Wire.beginTransmission(ID);
    Wire.write(lowByte(addr));
    if((err=Wire.endTransmission(false))!=0)    {
        Serial.printf(" EndTransmission=%d(%s)",Wire.lastError(),Wire.getErrorText(Wire.lastError()));

        if(err!=2) {
            Serial.printf(", resetting\n");
            if( !Wire.begin()) Serial.printf(" Reset Failed\n");
            if(debugFlag >0) Wire.setDebugFlags(0,debugFlag);
            return;
        } else {
            Serial.printf(", No Device present, aborting\n");
            currentCommand= NO_COMMAND;
            return;
        }
    }
    err = Wire.requestFrom(ID,block,true);
    if(debugFlag >0){
        Wire.setDebugFlags(0,debugFlag);
    }
```
### output of log console
```
[I][esp32-hal-i2c.c:437] i2cTriggerDumps(): after ProcQueue
[E][esp32-hal-i2c.c:318] i2cDumpI2c(): i2c=0x3ffbdc78
[I][esp32-hal-i2c.c:319] i2cDumpI2c(): dev=0x60013000 date=0x16042000
[I][esp32-hal-i2c.c:321] i2cDumpI2c(): lock=0x3ffb843c
[I][esp32-hal-i2c.c:323] i2cDumpI2c(): num=0
[I][esp32-hal-i2c.c:324] i2cDumpI2c(): mode=1
[I][esp32-hal-i2c.c:325] i2cDumpI2c(): stage=3
[I][esp32-hal-i2c.c:326] i2cDumpI2c(): error=1
[I][esp32-hal-i2c.c:327] i2cDumpI2c(): event=0x3ffb85c4 bits=10
[I][esp32-hal-i2c.c:328] i2cDumpI2c(): intr_handle=0x3ffb85f4
[I][esp32-hal-i2c.c:329] i2cDumpI2c(): dq=0x3ffb858c
[I][esp32-hal-i2c.c:330] i2cDumpI2c(): queueCount=2
[I][esp32-hal-i2c.c:331] i2cDumpI2c(): queuePos=1
[I][esp32-hal-i2c.c:332] i2cDumpI2c(): errorByteCnt=0
[I][esp32-hal-i2c.c:333] i2cDumpI2c(): errorQueue=0
[I][esp32-hal-i2c.c:334] i2cDumpI2c(): debugFlags=0x001F0000
[I][esp32-hal-i2c.c:288] i2cDumpDqData(): [0] 7bit 68 W  buf@=0x3ffc04b2, len=1, pos=1, ctrl=11101
[I][esp32-hal-i2c.c:306] i2cDumpDqData(): 0x0000: .                                00 
[I][esp32-hal-i2c.c:288] i2cDumpDqData(): [1] 7bit 68 R STOP buf@=0x3ffc042c, len=8, pos=8, ctrl=11111
[I][esp32-hal-i2c.c:306] i2cDumpDqData(): 0x0000: 5P......                         35 50 07 06 13 09 18 00 
[I][esp32-hal-i2c.c:346] i2cDumpInts(): 0 row	count	INTR	TX	RX	Tick 
[I][esp32-hal-i2c.c:350] i2cDumpInts(): [01]	0x0001	0x0002	0x0003	0x0000	0x000073d5
[I][esp32-hal-i2c.c:350] i2cDumpInts(): [02]	0x0001	0x0200	0x0000	0x0000	0x000073d5
[I][esp32-hal-i2c.c:350] i2cDumpInts(): [03]	0x0001	0x0080	0x0000	0x0008	0x000073d6
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 0]	Y	RSTART	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 1]	Y	WRITE	val[0]	exp[0]	en[1]	bytes[1]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 2]	Y	WRITE	val[0]	exp[0]	en[1]	bytes[1]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 3]	Y	RSTART	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 4]	Y	WRITE	val[0]	exp[0]	en[1]	bytes[1]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 5]	Y	READ	val[0]	exp[0]	en[0]	bytes[7]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 6]	Y	READ	val[1]	exp[0]	en[0]	bytes[1]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 7]	Y	STOP	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 8]	N	RSTART	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 9]	N	RSTART	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [10]	N	RSTART	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [11]	N	RSTART	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [12]	N	RSTART	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [13]	N	RSTART	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [14]	N	RSTART	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [15]	N	RSTART	val[0]	exp[0]	en[0]	bytes[0]
[I][esp32-hal-i2c.c:385] i2cDumpStatus(): ack(0) sl_rw(0) to(0) arb(0) busy(0) sl(1) trans(0) rx(0) tx(0) sclMain(5) scl(6)
[I][esp32-hal-i2c.c:424] i2cDumpFifo(): WRITE 0x68   0 
[I][esp32-hal-i2c.c:424] i2cDumpFifo(): READ  0x68
```
## Explaination of log output
### DumpI2c
```
[I][esp32-hal-i2c.c:437] i2cTriggerDumps(): after ProcQueue
[E][esp32-hal-i2c.c:318] i2cDumpI2c(): i2c=0x3ffbdc78
[I][esp32-hal-i2c.c:319] i2cDumpI2c(): dev=0x60013000 date=0x16042000
[I][esp32-hal-i2c.c:321] i2cDumpI2c(): lock=0x3ffb843c
[I][esp32-hal-i2c.c:323] i2cDumpI2c(): num=0
[I][esp32-hal-i2c.c:324] i2cDumpI2c(): mode=1
[I][esp32-hal-i2c.c:325] i2cDumpI2c(): stage=3
[I][esp32-hal-i2c.c:326] i2cDumpI2c(): error=1
[I][esp32-hal-i2c.c:327] i2cDumpI2c(): event=0x3ffb85c4 bits=10
[I][esp32-hal-i2c.c:328] i2cDumpI2c(): intr_handle=0x3ffb85f4
[I][esp32-hal-i2c.c:329] i2cDumpI2c(): dq=0x3ffb858c
[I][esp32-hal-i2c.c:330] i2cDumpI2c(): queueCount=2
[I][esp32-hal-i2c.c:331] i2cDumpI2c(): queuePos=1
[I][esp32-hal-i2c.c:332] i2cDumpI2c(): errorByteCnt=0
[I][esp32-hal-i2c.c:333] i2cDumpI2c(): errorQueue=0
[I][esp32-hal-i2c.c:334] i2cDumpI2c(): debugFlags=0x001F0000
```
variable | description
---- | ----
**i2c** | *memory address for control block*
**dev** | *memory address for access to i2c peripheral registers*
**date** | *revision date of peripheral silicon 2016, 42 week*
**lock** | *hal lock handle*
**num** | *0,1 which peripheral is being controlled*
**mode** | *configuration of driver 0=none, 1=MASTER, 2=SLAVE, 3=MASTER and SLAVE*
**stage** | *internal STATE of driver 0=not configured, 1=startup, 2=running, 3=done*
**error** | *internal ERROR status 0=not configured, 1=ok, 2=error, 3=address NAK, 4=data NAK, 5=arbitration loss, 6=timeout*
**event** | *handle for interprocess FreeRTOS eventSemaphore for communication between ISR and APP*
**intr_handle** | *FreeRTOS handle for allocated interrupt*
**dq** | *memory address for data queue control block*
**queueCount** | *number of data operations in queue control block*
**queuePos** | *last executed data block*
**errorByteCnt** | *position in current data block when error occured -1=address byte*
**errorQueue** | *queue that was executing when error occurred*
**debugFlags** | *current specified error bits*

### DQ data
```
[I][esp32-hal-i2c.c:288] i2cDumpDqData(): [0] 7bit 68 W  buf@=0x3ffc04b2, len=1, pos=1, ctrl=11101
[I][esp32-hal-i2c.c:306] i2cDumpDqData(): 0x0000: .                                00 
[I][esp32-hal-i2c.c:288] i2cDumpDqData(): [1] 7bit 68 R STOP buf@=0x3ffc042c, len=8, pos=8, ctrl=11111
[I][esp32-hal-i2c.c:306] i2cDumpDqData(): 0x0000: 5P......                         35 50 07 06 13 09 18 00 
```
variable | description
--- | ---
**[n]** | *index of data queue element*
**i2c address** | *7bit= 7bit i2c slave address, 10bit= 10bit i2c slave address*
**direction** | *W=Write, R=READ*
**STOP** | *command issued a I2C STOP, else if blank, a RESTART was issued by next dq element.*
**buf@** | *pointer to data buffer*
**len** | *length of data buffer*
**pos** | *last position used in buffer*
**ctrl** | *bit field for data queue control, this bits describe if all necessary commands have been added to peripheral command buffer. in order(START,ADDRESS_Write,DATA,STOP,ADDRESS_value*
**0xnnnn** | *data buffer content, displayable followed by HEX, 32 bytes on a line.*
 
### DumpInts
```
[I][esp32-hal-i2c.c:346] i2cDumpInts(): 0 row	count	INTR	TX	RX	Tick 
[I][esp32-hal-i2c.c:350] i2cDumpInts(): [01]	0x0001	0x0002	0x0003	0x0000	0x000073d5
[I][esp32-hal-i2c.c:350] i2cDumpInts(): [02]	0x0001	0x0200	0x0000	0x0000	0x000073d5
[I][esp32-hal-i2c.c:350] i2cDumpInts(): [03]	0x0001	0x0080	0x0000	0x0008	0x000073d6
```
variable | description
---- | ----
**row** | *array index*
**count** | *number of consecutive, duplicate interrupts*
**INTR** | *bit value of active interrupt (from ..\esp32\tools\sdk\include\soc\soc\i2c_struct.h)*
**TX** | *number of bytes added to txFifo*
**RX** | *number of bytes read from rxFifo*
**Tick** | *current tick counter from xTaskGetTickCountFromISR()*

### DumpCmdQueue
```
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 0]	Y	RSTART	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 1]	Y	WRITE	val[0]	exp[0]	en[1]	bytes[1]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 2]	Y	WRITE	val[0]	exp[0]	en[1]	bytes[1]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 3]	Y	RSTART	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 4]	Y	WRITE	val[0]	exp[0]	en[1]	bytes[1]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 5]	Y	READ	val[0]	exp[0]	en[0]	bytes[7]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 6]	Y	READ	val[1]	exp[0]	en[0]	bytes[1]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 7]	Y	STOP	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 8]	N	RSTART	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [ 9]	N	RSTART	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [10]	N	RSTART	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [11]	N	RSTART	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [12]	N	RSTART	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [13]	N	RSTART	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [14]	N	RSTART	val[0]	exp[0]	en[0]	bytes[0]
[E][esp32-hal-i2c.c:243] i2cDumpCmdQueue(): [15]	N	RSTART	val[0]	exp[0]	en[0]	bytes[0]
```
Column | description
---- | ----
**command** | *RSTART= generate i2c START sequence, WRITE= output byte(s), READ= input byte(s), STOP= generate i2c STOP sequence, END= continuation flag for peripheral to pause execution waiting for a refilled command list*
**val** | *value for ACK bit, 0 = LOW, 1= HIGH*
**exp** | *test of ACK bit 0=no, 1=yes*
**en** | *output of val, 0=no, 1=yes*
**bytes** | *number of byte to send(WRITE) or receive(READ) 1..255*
 
### DumpStatus
```
[I][esp32-hal-i2c.c:385] i2cDumpStatus(): ack(0) sl_rw(0) to(0) arb(0) busy(0) sl(1) trans(0) rx(0) tx(0) sclMain(5) scl(6)
```
variable | description
---- | ----
**ack** | *last value for ACK bit*
**sl_rw** | *mode for SLAVE operation 0=write, 1=read*
**to** | *timeout*
**arb** | *arbitration loss*
**busy** | *bus is inuse by other Master, or SLAVE is holding SCL,SDA*
**sl** | *last address on bus was equal to slave_addr*
**trans** | *a byte has moved though peripheral*
**rx** | *count of bytes in rxFifo*
**tx** | *count of bytes in txFifo*
**sclMain** | *state machine for i2c module.  0: SCL_MAIN_IDLE,  1: SCL_ADDRESS_SHIFT, 2: SCL_ACK_ADDRESS, 3: SCL_RX_DATA, 4: SCL_TX_DATA, 5: SCL_SEND_ACK, 6 :SCL_WAIT_ACK*
**scl** | *SCL clock state. 0: SCL_IDLE, 1: SCL_START, 2: SCL_LOW_EDGE, 3: SCL_LOW, 4: SCL_HIGH_EDGE, 5: SCL_HIGH, 6:SCL_STOP*

### DumpFifo
```
[I][esp32-hal-i2c.c:424] i2cDumpFifo(): WRITE 0x68   0 
[I][esp32-hal-i2c.c:424] i2cDumpFifo(): READ  0x68
```
Mode | datavalues
--- | ---
**WRITE** | the following bytes added to the txFifo are in response to a WRITE command
**READ** | the following bytes added to the txFifo are in response to a READ command

