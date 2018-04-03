#include <Wire.h>
// Connect 4.7k pullups on SDA, SCL
// for ESP32 SDA(pin 21), SCL(pin 22)

/* This sketch uses the address rollover of the 24LCxx EEPROMS to detect their size.
  This detection sequence will not work with small capacity EEPROMs 24LC01 .. 24LC16.
    These small EEPROMS use single byte addressing, To access more than 256 bytes,
    the lc04, lc08, and lc16 EEPROMs respond as if they were multiple lc02's with
    consective I2CDevice addresses.
    device I2Caddr Address Range
    24LC01 0x50     0x00 .. 0x7F
    24LC02 0x50     0x00 .. 0xFF
    24LC04 0x50     0x00 .. 0xFF
           0x51     0x00 .. 0xFF
    24LC08 0x50     0x00 .. 0xFF
           0x51     0x00 .. 0xFF
           0x52     0x00 .. 0xFF
           0x53     0x00 .. 0xFF
    24LC16 0x50     0x00 .. 0xFF
           0x51     0x00 .. 0xFF
           0x52     0x00 .. 0xFF
           0x53     0x00 .. 0xFF
           0x54     0x00 .. 0xFF
           0x55     0x00 .. 0xFF
           0x56     0x00 .. 0xFF
           0x57     0x00 .. 0xFF
  The 24LC32 with selectable I2C address 0x50..0x57 are 4kByte devices with an internal
  address range of 0x0000 .. 0x0FFF. A Write to address 0x1000 will actually
  be stored in 0x0000.  This allows us to read the value of 0x0000, compare
  it to the value read from 0x1000, if they are different, then this IC is
  not a 24LC32.
  If the Value is the same, then we have to change the byte at 0x1000 and
  see if the change is reflected in 0x0000.  If 0x0000 changes, then we know
  that the chip is a 24LC32.  We have to restore the 'changed' value so that
  the data in the EEPROM is not compromised.

  This pattern of read, compare, test, restore is used for each possible size.
  All that changes is the test Address, 0x1000, 0x2000, 0x4000, 0x8000.
  if the 0x8000 test is does not change, then the chip is a 24LC512.
*/

/* after a write, the I2C device requires upto 5ms to actually program
  the memory cells.  During this programming cycle, the IC does not respond
  to I2C requests.  This feature 'NAK' polling is used to determine when
  the program cycle has completed.
*/
bool i2cReady(uint8_t ID){
uint32_t timeout=millis();
bool ready=false;
while((millis()-timeout<10)&&(!ready)){ // try to evoke a response from the device.
// If the it does not respond within 10ms return as a failure.
  Wire.beginTransmission(ID);
  int err=Wire.endTransmission();
  ready=(err==0);
  if(!ready){
    if(err!=2)Serial.printf("{%d}:%s",err,Wire.getErrorText(err));
    }
  }
return ready;
}

void dispBuff(uint8_t *buf, uint16_t len,uint16_t offset){
char asciibuf[100];
uint8_t bufPos=0;
uint16_t adr=0;
asciibuf[0] ='\0';
while(adr<len){
	if(((offset+adr)&0x1F)==0){
    if(asciibuf[0]) Serial.printf(" %s\n",asciibuf);
		Serial.printf("0x%04x:",(uint16_t)(offset+adr));
		bufPos=0;
		}
	Serial.printf(" %02x",buf[adr]);
	char ch=buf[adr];
	if((ch<32)||(ch>127)) ch ='.';
	bufPos+=sprintf(&asciibuf[bufPos],"%c",ch);
	adr++;
	}

while(bufPos<32){
	Serial.print("   ");
	bufPos++;
	}
Serial.printf(" %s\n",asciibuf);
}

