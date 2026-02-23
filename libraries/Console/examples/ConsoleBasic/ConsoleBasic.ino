/*
 * ConsoleBasic — Basic interactive REPL with history and tab completion.
 *
 * This example demonstrates the basic usage of the Console library.
 * It uses the REPL (Read-Eval-Print Loop) API to create an interactive shell.
 * These are the available commands:
 *
 *   version — Print firmware version
 *   heap    — Show free heap memory
 *   restart — Restart the device
 *   echo    — Echo arguments back
 *
 * Connect with any serial terminal (115200 baud) and type "help" to list
 * available commands. Note that for full functionality (like history and tab completion),
 * you need to use a serial terminal that supports VT100 escape sequences (Arduino IDE does not).
 *
 * Board support: all ESP32 variants using UART or HWCDC (USB-JTAG).
 * Currently, the Console library does not support USB OTG (CDC via TinyUSB / USBSerial).
 *
 * Created by lucasssvaz
 */

#include <Arduino.h>
#include <Console.h>

// ---------------------------------------------------------------------------
// Command: version
// ---------------------------------------------------------------------------
static int cmd_version(int argc, char **argv) {
  printf("arduino-esp32 %s\n", ESP_ARDUINO_VERSION_STR);
  return 0;
}

// ---------------------------------------------------------------------------
// Command: heap
// ---------------------------------------------------------------------------
static int cmd_heap(int argc, char **argv) {
  printf("Free heap : %lu bytes\n", (unsigned long)ESP.getFreeHeap());
  printf("Min heap  : %lu bytes\n", (unsigned long)ESP.getMinFreeHeap());
  return 0;
}

// ---------------------------------------------------------------------------
// Command: restart
// ---------------------------------------------------------------------------
static int cmd_restart(int argc, char **argv) {
  printf("Restarting in 1 second...\n");
  fflush(stdout);
  delay(1000);
  ESP.restart();
  return 0;
}

// ---------------------------------------------------------------------------
// Command: echo  <message...>
// ---------------------------------------------------------------------------
static int cmd_echo(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    printf("%s", argv[i]);
    if (i < argc - 1) {
      printf(" ");
    }
  }
  printf("\n");
  return 0;
}

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  // Configure Console
  Console.setPrompt("esp32> ");
  Console.setMaxHistory(32);

  // Initialize Console
  if (!Console.begin()) {
    Serial.println("Console init failed");
    return;
  }

  // Add commands
  Console.addCmd("version", "Print firmware version", cmd_version);
  Console.addCmd("heap",    "Show free heap memory",  cmd_heap);
  Console.addCmd("restart", "Restart the device",     cmd_restart);
  Console.addCmd("echo",    "Echo arguments back",    "<message...>", cmd_echo);

  // Add built-in help command
  Console.addHelpCmd();

  // Begin Read-Eval-Print Loop
  if (!Console.attachToSerial(true)) {
    Serial.println("REPL start failed");
  }
}

void loop() {
  // Loop task is not used in this example, so we delete it to free up resources
  vTaskDelete(NULL);
}
