/*
    SPI Slave Demo Sketch
    Connect the SPI Master device to the following pins on the esp32:

    GPIO    NodeMCU   Name  |   Uno
  ===================================
     34       D8       SS   |   D10
     4        D7      MOSI  |   D11
     17       D6      MISO  |   D12
     16       D5      SCK   |   D13
*/
#define SO 17
#define SI 4
#define SCLK 16
#define SS 34

#include <WiFi.h>
#include "SlaveSPI.h"

void setup()
{
    Serial.begin(115200);
    Serial.setDebugOutput(true);

    // data has been received from the master. Beware that len is always 32
    // and the buffer is autofilled with zeroes if data is less than 32 bytes long
    // It's up to the user to implement protocol for handling data length
    SPISlave.onData([](uint8_t * data, size_t len) {
        String message = String((char *)data);
        (void) len;
        if(message.equals("Hello Slave!")) {
            slave.setData("Hello Master!");
        } else if(message.equals("Are you alive?")) {
            char answer[33];
            sprintf(answer,"Alive for %lu seconds!", millis() / 1000);
            SPISlave.setData(answer);
        } else {
            SPISlave.setData("Say what?");
        }
        Serial.printf("Question: %s\n", (char *)data);
    });

    // The master has read out outgoing data buffer
    // that buffer can be set with SPISlave.setData
    SPISlave.onDataSent([]() {
        Serial.println("Answer Sent");
    });

    // status has been received from the master.
    // The status register is a special register that bot the slave and the master can write to and read from.
    // Can be used to exchange small data or status information
    SPISlave.onStatus([](uint32_t data) {
        Serial.printf("Status: %u\n", data);
        slave.setStatus(millis()); //set next status
    });

    // The master has read the status register
    SPISlave.onStatusSent([]() {
        Serial.println("Status Sent");
    });

    // Setup SPI Slave registers and pins
    SPISlave.begin((gpio_num_t)SO, (gpio_num_t)SI, (gpio_num_t)SCLK, (gpio_num_t)SS);

    // Set the status register (if the master reads it, it will read this value)
    SPISlave.setStatus(millis());

    // Sets the data registers. Limited to 32 bytes at a time.
    // SPISlave.setData(uint8_t * data, size_t len); is also available with the same limitation
    SPISlave.setData("Ask me a question!");
}

void loop() {}

