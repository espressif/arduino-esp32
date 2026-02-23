/*
 * ConsoleGPIO — GPIO control via CLI using argtable3 argument parsing.
 *
 * Implements three sub-commands accessed as a single "gpio" command:
 *
 *   gpio read  <pin>              — read and print pin level
 *   gpio write <pin> <0|1>        — write HIGH or LOW to a pin
 *   gpio mode  <pin> <mode>       — set pin mode (in/out/in_pu/in_pd)
 *   led <on|off> [color]          — toggle LED (with RGB color on supported boards)
 *
 * Demonstrates argtable3 usage: typed arguments with automatic hint
 * generation and built-in error reporting. The "led" command shows how to
 * reuse existing command handlers from a higher-level convenience command.
 *
 * Created by lucasssvaz
 */

#include <Arduino.h>
#include <Console.h>
#include "argtable3/argtable3.h"

#define RGB_BRIGHTNESS 64 // Change brightness (max 255)

// ---------------------------------------------------------------------------
// gpio read <pin>
// ---------------------------------------------------------------------------

static struct {
  struct arg_int *pin;   // required int: GPIO pin number
  struct arg_end *end;   // sentinel: tracks parse errors
} gpio_read_args;

static int cmd_gpio_read(int argc, char **argv) {
  // arg_parse() validates argv against the argument table, returns error count
  int nerrors = arg_parse(argc, argv, (void **)&gpio_read_args);
  if (nerrors != 0) {
    // Print human-readable error messages (e.g. "missing <pin> argument")
    arg_print_errors(stderr, gpio_read_args.end, argv[0]);
    return 1;
  }

  // Access the parsed integer value via ->ival[0]
  int pin = gpio_read_args.pin->ival[0];
  int val = digitalRead(pin);
  Serial.printf("GPIO %d = %d (%s)\n", pin, val, val ? "HIGH" : "LOW");
  return 0;
}

// ---------------------------------------------------------------------------
// gpio write <pin> <value>
// ---------------------------------------------------------------------------

static struct {
  struct arg_int *pin;    // required int: GPIO pin number
  struct arg_int *value;  // required int: 0 or 1
  struct arg_end *end;    // sentinel: tracks parse errors
} gpio_write_args;

static int cmd_gpio_write(int argc, char **argv) {
  int nerrors = arg_parse(argc, argv, (void **)&gpio_write_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, gpio_write_args.end, argv[0]);
    return 1;
  }

  // Access the parsed integer values via ->ival[0]
  int pin = gpio_write_args.pin->ival[0];
  int val = gpio_write_args.value->ival[0];

  if (val != 0 && val != 1) {
    Serial.println("gpio write: value must be 0 or 1");
    return 1;
  }

  digitalWrite(pin, val);
  Serial.printf("GPIO %d set to %d (%s)\n", pin, val, val ? "HIGH" : "LOW");
  return 0;
}

// ---------------------------------------------------------------------------
// gpio mode <pin> <mode>
// ---------------------------------------------------------------------------

static struct {
  struct arg_int *pin;   // required int: GPIO pin number
  struct arg_str *mode;  // required string: mode name (in/out/in_pu/in_pd)
  struct arg_end *end;   // sentinel: tracks parse errors
} gpio_mode_args;

static int cmd_gpio_mode(int argc, char **argv) {
  int nerrors = arg_parse(argc, argv, (void **)&gpio_mode_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, gpio_mode_args.end, argv[0]);
    return 1;
  }

  // Access the parsed integer value via ->ival[0]
  int pin = gpio_mode_args.pin->ival[0];

  // Access the parsed string value via ->sval[0]
  const char *modeStr = gpio_mode_args.mode->sval[0];

  uint8_t mode;
  if (strcmp(modeStr, "in") == 0) {
    mode = INPUT;
  } else if (strcmp(modeStr, "out") == 0) {
    mode = OUTPUT;
  } else if (strcmp(modeStr, "in_pu") == 0) {
    mode = INPUT_PULLUP;
  } else if (strcmp(modeStr, "in_pd") == 0) {
    mode = INPUT_PULLDOWN;
  } else {
    Serial.printf("gpio mode: unknown mode '%s'. Use: in, out, in_pu, in_pd\n", modeStr);
    return 1;
  }

  pinMode(pin, mode);
  Serial.printf("GPIO %d mode set to %s\n", pin, modeStr);
  return 0;
}

