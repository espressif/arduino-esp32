#include <BLEPeripheral.h>

void setup()
{
  BLEPeripheral blePeripheral("MY_BLUETOOTH");
  uint8_t manuf_data[3] = {0xDE, 0xAD, 0xBE};
  blePeripheral.setCompanyID(0xBEEF);
  blePeripheral.setManufactureData(manuf_data, 3);
  blePeripheral.begin();
}

void loop()
{
  
}
