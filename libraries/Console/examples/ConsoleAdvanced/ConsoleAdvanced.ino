/*
 * ConsoleAdvanced — Demonstrates advanced Console API features.
 *
 * Covers APIs not shown in the basic examples:
 *
 *   - addCmdWithContext()      — pass state to a command via a context pointer
 *   - removeCmd()              — unregister a command at runtime
 *   - stopRepl() / beginRepl() — pause and resume the REPL
 *   - setHelpVerboseLevel()    — toggle brief / verbose help output
 *   - clearScreen()            — clear the terminal
 *   - setPlainMode()           — force plain (non-VT100) input
 *   - setMultiLine()           — enable multi-line editing
 *   - Task tuning: setTaskStackSize(), setTaskPriority(), setTaskCore()
 *
 * Created by lucasssvaz
 */

#include <Arduino.h>
#include <Console.h>

// ---------------------------------------------------------------------------
// Context-aware command: addCmdWithContext() demo
//
// A context pointer lets multiple commands share or isolate state without
// global variables.  The pointer is passed as the first argument to the
// callback every time the command is invoked.
// ---------------------------------------------------------------------------

struct LedCtx {
  int pin;
  bool state;
  const char *name;
};

static LedCtx led1 = {2,  false, "LED1"};
static LedCtx led2 = {15, false, "LED2"};

static int cmd_toggle(void *context, int argc, char **argv) {
  LedCtx *led = (LedCtx *)context;
  led->state = !led->state;
  pinMode(led->pin, OUTPUT);
  digitalWrite(led->pin, led->state ? HIGH : LOW);
  printf("%s (GPIO %d) = %s\n", led->name, led->pin, led->state ? "ON" : "OFF");
  return 0;
}

// ---------------------------------------------------------------------------
// removeCmd demo: register/unregister a "secret" command at runtime
// ---------------------------------------------------------------------------

static int cmd_secret(int argc, char **argv) {
  printf("You found the secret command!\n");
  return 0;
}

static bool secretRegistered = false;

static int cmd_lock(int argc, char **argv) {
  if (secretRegistered) {
    Console.removeCmd("secret");
    secretRegistered = false;
    printf("'secret' command removed.\n");
  } else {
    printf("'secret' is not registered.\n");
  }
  return 0;
}

static int cmd_unlock(int argc, char **argv) {
  if (!secretRegistered) {
    Console.addCmd("secret", "A hidden command", cmd_secret);
    secretRegistered = true;
    printf("'secret' command registered. Try it!\n");
  } else {
    printf("'secret' is already available.\n");
  }
  return 0;
}

// ---------------------------------------------------------------------------
// stopRepl / beginRepl demo: pause the REPL for 5 seconds
// ---------------------------------------------------------------------------

static int cmd_pause(int argc, char **argv) {
  printf("Stopping REPL for 5 seconds...\n");
  fflush(stdout);
  Console.stopRepl();
  delay(5000);
  Console.beginRepl();
  return 0;
}

// ---------------------------------------------------------------------------
// setHelpVerboseLevel demo
// ---------------------------------------------------------------------------

static int cmd_verbose_help(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: verbose_help <0|1>\n");
    printf("  0 = brief (names and hints only)\n");
    printf("  1 = verbose (includes help text)\n");
    return 1;
  }
  int level = atoi(argv[1]);
  if (Console.setHelpVerboseLevel(level)) {
    printf("Help verbose level set to %d.\n", level);
  } else {
    printf("Invalid level. Use 0 or 1.\n");
  }
  return 0;
}

// ---------------------------------------------------------------------------
// clearScreen demo
// ---------------------------------------------------------------------------

static int cmd_clear(int argc, char **argv) {
  Console.clearScreen();
  return 0;
}

// ---------------------------------------------------------------------------
// setPlainMode / setMultiLine toggles
// ---------------------------------------------------------------------------

static int cmd_plain(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: plain <on|off>\n");
    return 1;
  }
  bool enable = (strcmp(argv[1], "on") == 0);
  Console.setPlainMode(enable);
  printf("Plain mode %s.\n", enable ? "enabled" : "disabled");
  return 0;
}

static int cmd_multiline(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: multiline <on|off>\n");
    return 1;
  }
  bool enable = (strcmp(argv[1], "on") == 0);
  Console.setMultiLine(enable);
  printf("Multi-line mode %s.\n", enable ? "enabled" : "disabled");
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

  // Console task configuration — must be called before beginRepl()

  // Set the task stack size to 4096 bytes
  Console.setTaskStackSize(4096);
  // Set the task priority to 2
  Console.setTaskPriority(2);
  // Set the task core to 1 (core 1)
  Console.setTaskCore(1);      // pin REPL to core 1

  // Configure Console prompt
  Console.setPrompt("adv> ");

  // Initialize Console
  if (!Console.begin()) {
    Serial.println("Console init failed");
    return;
  }

  // Context-aware commands: same callback, different context pointer.
  // Each LED struct carries its own pin number, state, and label.
  Console.addCmdWithContext("led1", "Toggle LED1", cmd_toggle, &led1);
  Console.addCmdWithContext("led2", "Toggle LED2", cmd_toggle, &led2);

  // Dynamic command management
  Console.addCmd("unlock", "Register the 'secret' command",   cmd_unlock);
  Console.addCmd("lock",   "Unregister the 'secret' command", cmd_lock);

  // REPL control
  Console.addCmd("pause", "Stop the REPL for 5 seconds", cmd_pause);

  // Help verbosity
  Console.addCmd("verbose_help", "Set help verbosity", "<0|1>", cmd_verbose_help);

  // Terminal utilities
  Console.addCmd("clear",     "Clear the terminal screen",         cmd_clear);
  Console.addCmd("plain",     "Toggle plain (non-VT100) mode",     "<on|off>", cmd_plain);
  Console.addCmd("multiline", "Toggle multi-line editing",         "<on|off>", cmd_multiline);

  // Add built-in help command
  Console.addHelpCmd();

  // Begin Read-Eval-Print Loop
  if (!Console.beginRepl()) {
    Serial.println("REPL start failed");
  }
}

void loop() {
  // Loop task is not used in this example, so we delete it to free up resources
  vTaskDelete(NULL);
}
