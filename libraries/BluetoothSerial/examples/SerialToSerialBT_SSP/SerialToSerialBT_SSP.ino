// This example code is in the Public Domain (or CC0 licensed, at your option.)
// By Richard Li - 2020
//
// This example creates a bridge between Serial and Classical Bluetooth (SPP with authentication)
// and also demonstrate that SerialBT have the same functionalities of a normal Serial
// SSP - Simple Secure Pairing - The device (ESP32) will display random number and the user is responsible of comparing it to the number
// displayed on the other device (for example phone).
// If the numbers match the user authenticates the pairing.

#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Bluetooth not available or not enabled. It is only available for the ESP32 chip.
#endif

const char * deviceName = "ESP32_SSP_example";

BluetoothSerial SerialBT;
boolean confirmRequestPending = true;

void BTConfirmRequestCallback(uint32_t numVal)
{
  confirmRequestPending = true;
  Serial.printf("The PIN is: %lu\n", numVal);
}

void BTAuthCompleteCallback(boolean success)
{
  confirmRequestPending = false;
  if (success)
  {
    Serial.println("Pairing success!!");
  }
  else
  {
    Serial.println("Pairing failed, rejected by user!!");
  }
}


void setup()
{
  Serial.begin(115200);
  SerialBT.enableSSP();
  SerialBT.onConfirmRequest(BTConfirmRequestCallback);
  SerialBT.onAuthComplete(BTAuthCompleteCallback);
  SerialBT.begin(deviceName); //Bluetooth device name
  Serial.printf("The device started with name \"%s\", now you can pair it with Bluetooth!\n", deviceName);
}

void loop()
{
  if (confirmRequestPending)
  {
    if (Serial.available())
    {
      int dat = Serial.read();
      if (dat == 'Y' || dat == 'y')
      {
        SerialBT.confirmReply(true);
      }
      else
      {
        SerialBT.confirmReply(false);
      }
    }
  }
  else
  {
    if (Serial.available())
    {
      SerialBT.write(Serial.read());
    }
    if (SerialBT.available())
    {
      Serial.write(SerialBT.read());
    }
    delay(20);
  }
}
