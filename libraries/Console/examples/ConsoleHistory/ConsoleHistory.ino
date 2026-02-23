/*
 * ConsoleHistory — Persistent history, PSRAM allocation, and splitArgv demo.
 *
 * Demonstrates APIs not covered by the basic examples:
 *
 *   - setHistoryFile()  — save/load history to LittleFS across reboots
 *   - setMaxHistory()   — limit number of stored history lines
 *   - usePsram()        — allocate Console heap and REPL task stack in PSRAM
 *   - splitArgv()       — split a raw string into argv-style tokens
 *
 * On boot the REPL loads the previous session's history.  Use the up/down
 * arrow keys (VT100 terminals) or the "history" command to inspect entries.
 * Power-cycle the board to verify history persists.
 *
 * Created by lucasssvaz
 */

#include <Arduino.h>
#include <Console.h>
#include <LittleFS.h>

static const char *HISTORY_PATH = "/console_history.txt";

// ---------------------------------------------------------------------------
// Command: history — print the current history buffer
// ---------------------------------------------------------------------------
static int cmd_history(int argc, char **argv) {
  File f = LittleFS.open(HISTORY_PATH, "r");
  if (!f) {
    printf("No history saved yet.\n");
    return 0;
  }

  int index = 1;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      printf("  %3d  %s\n", index++, line.c_str());
    }
  }
  f.close();

  if (index == 1) {
    printf("History is empty.\n");
  }
  return 0;
}

// ---------------------------------------------------------------------------
// Command: tokenize — demonstrate Console.splitArgv()
//
// splitArgv() splits a raw string into argc/argv tokens, handling quotes
// and backslash escapes.  Useful when you receive a full command line from
// a non-REPL source (MQTT, BLE, HTTP, etc.) and need to parse it.
// ---------------------------------------------------------------------------
static int cmd_tokenize(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: tokenize <string>\n");
    printf("  Splits the string into tokens.  Use quotes for multi-word tokens.\n");
    printf("  Example: tokenize hello \"big world\" foo\n");
    return 1;
  }

  // Reconstruct the argument string from argv[1..] so the user can type
  // something like: tokenize hello "big world" foo
  String raw;
  for (int i = 1; i < argc; i++) {
    if (i > 1) {
      raw += ' ';
    }
    raw += argv[i];
  }

  // splitArgv() works in-place — it modifies the buffer and sets pointers
  // into it.  We need a mutable copy.
  char *buf = strdup(raw.c_str());
  if (!buf) {
    printf("Out of memory\n");
    return 1;
  }

  char *tokens[16];
  size_t count = Console.splitArgv(buf, tokens, 16);

  printf("Input : \"%s\"\n", raw.c_str());
  printf("Tokens: %zu\n", count);
  for (size_t i = 0; i < count; i++) {
    printf("  [%zu] \"%s\"\n", i, tokens[i]);
  }

  free(buf);
  return 0;
}

// ---------------------------------------------------------------------------
// Command: heap — show free memory
// ---------------------------------------------------------------------------
static int cmd_heap(int argc, char **argv) {
  printf("Free heap : %lu bytes\n", (unsigned long)ESP.getFreeHeap());
#if CONFIG_SPIRAM
  printf("Free PSRAM: %lu bytes\n", (unsigned long)ESP.getFreePsram());
#endif
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

  // Mount LittleFS (format on first use)
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed — history will not persist.");
  }

  // Persistent history: lines are stored in a plain text file on LittleFS.
  // On begin(), existing history is loaded; after every command, the file
  // is updated automatically.
  Console.setHistoryFile(HISTORY_PATH);
  Console.setMaxHistory(50);

  // If PSRAM is available, move Console allocations there to save internal
  // SRAM. Falls back to internal RAM if PSRAM is not available.
  Console.usePsram(true);

  // Configure Console prompt
  Console.setPrompt("hist> ");

  // Initialize Console
  if (!Console.begin()) {
    Serial.println("Console init failed");
    return;
  }

  // Add commands
  Console.addCmd("history",  "Show history info",             cmd_history);
  Console.addCmd("tokenize", "Split a string into tokens",    "<string>", cmd_tokenize);
  Console.addCmd("heap",     "Show free memory",              cmd_heap);

  // Add built-in help command
  Console.addHelpCmd();

  if (!Console.beginRepl()) {
    Serial.println("REPL start failed");
  }
}

void loop() {
  // Loop task is not used in this example, so we delete it to free up resources
  vTaskDelete(NULL);
}
