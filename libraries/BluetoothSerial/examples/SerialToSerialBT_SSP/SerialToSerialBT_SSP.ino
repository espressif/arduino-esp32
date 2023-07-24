// This example code is in the Public Domain (or CC0 licensed, at your option.)
// By Richard Li - 2020
//
// This example creates a bridge between Serial and Classical Bluetooth (SPP with authentication)
// and also demonstrate that SerialBT have the same functionalities of a normal Serial
// SSP - Simple Secure Pairing - The device (ESP32) will display random number and the user is responsible of comparing it to the number
// displayed on the other device (for example phone).
// If the numbers match the user authenticates the pairing on both devices - on phone simply press "Pair" and in terminal for the sketch send 'Y' or 'y' to confirm.
// Alternatively uncomment AUTO_PAIR to skip the terminal confirmation.

#include "BluetoothSerial.h"

//#define AUTO_PAIR // Uncomment to skip the terminal confirmation.

// Check if BlueTooth is available
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
  #error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

// Check Serial Port Profile
#if !defined(CONFIG_BT_SPP_ENABLED)
  #error Serial Port Profile for BlueTooth is not available or not enabled. It is only available for the ESP32 chip.
#endif

// Check Simple Secure Pairing
#if !defined(CONFIG_BT_SSP_ENABLED)
  #error Simple Secure Pairing for BlueTooth is not available or not enabled.
#endif

const char * deviceName = "ESP32_SSP_example";

BluetoothSerial SerialBT;
boolean confirmRequestPending = true;

void BTConfirmRequestCallback(uint32_t numVal)
{
  confirmRequestPending = true;
#ifndef AUTO_PAIR
  Serial.printf("The PIN is: %lu. If it matches number displayed on the other device write \'Y\' or \'y\':\n", numVal);
#else
  SerialBT.confirmReply(true);
#endif
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

void serial_response(){
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

void setup()
{
  Serial.begin(115200);
  SerialBT.enableSSP();
  SerialBT.dropCache();
  SerialBT.onConfirmRequest(BTConfirmRequestCallback);
  SerialBT.onAuthComplete(BTAuthCompleteCallback);
  SerialBT.begin(deviceName); //Bluetooth device name
#ifndef AUTO_PAIR
  Serial.printf("The device started with name \"%s\", now you can pair it with Bluetooth!\nIf the numbers in terminal and on the other device match, press \"Pair\" in phone/computer and write \'Y\' or \'y\' in terminal\n", deviceName);
#else
  Serial.printf("The device started with name \"%s\", now you can pair it with Bluetooth!\nIf the numbers in terminal and on the other device match, press \"Pair\" in phone/computer.\n", deviceName);
#endif
}

void loop()
{
#ifndef AUTO_PAIR
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
    serial_response();
  }
#else
  serial_response();
#endif
}
