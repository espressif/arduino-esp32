HardwareSerial hwSerial(0);
HardwareSerial hwSerial2(2);

void setup()
{
  hwSerial.begin(115200);
  hwSerial2.begin(115200);
  hwSerial2.setRXInterrupt(onSerialRX);
  
}

void loop()
{
  delay(1);
}

void onSerialRX(char* c){
  hwSerial.print(c);
}
