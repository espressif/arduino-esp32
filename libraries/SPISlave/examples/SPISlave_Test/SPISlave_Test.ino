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
#include <SlaveSPIClass.h>

SPISlave slave;

void setup()
{
    Serial.begin(115200);
    Serial.setDebugOutput(true);

    // data has been received from the master. Beware that len is always 32
    // and the buffer is autofilled with zeroes if data is less than 32 bytes long
    // It's up to the user to implement protocol for handling data length
    slave.onData([](uint8_t * data, size_t len) {
        String message = String((char *)data);
        (void) len;
        if(message.equals("Hello Slave!")) {
            slave.setData("Hello Master!");
        } else if(message.equals("Are you alive?")) {
            char answer[33];
            sprintf(answer,"Alive for %lu seconds!", millis() / 1000);
            slave.setData(answer);
        } else {
            slave.setData("Say what?");
        }
        Serial.printf("Question: %s\n", (char *)data);
    });

    // The master has read out outgoing data buffer
    // that buffer can be set with SPISlave.setData
    slave.onDataSent([]() {
        Serial.println("Answer Sent");
    });

    // status has been received from the master.
    // The status register is a special register that bot the slave and the master can write to and read from.
    // Can be used to exchange small data or status information
    slave.onStatus([](uint32_t data) {
        Serial.printf("Status: %u\n", data);
        slave.setStatus(millis()); //set next status
    });

    // The master has read the status register
    slave.onStatusSent([]() {
        Serial.println("Status Sent");
    });

    // Setup SPI Slave registers and pins
    slave.begin((gpio_num_t)SO, (gpio_num_t)SI, (gpio_num_t)SCLK, (gpio_num_t)SS);

    // Set the status register (if the master reads it, it will read this value)
    slave.setStatus(millis());

    // Sets the data registers. Limited to 32 bytes at a time.
    // SPISlave.setData(uint8_t * data, size_t len); is also available with the same limitation
    slave.setData("Ask me a question!");
}

void loop() {}