// ---------------------------------------------------------------------------
// Top-level dispatcher: gpio <subcmd> ...
// ---------------------------------------------------------------------------

static int cmd_gpio(int argc, char **argv) {
  if (argc < 2) {
    Serial.println("Usage: gpio <read|write|mode> ...");
    Serial.println("  gpio read  <pin>");
    Serial.println("  gpio write <pin> <0|1>");
    Serial.println("  gpio mode  <pin> <in|out|in_pu|in_pd>");
    return 1;
  }

  // Shift argv so the sub-command is argv[0]
  if (strcmp(argv[1], "read") == 0) {
    return cmd_gpio_read(argc - 1, argv + 1);
  } else if (strcmp(argv[1], "write") == 0) {
    return cmd_gpio_write(argc - 1, argv + 1);
  } else if (strcmp(argv[1], "mode") == 0) {
    return cmd_gpio_mode(argc - 1, argv + 1);
  } else {
    Serial.printf("gpio: unknown sub-command '%s'\n", argv[1]);
    return 1;
  }
}

// ---------------------------------------------------------------------------
// led <on|off> [color] — convenience command that reuses the gpio handlers
//
// When RGB_BUILTIN is defined (boards with an addressable RGB LED), an
// optional color name is accepted: red, green, blue, white, yellow.
//
// This command will reuse the gpio commands if the board does not have an RGB LED.
// Otherwise, it will use the rgbLedWrite function to set the color of the LED.
// ---------------------------------------------------------------------------

#ifdef LED_BUILTIN

#ifdef RGB_BUILTIN
struct NamedColor {
  const char *name;
  uint8_t r, g, b;
};

static const NamedColor colors[] = {
  {"red",    RGB_BRIGHTNESS, 0,              0             },
  {"green",  0,              RGB_BRIGHTNESS, 0             },
  {"blue",   0,              0,              RGB_BRIGHTNESS},
  {"white",  RGB_BRIGHTNESS, RGB_BRIGHTNESS, RGB_BRIGHTNESS},
  {"yellow", RGB_BRIGHTNESS, RGB_BRIGHTNESS, 0             },
};

static const size_t numColors = sizeof(colors) / sizeof(colors[0]);
#else
static bool ledInitialized = false;
#endif

static void printLedUsage() {
#ifdef RGB_BUILTIN
  Serial.println("Usage: led <on [color]|off>");
  Serial.println("  Colors: red, green, blue, white, yellow");
#else
  Serial.println("Usage: led <on|off>");
#endif
}

