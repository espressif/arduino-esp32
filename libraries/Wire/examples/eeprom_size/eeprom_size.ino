#include <Wire.h>
// Connect 4.7k pullups on SDA, SCL
// for ESP32 SDA(pin 21), SCL(pin 22)
// for AtMega328p SDA(Pin A5), SCL(pin A4)
// for Mega2560 SDA(Pin 20), SCL(pin 21)

/* This sketch uses the address rollover of the 24LCxx EEPROMS to detect their size.
  The 24LC32 (4kByte) 0x0000 .. 0x0FFF, a Write to addres 0x1000 will actually
  be stored in 0x0000.  This allows us to read the value of 0x0000, compare
  it to the value read from 0x1000, if they are different, then this IC is
  not a 24LC32.
  If the Value is the same, then we have to change the byte at 0x1000 and
  see if the change is reflected in 0x0000.  If 0x0000 changes, then we know
  that the chip is a 24LC32.  We have to restore the 'changed' value so that
  the data in the EEPROM is not compromized.

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
while((millis()-timeout<100)&&(!ready)){
	Wire.beginTransmission(ID);
	int err=Wire.endTransmission();
	ready=(err==0);
	if(!ready){
		if(err!=2)Serial.printf("{%d}:%s",err,Wire.getErrorText(err));
		}
	}
return ready;
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
				else i+=sprintf_P(&buf[i],PSTR("%dk Bytes"),size/1024);
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
          err= Wire.requestFrom(ID,1);
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