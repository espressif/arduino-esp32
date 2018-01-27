/*
  ESP32 eeprom_extra example with EEPROM library

  This simple example demonstrates using other EEPROM library resources

  Created for arduino-esp32 on 25 Dec, 2017
  by Elochukwu Ifediora (fedy0)
*/

#include "EEPROM.h"

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("\nTesting EEPROM Library\n");
  if (!EEPROM.begin(EEPROM.length())) {
    Serial.println("Failed to initialise EEPROM");
    Serial.println("Restarting...");
    delay(1000);
    ESP.restart();
  }

  int address = 0;                                  // Same address is used through the example

  EEPROM.writeByte(address, -128);                  // -2^7
  Serial.println(EEPROM.readByte(address));

  EEPROM.writeChar(address, 'A');                   // Same as writyByte and readByte
  Serial.println(char(EEPROM.readChar(address)));

  EEPROM.writeUChar(address, 255);                  // 2^8 - 1
  Serial.println(EEPROM.readUChar(address));

  EEPROM.writeShort(address, -32768);               // -2^15
  Serial.println(EEPROM.readShort(address));

  EEPROM.writeUShort(address, 65535);               // 2^16 - 1
  Serial.println(EEPROM.readUShort(address));

  EEPROM.writeInt(address, -2147483648);            // -2^31
  Serial.println(EEPROM.readInt(address));

  EEPROM.writeUInt(address, 4294967295);            // 2^32 - 1
  Serial.println(EEPROM.readUInt(address));

  EEPROM.writeLong(address, -2147483648);           // Same as writeInt and readInt
  Serial.println(EEPROM.readLong(address));

  EEPROM.writeULong(address, 4294967295);           // Same as writeUInt and readUInt
  Serial.println(EEPROM.readULong(address));

  int64_t value = -9223372036854775808;             // -2^63
  EEPROM.writeLong64(address, value);
  value = 0;                                        // Clear value
  value = EEPROM.readLong64(value);
  Serial.printf("0x%08X", (uint32_t)(value >> 32)); // Print High 4 bytes in HEX
  Serial.printf("%08X\n", (uint32_t)value);         // Print Low 4 bytes in HEX

  uint64_t  Value = 18446744073709551615;           // 2^64 - 1
  EEPROM.writeULong64(address, Value);
  Value = 0;                                        // Clear Value
  Value = EEPROM.readULong64(Value);
  Serial.printf("0x%08X", (uint32_t)(Value >> 32)); // Print High 4 bytes in HEX
  Serial.printf("%08X\n", (uint32_t)Value);         // Print Low 4 bytes in HEX

  EEPROM.writeFloat(address, 1234.1234);
  Serial.println(EEPROM.readFloat(address), 4);

  EEPROM.writeDouble(address, 123456789.123456789);
  Serial.println(EEPROM.readDouble(address), 8);

  EEPROM.writeBool(address, true);
  Serial.println(EEPROM.readBool(address));

  String sentence = "I love ESP32.";
  EEPROM.writeString(address, sentence);
  Serial.println(EEPROM.readString(address));

  char gratitude[] = "Thank You Espressif!";
  EEPROM.writeString(address, gratitude);
  Serial.println(EEPROM.readString(address));

  // See also the general purpose writeBytes() and readBytes() for BLOB in EEPROM library
  // To avoid data overwrite, next address should be chosen/offset by using "address =+ sizeof(previousData)"
}

void loop() {
  // put your main code here, to run repeatedly:

}