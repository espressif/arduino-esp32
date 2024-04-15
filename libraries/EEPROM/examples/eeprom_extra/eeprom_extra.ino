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
  if (!EEPROM.begin(1000)) {
    Serial.println("Failed to initialize EEPROM");
    Serial.println("Restarting...");
    delay(1000);
    ESP.restart();
  }

  int address = 0;

  EEPROM.writeByte(address, -128);  // -2^7
  address += sizeof(byte);

  EEPROM.writeChar(address, 'A');  // Same as writyByte and readByte
  address += sizeof(char);

  EEPROM.writeUChar(address, 255);  // 2^8 - 1
  address += sizeof(unsigned char);

  EEPROM.writeShort(address, -32768);  // -2^15
  address += sizeof(short);

  EEPROM.writeUShort(address, 65535);  // 2^16 - 1
  address += sizeof(unsigned short);

  EEPROM.writeInt(address, -2147483648);  // -2^31
  address += sizeof(int);

  EEPROM.writeUInt(address, 4294967295);  // 2^32 - 1
  address += sizeof(unsigned int);

  EEPROM.writeLong(address, -2147483648);  // Same as writeInt and readInt
  address += sizeof(long);

  EEPROM.writeULong(address, 4294967295);  // Same as writeUInt and readUInt
  address += sizeof(unsigned long);

  int64_t value = -1223372036854775808LL;  // -2^63
  EEPROM.writeLong64(address, value);
  address += sizeof(int64_t);

  uint64_t Value = 18446744073709551615ULL;  // 2^64 - 1
  EEPROM.writeULong64(address, Value);
  address += sizeof(uint64_t);

  EEPROM.writeFloat(address, 1234.1234);
  address += sizeof(float);

  EEPROM.writeDouble(address, 123456789.123456789);
  address += sizeof(double);

  EEPROM.writeBool(address, true);
  address += sizeof(bool);

  String sentence = "I love ESP32.";
  EEPROM.writeString(address, sentence);
  address += sentence.length() + 1;

  char gratitude[21] = "Thank You Espressif!";
  EEPROM.writeString(address, gratitude);
  address += 21;

  // See also the general purpose writeBytes() and readBytes() for BLOB in EEPROM library
  EEPROM.commit();
  address = 0;

  Serial.println(EEPROM.readByte(address));
  address += sizeof(byte);

  Serial.println((char)EEPROM.readChar(address));
  address += sizeof(char);

  Serial.println(EEPROM.readUChar(address));
  address += sizeof(unsigned char);

  Serial.println(EEPROM.readShort(address));
  address += sizeof(short);

  Serial.println(EEPROM.readUShort(address));
  address += sizeof(unsigned short);

  Serial.println(EEPROM.readInt(address));
  address += sizeof(int);

  Serial.println(EEPROM.readUInt(address));
  address += sizeof(unsigned int);

  Serial.println(EEPROM.readLong(address));
  address += sizeof(long);

  Serial.println(EEPROM.readULong(address));
  address += sizeof(unsigned long);

  value = 0;
  value = EEPROM.readLong64(value);
  Serial.printf("0x%08lX", (uint32_t)(value >> 32));  // Print High 4 bytes in HEX
  Serial.printf("%08lX\n", (uint32_t)value);          // Print Low 4 bytes in HEX
  address += sizeof(int64_t);

  Value = 0;  // Clear Value
  Value = EEPROM.readULong64(Value);
  Serial.printf("0x%08lX", (uint32_t)(Value >> 32));  // Print High 4 bytes in HEX
  Serial.printf("%08lX\n", (uint32_t)Value);          // Print Low 4 bytes in HEX
  address += sizeof(uint64_t);

  Serial.println(EEPROM.readFloat(address), 4);
  address += sizeof(float);

  Serial.println(EEPROM.readDouble(address), 8);
  address += sizeof(double);

  Serial.println(EEPROM.readBool(address));
  address += sizeof(bool);

  Serial.println(EEPROM.readString(address));
  address += sentence.length() + 1;

  Serial.println(EEPROM.readString(address));
  address += 21;
}

void loop() {
  // put your main code here, to run repeatedly:
}