/* detectWritePageSize() attempts to write a 256 byte block to an I2C EEPROM.
  This large block will use the side effect of the WritePage to detect it's size.
  EEPROM's have to erase a 'page' of data in their memory cell array before they 
  can change it. To facilitate partial page writes they contain a temporary 'WritePage'
  that is used store the contents of the memory cells while their page is erase.
  When data is written to the device it is merged into this temporary buffer.  If the 
  amount of data written is longer than the temporary buffer it rolls over to the beginning
  of the temporary buffer.  In a 24lc32, the standard WritePage is 32 bytes, if 256
  bytes of data is written to the device, only the last 32 bytes are stored.
  This side effect allow easy detect of the WritePage size.
*/
  
uint16_t detectWritePageSize(uint16_t i2cID,bool verbose=false){
if(verbose) Serial.printf("detecting WritePage Size for (0x%02x)\n",i2cID);
uint16_t adr = 0,ps=0;
randomSeed(micros());
adr = random(256)*256; //address is selected at random for wear leveling purposes.
uint8_t *buf =(uint8_t*)malloc(256); //restore buffer
uint8_t *buf1 =(uint8_t*)malloc(258); // write buffer
i2cReady(i2cID); // device may completing a Write Cycle, wait if necessary
Wire.beginTransmission(i2cID);
Wire.write(highByte(adr));
Wire.write(lowByte(adr));
uint16_t count = Wire.transact(buf,256);//save current EEPROM content for Restore
if(Wire.lastError()==0){ // successful read, now we can try the test
  for(uint16_t a=0;a<256;a++){ //initialize a detectable pattern in the writeBuffer
    buf1[a+2]=a;   // leave room for the the address 
    }
  buf1[0] = highByte(adr);
  buf1[1] = lowByte(adr);
  Wire.writeTransmission(i2cID,buf1,258);
  if(Wire.lastError()==0){ // wait for the write to complete
    if(i2cReady(i2cID)){
      Wire.beginTransmission(i2cID);
      Wire.write(highByte(adr));
      Wire.write(lowByte(adr));
      Wire.transact(&buf1[2],256);
      if(Wire.lastError()==0){
        ps = 256-buf1[2]; // if the read succeeded, the byte read from offset 0x0
        // can be used to calculate the WritePage size.  On a 24lc32 with a 32byte
        // WritePage it will contain 224, therefore 256-224 = 32 byte Writepage.
        if(verbose){
          Serial.printf("Origonal data\n",i2cID);
          dispBuff(buf,256,adr);
          Serial.printf("\n OverWritten with\n");
          dispBuff(&buf1[2],256,adr);
          Serial.printf("Calculated Write Page is %d\n",ps);
          }
        memmove(&buf1[2],buf,256); // copy the savebuffer back into
        if(i2cReady(i2cID)){
          Wire.writeTransmission(i2cID,buf1,ps+2); // two address bytes plus WritePage
          }
        if(Wire.lastError()!=0){
          Serial.printf("unable to Restore prom\n");
          if(i2cReady(i2cID)){
            Wire.beginTransmission(i2cID);
            Wire.write(highByte(adr));
            Wire.write(lowByte(adr));
            Wire.transact(&buf1[2],256);
            Serial.printf("\n Restored to\n");
            dispBuff(&buf1[2],256,adr);
            }
          }
        }
      }
    }
  }
free(buf1);
free(buf);
return ps;
}

