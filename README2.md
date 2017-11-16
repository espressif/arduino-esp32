# A fork of Espressif/arduino-esp32:
## i2c communications using a ISR

The existing main fork of arduino-esp32 implement i2c communications using a polled monitoring of the I2C Hardware StateMachine.  The implementation of the I2C ReSTART operations leaves the I2C Hardware StateMachine(SM) in an unstable configuration.  The I2C protocol arbitrates exclusive ownership of the bus to a single Master Device from a `START` signal to the next `STOP`.  A restart operation is just a start signal inserted into this `START` -> `STOP` flow.  Schematically like this:
 `START` (Read or Write) -> `ReSTART` (Read or Write) -> `STOP`.
 
 By I2C protocol as [UM10204 I2C-Bus Specification and User Manual](https://Fwww.nxp.com/docs/en/user-guide/UM10204.pdf), A valid I2C transaction must be a `START` at least one Data Byte `STOP`.  The Arduino environment is base on the AVR processor series, These Atmel CPU's implementation of the the I2C protocol allows unlimited pauses between protocol elements.  The Espressif implement enforces a Strict TimeOut between elements.  This has resulted in perceived I2C bus stability issues ([834](https://github.com/espressif/arduino-esp32/issues/834),[811](https://github.com/espressif/arduino-esp32/issues/811),[741](https://github.com/espressif/arduino-esp32/issues/741),[682](https://github.com/espressif/arduino-esp32/issues/682),[659](https://github.com/espressif/arduino-esp32/issues/659) and many more.  @lonerzzz created a partial solution [#751](https://github.com/espressif/arduino-esp32/pull/751).
 I spend a couple of weeks investigating these problems, and discovered this Hard TimeOut was the basis of all of these problems.  Once this Hard TimeOut limit is triggered the SM cannot recover without a complete reinitialization.  
The existing Arduino code base is reliant on the AVR's ability to infintely pause a i2c transaction.  The standard coding practice of:
```c++
// set internal address pointer in I2C EEPROM from which to read
Wire.beginTransmission(ID);
Wire.write(highByte(addr));
Wire.write(lowByte(addr));
uint8_t err =Wire.endTransmission(false); // don't send a STOP, just Pause I2C operations
if(err==0){ // successfully set internal address pointer
  err=Wire.requestFrom(addr,len);
  if(err==0){ // read failed
    Serial.print("Bad Stuff!! Read Failed\n");
    }
  else {// successful read
    while(Wire.avaiable()){
      Serial.print((char)Wire.read());
      }
    Serial.println();
    }
  }
```
May not function correctly with the ESP32, actually *usually* will not function correctly.  The current arduino-esp32 platform is built upon the espressif-idf which is built on FreeRTOS, a multi-process/processor operating system. The Arduino sketch is just one task executing on One of the Two processor cores. The TimeOut is triggered when the FreeRTOS Task Scheduler does a task switch between `Wire.endTransmission(false);` and the i2c activites inside `Wire.requestfrom()`. The maximum TimeOut interval is around 12ms. 
My solution is to avoid this TimeOut from every being possible.  I have changed how `Wire()` uses the SM.  Specifically I have changed how a `ReSTART` operation is implemented.  To avoid the TimeOut I have create a i2c queuing system that does not allow an i2c transaction to start unless a `STOP` has been issued.  But, alas, this creates some incompatibilities with the pre-exisiting Arduino code base. The changes to the standard Arduino `Wire()` coding are minimal, but, they are necessary:
```c++
// new, maybe Excessive Error Return codes for @stickbreaker:arduino-esp32
typedef enum {
    I2C_ERROR_OK=0,
    I2C_ERROR_DEV,
    I2C_ERROR_ACK,
    I2C_ERROR_TIMEOUT,
    I2C_ERROR_BUS,
    I2C_ERROR_BUSY,
    I2C_ERROR_MEMORY,
    I2C_ERROR_CONTINUE,
    I2C_ERROR_MISSING_WRITE,
    I2C_ERROR_NO_BEGIN
} i2c_err_t;

// set internal address pointer in I2C EEPROM from which to read
Wire.beginTransmission(ID);
Wire.write(highByte(addr));
Wire.write(lowByte(addr));
uint8_t err =Wire.endTransmission(false); // don't send a STOP, just Pause I2C operations

//err will be I2C_ERROR_CONTINUE (7), an indication that the preceding
//transaction has been Queued, NOT EXECUTED

if(err==7){ // Prior Operation has been queued, it is NOT guaranteed that it will
// successfully occur!
  err=Wire.requestFrom(addr,len);
  if(err!=len){ // complete/partial read failure
    Serial.print("Bad Stuff!! Read Failed lastError=");
    Serial.print(Wire.lastError(),DEC);
    }
  // some of the read may have executed
  while(Wire.avaiable()){
    Serial.print((char)Wire.read());
    }
  Serial.println();
  }
```

Additionally I have expanded the ability of `Wire()` to handle larger Reads and Writes: up to 64k-1, I have tested READs of these sizes, but the largest single WRITE i have tested is 128bytes.  I can send more data to a 24LC512, but it will only store the last 128bytes.

I have create a few new methods for Wire:
```c++
    uint8_t oldEndTransmission(uint8_t); //released implementation
    size_t oldRequestFrom(uint8_t address, size_t size, bool sendStop); //released implementation
//@stickBreaker for big blocks and ISR model
    uint8_t writeTransaction(uint8_t address, uint8_t* buff, size_t size, bool sendStop);// big block handling
    size_t requestFrom(uint8_t address, size_t size, bool sendStop);
    size_t requestFrom(uint8_t address, uint8_t* buf, size_t size, bool sendStop);
    size_t polledRequestFrom(uint8_t address, uint8_t* buf, size_t size, bool sendStop);//a BigBlock test case Not USING ISR
    size_t transact(size_t readLen); // replacement for endTransmission(false),requestFrom(ID,readLen,true);
    size_t transact(uint8_t* readBuff, size_t readLen);// bigger Block read
    i2c_err_t lastError(); // Expose complete error
    void dumpInts(); // diagnostic dump for the last 64 different i2c Interrupts
    size_t getClock(); // current i2c Clock rate
``` 

`transact()` coding is:
```c++
// set internal address pointer in I2C EEPROM from which to read
Wire.beginTransmission(ID);
Wire.write(highByte(addr));
Wire.write(lowByte(addr));

uint8_t err=Wire.transact(len);
if(err!=len){ // complete/partial read failure
  Serial.print("Bad Stuff!! Read Failed lastError=");
  Serial.print(Wire.lastError(),DEC);
  }
  // some of the read may have executed
while(Wire.avaiable()){
  Serial.print((char)Wire.read());
  }
Serial.println();

```

This **APLHA** release should be compiled with ESP32 Dev Module as its target, and
Set the "core debug level" to 'error'

There is MINIMAL to NO ERROR detection, BUS, BUSY.  because I have not encounter any of them!



Chuck.
