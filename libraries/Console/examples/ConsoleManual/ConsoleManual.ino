/*
 * ConsoleManual — Run commands from Serial without a background REPL task.
 *
 * Demonstrates using Console.run() inside loop() to execute commands read
 * from Serial. Useful when you need full control over the input loop or
 * want to integrate the console into an existing event-driven sketch.
 *
 * Type a command (e.g. "heap" or "help") in the Serial Monitor and press
 * Enter. No background task is created.
 *
 * Created by lucasssvaz
 */

#include <Arduino.h>
#include <Console.h>

// ---------------------------------------------------------------------------
// Command: heap
// ---------------------------------------------------------------------------
static int cmd_heap(int argc, char **argv) {
  printf("Free heap : %lu bytes\n", (unsigned long)ESP.getFreeHeap());
  return 0;
}

// ---------------------------------------------------------------------------
// Command: uptime
// ---------------------------------------------------------------------------
static int cmd_uptime(int argc, char **argv) {
  unsigned long ms = millis();
  unsigned long s = ms / 1000;
  unsigned long days = s / 86400;
  unsigned long hours = (s % 86400) / 3600;
  unsigned long mins = (s % 3600) / 60;
  unsigned long secs = s % 60;
  printf("up %lu day(s), %02lu:%02lu:%02lu  (%.3f s)\n", days, hours, mins, secs, ms / 1000.0f);
  return 0;
}

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------

static String inputBuffer;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  // Configure Console prompt
  Console.setPrompt("");  // No prompt — we print it manually below

  // Initialize Console
  if (!Console.begin()) {
    Serial.println("Console init failed");
    return;
  }

  // Add commands
  Console.addCmd("heap",   "Show free heap",  cmd_heap);
  Console.addCmd("uptime", "Show uptime",     cmd_uptime);

  // Add built-in help command
  Console.addHelpCmd();

  Serial.println("Console ready. Type a command and press Enter.");
  Serial.print("> ");
}

// In this example we manually implement the input loop instead of using the REPL task.
// This is useful when you need full control over the input loop or want to integrate the console into an existing event-driven sketch.
void loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      inputBuffer.trim();
      if (inputBuffer.length() > 0) {
        Serial.println();
        Console.run(inputBuffer);
        inputBuffer = "";
      }
      Serial.print("> ");
    } else {
      Serial.print(c);  // local echo
      inputBuffer += c;
    }
  }
}