static int cmd_led(int argc, char **argv) {
  if (argc < 2) {
    printLedUsage();
    return 1;
  }

#ifdef RGB_BUILTIN
  // Read argv values BEFORE calling Console.run() — nested run() calls
  // invalidate the caller's argv pointers (shared IDF parse buffer).
  if (strcmp(argv[1], "on") == 0) {
    // If the board has an RGB LED, and a color is specified, set the LED to the specified color.
    // If no color is specified, set the LED to white.
    // This will use the rgbLedWrite function to set the color of the LED.
    const char *colorName = (argc > 2) ? argv[2] : "white";
    for (size_t i = 0; i < numColors; i++) {
      if (strcasecmp(colorName, colors[i].name) == 0) {
        rgbLedWrite(RGB_BUILTIN, colors[i].r, colors[i].g, colors[i].b);
        Serial.printf("RGB LED on: %s (%d, %d, %d)\n", colors[i].name, colors[i].r, colors[i].g, colors[i].b);
        return 0;
      }
    }
    Serial.printf("Unknown color '%s'. Available: ", colorName);
    for (size_t i = 0; i < numColors; i++) {
      Serial.printf("%s%s", colors[i].name, (i < numColors - 1) ? ", " : "\n");
    }
    return 1;
  } else if (strcmp(argv[1], "off") == 0) {
    rgbLedWrite(RGB_BUILTIN, 0, 0, 0);
    Serial.println("RGB LED off");
    return 0;
  }
#else
  // Read argv values BEFORE calling Console.run() — nested run() calls
  // invalidate the caller's argv pointers (shared IDF parse buffer).
  if (strcmp(argv[1], "on") == 0) {
    // If the board does not have an RGB LED, use the gpio commands to toggle the LED.
    // This will call the gpio commands to configure the pin as an output and set the LED to HIGH.
    if (!ledInitialized) {
      Console.run(String("gpio mode ") + LED_BUILTIN + " out");
      ledInitialized = true;
    }
    return Console.run(String("gpio write ") + LED_BUILTIN + " 1");
  } else if (strcmp(argv[1], "off") == 0) {
    if (!ledInitialized) {
      Console.run(String("gpio mode ") + LED_BUILTIN + " out");
      ledInitialized = true;
    }
    return Console.run(String("gpio write ") + LED_BUILTIN + " 0");
  }
#endif

  printLedUsage();
  return 1;
}

#endif

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  // Initialize argtable argument structures for each GPIO sub-command.
  // These structures help parse and validate command line arguments for the Console commands.
  //
  // arg_int1(shortopts, longopts, datatype, glossary)
  //   - Adds a required integer argument.
  // arg_str1(...)
  //   - Adds a required string argument.
  // arg_end(N)
  //   - Terminates the argument table and provides space for up to N error messages.
  //
  // Passing NULL for both shortopts and longopts makes arguments positional (not using flags like -x or --foo).
  // The glossary string is a description that appears in the help message.

  // Arguments for: gpio read <pin>
  gpio_read_args.pin = arg_int1(NULL, NULL, "<pin>", "GPIO pin number to read (required)");
  gpio_read_args.end = arg_end(1); // Up to 1 error message for reading.

  // Arguments for: gpio write <pin> <0|1>
  gpio_write_args.pin = arg_int1(NULL, NULL, "<pin>", "GPIO pin number to write (required)");
  gpio_write_args.value = arg_int1(NULL, NULL, "<0|1>", "Value to write: 0 = LOW, 1 = HIGH (required)");
  gpio_write_args.end = arg_end(2); // Up to 2 error messages for writing.

  // Arguments for: gpio mode <pin> <in|out|in_pu|in_pd>
  gpio_mode_args.pin = arg_int1(NULL, NULL, "<pin>", "GPIO pin number to configure (required)");
  gpio_mode_args.mode = arg_str1(NULL, NULL, "<in|out|in_pu|in_pd>", "Pin mode: in, out, in_pu, or in_pd (required)");
  gpio_mode_args.end = arg_end(2); // Up to 2 error messages for mode.

  // Configure Console prompt
  Console.setPrompt("gpio> ");

  // Initialize Console
  if (!Console.begin()) {
    Serial.println("Console init failed");
    return;
  }

  // Add commands
  Console.addCmd("gpio", "Control GPIO pins", "<read|write|mode> ...", cmd_gpio);

#ifdef LED_BUILTIN
#ifdef RGB_BUILTIN
  Console.addCmd("led", "Control RGB LED", "<on [color]|off>", cmd_led);
#else
  Console.addCmd("led", "Toggle LED_BUILTIN", "<on|off>", cmd_led);
#endif
#endif

  // Add built-in help command
  Console.addHelpCmd();

  // Begin Read-Eval-Print Loop
  Console.beginRepl();
}

void loop() {
  // Loop task is not used in this example, so we delete it to free up resources
  vTaskDelete(NULL);
}
