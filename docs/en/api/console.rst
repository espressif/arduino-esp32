#######
Console
#######

About
-----

The Console library provides an interactive command-line interface (CLI) for ESP32 applications.
It wraps the ESP-IDF ``console`` component and exposes an Arduino-style API for registering
commands, starting a background REPL (Read-Eval-Print Loop) task, managing command history, and
parsing typed arguments using the argtable3 library.

Features include:

- Background REPL task with configurable stack, priority, and core affinity.
- Tab completion and inline hints powered by the linenoise library.
- Persistent command history saved to LittleFS or SPIFFS.
- Typed argument parsing via argtable3 (integers, strings, optional arguments, etc.).
- Context-aware command callbacks for object-oriented designs.
- Transport-agnostic: works automatically with UART ``Serial`` and
  HWCDC / USB-JTAG (``HWCDCSerial``) — the same transport that ``Serial`` uses is detected
  automatically from the ``ARDUINO_USB_MODE`` / ``ARDUINO_USB_CDC_ON_BOOT`` build flags.

.. note::

   The Console REPL currently does **not** support USB OTG (CDC via TinyUSB / ``USBSerial``).
   On boards where USB OTG is selected as the ``Serial`` transport, the REPL will not
   receive input. Use UART or HWCDC instead.

Note that some features like history and tab completion are only available on terminals that
support VT100 escape sequences. The Arduino IDE Serial Monitor does not support them.
If needed, you can use any other terminal that supports VT100 escape sequences, such as
PuTTY, Minicom or Picocom. If your terminal prints weird sequences of characters like ``[5n``,
it means that the terminal does not support VT100 escape sequences.
To disable VT100 escape sequences, you can use the ``setPlainMode()`` method and use a plain text mode instead.

The Console library is usable by all ESP32 variants. Depending on how the board USB and Serial are implemented, you may
need to ensure that USB CDC is enabled on boot and use the Hardware CDC. For example, on the most recent ESP32-P4 revisions,
you need to enable ``USB CDC On Boot`` and set the USB Mode to ``Hardware CDC and JTAG``.

Header File
-----------

.. code-block:: arduino

    #include <Console.h>

..

Overview
--------

The library exposes a single global object ``Console`` of type ``ConsoleClass``.

Typical usage:

1. (Optional) Call configuration methods such as ``setPrompt()``, ``setMaxHistory()``, and
   ``setHistoryFile()`` **before** calling ``begin()``.
2. Call ``Console.begin()`` to initialise the underlying ``esp_console`` module.
3. Register commands with ``Console.addCmd()`` or ``Console.addCmdWithContext()``.
4. Call ``Console.addHelpCmd()`` to register the built-in ``help`` command.
5. Call ``Console.beginRepl()`` to start the interactive session in a background task.

Alternatively, skip ``beginRepl()`` and call ``Console.run()`` manually from your own input loop
(see the ``ConsoleManual`` example).

Two function-pointer typedefs are provided for command callbacks:

.. code-block:: arduino

    // Simple callback
    typedef int (*ConsoleCommandFunc)(int argc, char **argv);

    // Context-aware callback (receives a user-supplied pointer as first argument)
    typedef int (*ConsoleCommandFuncWithCtx)(void *context, int argc, char **argv);

..

Both argtable3 (``<console/argtable3/argtable3.h>``) and linenoise
(``<console/linenoise/linenoise.h>``) headers are included by ``Console.h`` and are directly
available in your sketch.


Argument Parsing with argtable3
-------------------------------

The Console library re-exports the `argtable3 <https://www.argtable.org/>`_ C library, which
provides GNU-style argument parsing for your commands.  Instead of manually parsing ``argv``
strings, you declare typed argument descriptors and let argtable handle validation, error
messages, and automatic hint generation.

Declaring Arguments
*******************

Each argument is created by calling a constructor function.  The two most common variants
are:

- ``arg_xxx1(...)`` — **required** argument (exactly 1 occurrence).
- ``arg_xxx0(...)`` — **optional** argument (0 or 1 occurrence).

Available types:

.. list-table::
   :header-rows: 1
   :widths: 25 25 50

   * - Constructor
     - Value field
     - Description
   * - ``arg_int0`` / ``arg_int1``
     - ``->ival[0]``
     - Integer (decimal, hex ``0x``, or octal ``0`` prefix)
   * - ``arg_dbl0`` / ``arg_dbl1``
     - ``->dval[0]``
     - Double-precision floating point
   * - ``arg_str0`` / ``arg_str1``
     - ``->sval[0]``
     - String (no whitespace unless quoted)
   * - ``arg_lit0`` / ``arg_lit1``
     - ``->count``
     - Boolean flag (no value — presence means *true*)
   * - ``arg_end``
     - *(none)*
     - Sentinel that terminates the table and stores parse errors

Every constructor takes four parameters:

.. code-block:: c

    struct arg_xxx* arg_xxx1(
        const char *shortopts,   // Short option letter, e.g. "p" for -p   (or NULL)
        const char *longopts,    // Long option name, e.g. "pin" for --pin (or NULL)
        const char *datatype,    // Placeholder shown in hints, e.g. "<pin>"
        const char *glossary     // Description shown in help text
    );

When both ``shortopts`` and ``longopts`` are ``NULL``, the argument is **positional** (matched
by order, not by a flag).

Building an Argument Table
**************************

Group all argument descriptors into a ``struct`` that ends with ``arg_end``:

.. code-block:: arduino

    // Declare a global struct for the argument table
    static struct {
      struct arg_int *pin;     // positional, required: GPIO pin number
      struct arg_int *value;   // positional, required: 0 or 1
      struct arg_end *end;     // must always be last — tracks parse errors
    } my_args;

    void setup() {
      // Allocate and initialise each argument descriptor.
      // arg_int1: required int arg.  (NULL, NULL) = positional (no -x/--xx flag).
      my_args.pin   = arg_int1(NULL, NULL, "<pin>", "GPIO pin number");
      my_args.value = arg_int1(NULL, NULL, "<0|1>", "0 = LOW, 1 = HIGH");
      // arg_end(N): sentinel — N is the max number of errors to store.
      my_args.end   = arg_end(2);

      // Register the command, passing &my_args as the argtable.
      // The Console library auto-generates the hint string from the table.
      Console.addCmd("gpio", "Write a GPIO pin", &my_args, cmd_gpio);
    }

Parsing Inside a Command Handler
*********************************

Inside the command callback, call ``arg_parse()`` to validate the arguments and fill in the
values:

.. code-block:: arduino

    static int cmd_gpio(int argc, char **argv) {
      // Parse argv against the argument table
      int nerrors = arg_parse(argc, argv, (void **)&my_args);
      if (nerrors != 0) {
        // Print human-readable error messages to stdout
        arg_print_errors(stdout, my_args.end, argv[0]);
        return 1;
      }

      // Access parsed values through the typed fields
      int pin = my_args.pin->ival[0];   // first (and only) int value
      int val = my_args.value->ival[0];

      pinMode(pin, OUTPUT);
      digitalWrite(pin, val ? HIGH : LOW);
      printf("GPIO %d = %d\n", pin, val);
      return 0;
    }

Optional Arguments and Flags
*****************************

Use ``arg_xxx0`` for optional parameters, and check ``->count`` to see if they were supplied:

.. code-block:: arduino

    static struct {
      struct arg_str *ssid;      // required positional string
      struct arg_str *password;  // optional positional string
      struct arg_lit *verbose;   // optional flag: -v / --verbose
      struct arg_end *end;
    } connect_args;

    void setup() {
      connect_args.ssid     = arg_str1(NULL, NULL, "<ssid>", "Network name");
      connect_args.password = arg_str0(NULL, NULL, "[<password>]", "Password (optional)");
      connect_args.verbose  = arg_lit0("v", "verbose", "Print extra info");
      connect_args.end      = arg_end(3);
      // ...
    }

    static int cmd_connect(int argc, char **argv) {
      int nerrors = arg_parse(argc, argv, (void **)&connect_args);
      if (nerrors != 0) {
        arg_print_errors(stdout, connect_args.end, argv[0]);
        return 1;
      }

      const char *ssid = connect_args.ssid->sval[0];

      // Check if the optional password was supplied
      const char *pass = (connect_args.password->count > 0)
                         ? connect_args.password->sval[0]
                         : "";

      // Check if the -v / --verbose flag was present
      bool verbose = (connect_args.verbose->count > 0);

      // ...
      return 0;
    }

See the ``ConsoleGPIO`` and ``ConsoleWiFi`` examples for complete working sketches.


Arduino-esp32 Console API
--------------------------

``begin``
*********

   Initialise the console module.  Must be called before any other method.

   .. code-block:: arduino

       bool begin(size_t maxCmdLen = 256, size_t maxArgs = 32)

   ..

   **Parameters**

      * ``maxCmdLen`` (Optional)
         - Maximum length of a single command line in bytes. Default: ``256``.

      * ``maxArgs`` (Optional)
         - Maximum number of whitespace-separated tokens on a line. Default: ``32``.

   **Returns**
      * ``true`` on success; ``false`` if ``esp_console_init()`` fails.

   **Notes**
      * Sets up linenoise tab-completion and hints automatically.
      * Loads history from the file set by ``setHistoryFile()`` if one was configured.
      * Calling ``begin()`` again after it has already succeeded has no effect.


``end``
*******

   De-initialise the console module.

   .. code-block:: arduino

       void end()

   ..

   **Notes**
      * Stops the REPL task if running.
      * Saves command history to the configured file before freeing resources.


``setPrompt``
*************

   Set the prompt string displayed at the start of each input line.

   .. code-block:: arduino

       void setPrompt(const char *prompt)
       void setPrompt(const String &prompt)

   ..

   **Parameters**
      * ``prompt`` — Prompt string. Default: ``"esp> "``.

   **Notes**
      * Must be called before ``begin()`` to take effect in the REPL task.


``setMaxHistory``
*****************

   Set the maximum number of command history entries kept in RAM.

   .. code-block:: arduino

       void setMaxHistory(uint32_t maxLen)

   ..

   **Parameters**
      * ``maxLen`` — Maximum history entries. Default: ``32``.


``setHistoryFile``
******************

   Set a filesystem and file path for persistent command history.

   .. code-block:: arduino

       void setHistoryFile(const char *path, fs::FS &fs = LittleFS)
       void setHistoryFile(const String &path, fs::FS &fs = LittleFS)

   ..

   The method combines ``fs.mountpoint()`` and ``path`` internally to build the full VFS path
   used by linenoise for loading and saving history.  You only need to pass the
   filesystem-relative path.

   **Parameters**
      * ``path`` — File path relative to the filesystem root, e.g. ``"/history.txt"``.

      * ``fs`` — A mounted Arduino filesystem object. Default: ``LittleFS``.
        Also accepts ``SPIFFS``, ``FFat``, etc.

   **Notes**
      * History is loaded from this file at ``begin()`` and saved after every command.
      * The filesystem must be mounted before ``begin()`` is called.

   **Example**

      .. code-block:: arduino

          LittleFS.begin(true);
          Console.setHistoryFile("/history.txt");  // uses LittleFS by default

          // Or with a different filesystem:
          // FFat.begin(true);
          // Console.setHistoryFile("/history.txt", FFat);


``setTaskStackSize``
********************

   Set the REPL background task stack size in bytes.

   .. code-block:: arduino

       void setTaskStackSize(uint32_t size)

   ..

   **Parameters**
      * ``size`` — Stack size in bytes. Default: ``4096``.


``setTaskPriority``
*******************

   Set the REPL background task priority.

   .. code-block:: arduino

       void setTaskPriority(uint32_t priority)

   ..

   **Parameters**
      * ``priority`` — FreeRTOS task priority. Default: ``2``.


``setTaskCore``
***************

   Pin the REPL task to a specific CPU core.

   .. code-block:: arduino

       void setTaskCore(BaseType_t core)

   ..

   **Parameters**
      * ``core`` — Core index (``0`` or ``1``) or ``tskNO_AFFINITY``. Default: ``tskNO_AFFINITY``.


``usePsram``
************

   Route all Console PSRAM-eligible allocations to external SPI RAM.

   .. code-block:: arduino

       void usePsram(bool enable)

   ..

   When enabled:

   - The ``esp_console`` command registry (``heap_alloc_caps`` in ``esp_console_config_t``) is
     allocated from PSRAM.
   - The REPL FreeRTOS task stack is allocated from PSRAM via
     ``xTaskCreatePinnedToCoreWithCaps()``.

   This frees internal SRAM for other uses at the cost of slightly higher latency for stack
   and heap accesses.  Has no effect if PSRAM is not available or not initialised at boot.

   **Parameters**
      * ``enable`` — ``true`` to use PSRAM, ``false`` for internal RAM (default).

   **Notes**
      * Must be called before ``begin()`` to affect heap allocation, and before ``beginRepl()``
        to affect the task stack.

   **Example**

      .. code-block:: arduino

          Console.usePsram(true);  // before begin()
          Console.begin();
          Console.beginRepl();

      ..


``addCmd``
**********

   Register a command.  Three overloads are available depending on how the hint is provided.

   .. code-block:: arduino

       // Auto-generated hint (no hint / hint from argtable)
       bool addCmd(const char *name, const char *help, ConsoleCommandFunc func)
       bool addCmd(const String &name, const String &help, ConsoleCommandFunc func)

       // Explicit hint string
       bool addCmd(const char *name, const char *help, const char *hint, ConsoleCommandFunc func)
       bool addCmd(const String &name, const String &help, const String &hint, ConsoleCommandFunc func)

       // argtable3 argument table (hint auto-generated from the table)
       bool addCmd(const char *name, const char *help, void *argtable, ConsoleCommandFunc func)

   ..

   **Parameters**

      * ``name`` (Required)
         - Command name as typed at the prompt. Must not contain spaces. The pointer must
           remain valid until ``end()`` is called.

      * ``help`` (Required)
         - Help text shown by the built-in ``help`` command.

      * ``hint`` (Optional)
         - Short argument synopsis shown inline while typing, e.g. ``"<pin> <value>"``.

      * ``argtable`` (Optional)
         - Pointer to an argtable3 argument table (a struct ending with ``arg_end``). The hint
           is generated automatically from the table. The table is only read during registration.

      * ``func`` (Required)
         - Command handler. Receives ``argc`` / ``argv`` where ``argv[0]`` is the command name.
           Return ``0`` for success, non-zero for failure.

   **Returns**
      * ``true`` on success.


``addCmdWithContext``
*********************

   Register a context-aware command.

   .. code-block:: arduino

       bool addCmdWithContext(const char *name, const char *help,
                              ConsoleCommandFuncWithCtx func, void *ctx)
       bool addCmdWithContext(const String &name, const String &help,
                              ConsoleCommandFuncWithCtx func, void *ctx)

       bool addCmdWithContext(const char *name, const char *help, const char *hint,
                              ConsoleCommandFuncWithCtx func, void *ctx)
       bool addCmdWithContext(const String &name, const String &help, const String &hint,
                              ConsoleCommandFuncWithCtx func, void *ctx)

       bool addCmdWithContext(const char *name, const char *help, void *argtable,
                              ConsoleCommandFuncWithCtx func, void *ctx)

   ..

   **Parameters**

      * ``func`` — Callback of type ``ConsoleCommandFuncWithCtx``. The ``context`` pointer is
        passed as the first argument on every call.

      * ``ctx`` — Arbitrary pointer forwarded to ``func`` as its first argument.

      All other parameters are the same as ``addCmd``.

   **Returns**
      * ``true`` on success.


``removeCmd``
*************

   Unregister a previously registered command.

   .. code-block:: arduino

       bool removeCmd(const char *name)
       bool removeCmd(const String &name)

   ..

   **Parameters**
      * ``name`` — Name of the command to remove.

   **Returns**
      * ``true`` on success; ``false`` if the command was not found.


``addHelpCmd``
**************

   Register the built-in ``help`` command.

   .. code-block:: arduino

       bool addHelpCmd()

   ..

   When called with no arguments, ``help`` lists all registered commands with their hints and
   help text.  When called with a command name (e.g. ``help gpio``), it prints the details for
   that command only.

   **Returns**
      * ``true`` on success.


``removeHelpCmd``
*****************

   Remove the built-in ``help`` command.

   .. code-block:: arduino

       bool removeHelpCmd()

   ..

   **Returns**
      * ``true`` on success.


``setHelpVerboseLevel``
***********************

   Control the verbosity of the built-in ``help`` command.

   .. code-block:: arduino

       bool setHelpVerboseLevel(int level)

   ..

   **Parameters**
      * ``level``
         - ``0`` — Brief: show command name and hint only.
         - ``1`` — Verbose: also show the full help text.

   **Returns**
      * ``true`` on success; ``false`` if ``level`` is out of range.


``beginRepl``
*************

   Start the REPL in a background FreeRTOS task.

   .. code-block:: arduino

       bool beginRepl()

   ..

   The task reads input through ``Serial`` (bypassing VFS ``stdin`` to avoid a race condition
   with the UART/USB driver ISR) and passes each line to ``esp_console_run()``.  Non-empty
   entries are added to the command history.  Because reading goes through ``Serial``, no
   transport-specific configuration is required — UART and HWCDC work automatically.

   .. note::

      USB OTG (CDC via TinyUSB / ``USBSerial``) is **not** currently supported as a REPL
      transport.

   At startup the task probes the terminal for VT100 support by sending a one-time Device
   Status Request (``ESC[5n``).  If the terminal does not respond within 500 ms, plain mode
   is enabled automatically.  Use ``setPlainMode()`` before ``beginRepl()`` to override the
   probe result.

   **Returns**
      * ``true`` on success.

   **Notes**
      * ``begin()`` must be called before ``beginRepl()``.
      * Calling ``beginRepl()`` a second time has no effect.


``stopRepl``
************

   Stop the REPL background task.

   .. code-block:: arduino

       bool stopRepl()

   ..

   **Returns**
      * ``true`` on success.


``run``
*******

   Execute a command line string directly without a REPL task.

   .. code-block:: arduino

       int run(const char *cmdline)
       int run(const String &cmdline)

   ..

   **Parameters**
      * ``cmdline`` — Full command line (command name followed by arguments).

   **Returns**
      * The command handler's return code (``0`` = success).
      * ``-1`` if the command was not found or an ``esp_console`` error occurred.

   **Notes**
      * ``run()`` can be called from inside a command handler to invoke other commands
        (command composition).  However, the underlying IDF function shares a single
        parse buffer, so the caller's ``argv`` pointers are **invalidated** after the
        call returns.  Copy any ``argv`` values you need into local variables **before**
        calling ``run()``.

        .. code-block:: arduino

            static int cmd_led(int argc, char **argv) {
              // Read argv BEFORE calling run() — it becomes invalid after.
              bool turnOn = (strcmp(argv[1], "on") == 0);

              if (turnOn) {
                Console.run("gpio write 2 1");
              }
              // Do NOT use argv[1] here.
              return 0;
            }

      * History is **not** updated by ``run()``. Manage history manually when building a
        custom input loop (see the ``ConsoleManual`` example).


``clearScreen``
***************

   Clear the terminal screen.

   .. code-block:: arduino

       void clearScreen()

   ..


``setMultiLine``
****************

   Enable or disable multi-line editing mode.

   .. code-block:: arduino

       void setMultiLine(bool enable)

   ..

   **Parameters**
      * ``enable`` — ``true`` to allow the input line to wrap across multiple terminal rows.
        Disabled by default.


``setPlainMode``
****************

   Enable or disable plain (non-VT100) input mode.

   .. code-block:: arduino

       void setPlainMode(bool enable, bool force = true)

   ..

   In plain mode linenoise falls back to basic line input without cursor movement, arrow-key
   history navigation, or tab-completion rendering.  Useful for terminals that do not support
   ANSI/VT100 escape sequences (such as the Arduino IDE Serial Monitor).

   By default, ``beginRepl()`` probes the terminal at startup (sending a one-time Device
   Status Request) and enables plain mode automatically if the terminal does not respond.

   **Parameters**
      * ``enable`` — ``true`` to enable plain mode, ``false`` to restore VT100 mode.

      * ``force`` — When ``true`` (default), the automatic VT100 probe at ``beginRepl()``
        startup is skipped and the mode set here is kept unconditionally.  When ``false``,
        the mode is applied immediately but ``beginRepl()`` may still override it based on
        the probe result.

   **Notes**
      * This wraps linenoise's internal "dumb mode", a legacy term from the era of
        character-only display terminals.  The Arduino API uses the friendlier name.


``isPlainMode``
***************

   Check whether plain (non-VT100) input mode is currently active.

   .. code-block:: arduino

       bool isPlainMode()

   ..

   Returns ``true`` if plain mode is enabled, either because it was set explicitly with
   ``setPlainMode(true)`` or because the automatic VT100 probe at ``beginRepl()`` startup
   detected a non-VT100 terminal.

   **Returns**
      * ``true`` if plain mode is active, ``false`` otherwise.


``splitArgv``
*************

   Split a command line string into ``argv``-style tokens in place.

   .. code-block:: arduino

       static size_t Console.splitArgv(char *line, char **argv, size_t argv_size)

   ..

   Handles quoted strings (spaces preserved, quotes stripped) and backslash escapes.
   Modifies the input buffer.

   **Parameters**

      * ``line`` — Null-terminated input string, modified in place.

      * ``argv`` — Output array; each element will point into ``line``.

      * ``argv_size`` — Size of the ``argv`` array.  At most ``argv_size - 1`` tokens are
        returned; ``argv[argc]`` is always set to ``NULL``.

   **Returns**
      * Number of tokens found (``argc``).

   **Example**

      .. code-block:: arduino

          char line[] = "gpio write 5 1";
          char *argv[8];
          size_t argc = Console.splitArgv(line, argv, 8);
          // argc == 4, argv[0]=="gpio", argv[1]=="write", argv[2]=="5", argv[3]=="1"

      ..


Examples
--------

ConsoleREPL
***********

Full interactive REPL with history, tab completion, and inline hints.

.. literalinclude:: ../../../libraries/Console/examples/ConsoleREPL/ConsoleREPL.ino
    :language: arduino

ConsoleManual
*************

Manual command execution from a custom input loop in ``loop()``, without a background task.

.. literalinclude:: ../../../libraries/Console/examples/ConsoleManual/ConsoleManual.ino
    :language: arduino

ConsoleSysInfo
**************

System information commands: ``uptime``, ``heap``, ``tasks``, ``version``, ``restart``.

.. literalinclude:: ../../../libraries/Console/examples/ConsoleSysInfo/ConsoleSysInfo.ino
    :language: arduino

ConsoleFS
*********

File system CLI over LittleFS: ``ls``, ``cat``, ``rm``, ``mkdir``, ``write``, ``df``.

.. literalinclude:: ../../../libraries/Console/examples/ConsoleFS/ConsoleFS.ino
    :language: arduino

ConsoleGPIO
***********

GPIO control with argtable3 argument parsing: ``gpio read``, ``gpio write``, ``gpio mode``.

.. literalinclude:: ../../../libraries/Console/examples/ConsoleGPIO/ConsoleGPIO.ino
    :language: arduino

ConsoleAdvanced
***************

Demonstrates advanced APIs: context-aware commands (``addCmdWithContext``),
dynamic command registration and removal (``removeCmd``), REPL pause/resume
(``stopRepl`` / ``beginRepl``), help verbosity (``setHelpVerboseLevel``),
``clearScreen``, ``setPlainMode``, ``setMultiLine``, and task tuning
(``setTaskStackSize``, ``setTaskPriority``, ``setTaskCore``).

.. literalinclude:: ../../../libraries/Console/examples/ConsoleAdvanced/ConsoleAdvanced.ino
    :language: arduino

ConsoleHistory
**************

Persistent command history across reboots using LittleFS (``setHistoryFile``),
PSRAM allocation (``usePsram``), and the ``splitArgv`` tokenising utility.

.. literalinclude:: ../../../libraries/Console/examples/ConsoleHistory/ConsoleHistory.ino
    :language: arduino

ConsoleWiFi
***********

Wi-Fi management: ``wifi scan``, ``wifi connect``, ``wifi disconnect``, ``wifi status``,
``wifi ping``.

.. literalinclude:: ../../../libraries/Console/examples/ConsoleWiFi/ConsoleWiFi.ino
    :language: arduino
