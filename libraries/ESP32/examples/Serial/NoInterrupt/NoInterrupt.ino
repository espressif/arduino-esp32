HardwareSerial hwSerial(0);
HardwareSerial hwSerial2(2);

void setup()
{
  hwSerial.begin(115200);
  hwSerial2.begin(115200);
}

void loop()
{
  hwSerial.print(hwSerial2.read());
}