/* eepromSize() only works on 24LC32 .. 24LC512 eeprom,
 the smaller 24LC01 .. 24LC16 use one byte addressings.
*/
void eepromSize(){
Serial.println("Discovering eeprom sizes 0x50..0x57");
uint8_t ID=0x50,i;
uint16_t size;
char buf[256];
while(ID<0x58){
  i=0;
  size = 0x1000; // Start at 4k, 16bit address devices,
  i += sprintf_P(&buf[i],PSTR("0x%02X: "),ID);
  if(i2cReady(ID)) { // EEPROM answered
    uint8_t zeroByte;
    Wire.beginTransmission(ID);
    Wire.write((uint8_t)0); // set address ptr to 0, two bytes High
    Wire.write((uint8_t)0); // set address ptr to 0, two bytes Low
    uint8_t err=Wire.endTransmission();
    if(err==0){// worked, device exists at this ID
      err=Wire.requestFrom(ID,(uint8_t)1);
      if(err==1){// got the value of the byte at address 0
        zeroByte=Wire.read();
        uint8_t saveByte,testByte;
        do{
          if(i2cReady(ID)){
            Wire.beginTransmission(ID);
            Wire.write(highByte(size)); // set next test address
            Wire.write(lowByte(size));
            Wire.endTransmission();
            err=Wire.requestFrom(ID,(uint8_t)1);
            if(err==1){
              saveByte=Wire.read();
              if(saveByte == zeroByte) { // have to test it
                Wire.beginTransmission(ID);
                Wire.write(highByte(size)); // set next test address
                Wire.write(lowByte(size));
                Wire.write((uint8_t)~zeroByte); // change it
                err=Wire.endTransmission();
                if(err==0){ // changed it
                  if(!i2cReady(ID)){
                    i+=sprintf_P(&buf[i],PSTR(" notReady2.\n"));
                    Serial.print(buf);
                    ID++;
                    break;
                    }
                  Wire.beginTransmission(ID);
                  Wire.write((uint8_t)0); // address 0 byte High
                  Wire.write((uint8_t)0); // address 0 byte Low
                  err=Wire.endTransmission();
                  if(err==0){
                    err=Wire.requestFrom(ID,(uint8_t)1);
                    if(err==1){ // now compare it
                      testByte=Wire.read();
                      }
                    else {
                      testByte=~zeroByte; // error out
                      }
                    }
                  else {
                    testByte=~zeroByte;
                    }
                  }
                else {
                  testByte = ~zeroByte;
                  }

                //restore byte
                if(!i2cReady(ID)){
                  i+=sprintf_P(&buf[i],PSTR(" notReady4.\n"));
                  Serial.print(buf);
                  ID++;
                  break;
                  }
                Wire.beginTransmission(ID);
                Wire.write(highByte(size)); // set next test address
                Wire.write(lowByte(size));
                Wire.write((uint8_t)saveByte); // restore it
                Wire.endTransmission();
                }
              else testByte = zeroByte; // They were different so the eeprom Is Bigger
              }
            else testByte=~zeroByte;
            }
          else testByte=~zeroByte;
          if(testByte==zeroByte){
            size = size <<1;
            }
          }while((testByte==zeroByte)&&(size>0));
        if(size==0) i += sprintf_P(&buf[i],PSTR("64k Bytes"));
        else i+=sprintf_P(&buf[i],PSTR("%2uk Bytes"),size/1024);
        i+=sprintf_P(&buf[i],PSTR(" WritePage=%3u"),detectWritePageSize(ID,false));
        if(!i2cReady(ID)){
          i+=sprintf_P(&buf[i],PSTR(" notReady3.\n"));
          Serial.print(buf);
          ID++;
          continue;
          }
        Wire.beginTransmission(ID);
        Wire.write((uint8_t)0); // set address ptr to 0, two bytes High
        Wire.write((uint8_t)0); // set address ptr to 0, two bytes Low
        err=Wire.endTransmission();
        if(err==0){
          err= Wire.requestFrom(ID,(uint8_t)1);
          if (err==1) {
            testByte = Wire.read();
            if(testByte != zeroByte){ //fix it
              Wire.beginTransmission(ID);
              Wire.write((uint8_t)0); // set address ptr to 0, two bytes High
              Wire.write((uint8_t)0); // set address ptr to 0, two bytes Low
              Wire.write(zeroByte);  //Restore
              err=Wire.endTransmission();
              }
            }
          }
        }
      else i+=sprintf_P(&buf[i],PSTR("Read 0 Failure"));
      }
    else i+=sprintf_P(&buf[i],PSTR("Write Adr 0 Failure"));

    }
  else i+=sprintf_P(&buf[i],PSTR("Not Present"));
  Serial.println(buf);
  ID++;
  }
}

void setup(){
  Serial.begin(115200);
  Wire.begin();
  eepromSize();
}

void loop(){
}