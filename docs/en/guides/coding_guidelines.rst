#################
Coding Guidelines
#################

This page describes the coding standards used in the Arduino ESP32 project.
Adherence to these guidelines ensures consistency, portability, and compatibility
across all supported ESP-IDF configurations and future hardware targets.

Code Formatting
===============

The project uses `clang-format <https://clang.llvm.org/docs/ClangFormat.html>`_ version 18.1.3
for automatic formatting. The configuration is stored in ``.clang-format`` at the repository root.
A pre-commit hook enforces formatting before every commit, so contributors can also run
``clang-format`` manually or rely on CI to flag violations.

The style is based on the LLVM preset with the following key rules:

Indentation and Spacing
-----------------------

- **Indent width**: 2 spaces. Tabs are never used.
- **Continuation indent**: 2 spaces for wrapped lines.
- **Column limit**: 160 characters per line.
- **Trailing comments**: 2 spaces before inline ``//`` comments.
- **Spaces before parentheses**: required after control-flow keywords (``if (``, ``for (``, ``while (``), not after function names (``foo(``).

.. code-block:: c

   // Correct
   if (condition) {
     foo(a, b);
   }

   // Wrong
   if(condition){
     foo (a, b);
   }

Braces
------

- **Style**: K&R (opening brace on the same line as the statement).
- **Mandatory braces**: ``clang-format`` inserts braces around single-statement bodies automatically (``InsertBraces: true``). Never omit braces even for single-statement ``if``/``for``/``while`` bodies.
- **Short blocks**: empty function/record bodies may be on a single line. Short ``if`` statements must always span two lines; short ``for``/``while`` loops may be on one line.

.. code-block:: c

   // Correct – braces always present
   if (error) {
     return false;
   }
   for (int i = 0; i < n; i++) continue;  // short loop – OK on one line

   // Wrong – missing braces
   if (error)
     return false;

Switch / Case
-------------

- Case labels are **indented** inside the ``switch``.
- Short ``case`` bodies may be on the same line as the label.
- When a ``case`` body requires braces (e.g., to declare a variable), the opening brace goes on the **next** line after the label (Allman style for case blocks only).

.. code-block:: c

   switch (state) {
     case STATE_IDLE:   handle_idle(); break;
     case STATE_ACTIVE:
     {
       int tmp = compute();
       handle_active(tmp);
       break;
     }
     default: break;
   }

Pointers and References
-----------------------

- Pointer ``*`` and reference ``&`` attach to the **type name**, not the variable name (``PointerAlignment: Right``).

.. code-block:: c

   // Correct
   int *ptr;
   void func(const char *buf, size_t len);

   // Wrong
   int* ptr;
   void func(const char* buf, size_t len);

Include Order
-------------

- Include order is **preserved as written** (``SortIncludes: Never``). Do not rely on automatic sorting; group and order headers intentionally.

Line Endings
------------

- All files must use **LF** (Unix) line endings (``LineEnding: LF``).
- Every file must end with a **single newline** (``InsertNewlineAtEOF: true``).

Macro Alignment
---------------

- Consecutive ``#define`` macro definitions are horizontally aligned (``AlignConsecutiveMacros``).

.. code-block:: c

   #define PIN_SCL   22
   #define PIN_SDA   21
   #define TIMEOUT   1000

Format Specifiers
=================

Arduino ESP32 supports three C standard library configurations:

- **NEWLIB full** – supports all C99 format specifiers including ``%hh``, ``%h``, ``%z``, ``%ll``.
- **NEWLIB-nano** – a size-optimised subset; supports only C89 specifiers. Does **not** support ``%hh``, ``%h``, ``%z``, or ``%ll``.
- **PICOLIBC** – supports C99 specifiers by default.

The project must also remain forward-compatible with future 64-bit RISC-V targets (LP64 model, where ``long`` and pointers are 64-bit).

Use the rules in the table below to select the correct format specifier for each type.

.. list-table::
   :header-rows: 1
   :widths: 25 35 40

   * - C type
     - Correct specifier
     - Notes
   * - ``uint8_t`` / ``int8_t``
     - ``%u`` / ``%d``
     - Integer promotion to ``int`` makes these safe everywhere. Do **not** use ``PRIu8`` / ``PRId8``.
   * - ``uint16_t`` / ``int16_t``
     - ``%u`` / ``%d``
     - Same reason. Do **not** use ``PRIu16`` / ``PRId16``.
   * - ``uint8_t`` / ``uint16_t`` (hex)
     - ``%x`` / ``%X``
     - Use width flags as needed: ``%02x``, ``%04X``. Do **not** use ``PRIx8`` / ``PRIX16``.
   * - ``uint32_t``
     - ``"%" PRIu32``
     - **Must** use the PRI macro; expands to ``"u"`` or ``"lu"`` depending on the library.
   * - ``int32_t``
     - ``"%" PRId32``
     - Same rule.
   * - ``uint32_t`` (hex)
     - ``"%" PRIx32`` / ``"%" PRIX32``
     - Same rule.
   * - ``uint64_t``
     - ``"%s"``
     - Use ``u64_to_str(value)`` or ``u64_to_String(value)`` from ``StringUtils.h``. In sketches you can also use ``Serial.print(value)``.
   * - ``int64_t``
     - ``"%s"``
     - Use ``i64_to_str(value)`` or ``i64_to_String(value)`` from ``StringUtils.h``. In sketches you can also use ``Serial.print(value)``.
   * - ``size_t``
     - ``%lu`` with ``(unsigned long)`` cast
     - ``%zu`` is **not** supported on NEWLIB-nano. Cast to ``(unsigned long)`` before printing: ``printf("%lu", (unsigned long)len)``. On ILP32 this is a no-op; on LP64 ``unsigned long`` matches ``size_t``.
   * - ``unsigned long`` / ``long``
     - ``%lu`` / ``%ld``
     - Correct as-is; no cast needed.
   * - ``int`` / ``unsigned int``
     - ``%d`` / ``%u`` / ``%x`` / ``%X``
     - Correct as-is.

.. note::

   ``#include <inttypes.h>`` is required for the ``PRIu32`` / ``PRId32`` / ``PRIx32`` etc. macros.
   In most Arduino ESP32 files this is provided transitively by ``Arduino.h``, ``esp32-hal.h``,
   or ``esp32-hal-log.h`` (which includes ``esp_log.h`` → ``inttypes.h``). For standalone C files
   that do not include any of those headers, add ``#include <inttypes.h>`` explicitly.

Printing Enums
--------------

The correct specifier for an enum depends on its **underlying type**, not on the range of its values.

**C enums and C++ unscoped enums**

The underlying type is always ``int`` (signed 32-bit). Always use ``%d``, even when all values are non-negative.

.. code-block:: c

   typedef enum { STATE_IDLE, STATE_RUNNING, STATE_ERROR } state_t;
   state_t state = STATE_RUNNING;
   log_d("State: %d", state);

   // Wrong – enums are int, not long
   log_d("State: %ld", state);

**C++ scoped enums (enum class)**

Scoped enums do **not** implicitly convert to integers. An explicit cast is required. The specifier must match the underlying type of the cast.

.. code-block:: cpp

   // Default underlying type: int
   enum class Color { Red, Green, Blue };
   Color c = Color::Green;
   log_d("Color: %d", (int)c);

   // Explicit underlying type: uint8_t (promoted to int, %u is safe)
   enum class Pin : uint8_t { SCL = 22, SDA = 21 };
   Pin p = Pin::SCL;
   log_d("Pin: %u", (uint8_t)p);

   // Explicit underlying type: uint32_t
   enum class Flags : uint32_t { None = 0, Read = 1, Write = 2 };
   Flags f = Flags::Read;
   log_d("Flags: %" PRIu32, (uint32_t)f);

**ESP-IDF enums**

ESP-IDF types such as ``esp_err_t``, ``arduino_event_id_t``, and driver event types are C enums
(underlying type ``int``). Use ``%d``. Do **not** use ``%ld`` or ``"%" PRId32`` — they are ``int``,
not ``int32_t`` or ``long``.

.. code-block:: c

   esp_err_t err = some_function();
   log_e("Error: %d (%s)", err, esp_err_to_name(err));

   // Wrong
   log_e("Error: %ld", err);

**Key rule**: always match the specifier to the *actual underlying type* of the enum value being
printed. When in doubt, cast explicitly and use the specifier that corresponds to the cast type.

Examples
--------

.. code-block:: c

  #include <StringUtils.h>

   uint8_t  pin      = 5;
   uint16_t addr     = 0x1234;
   uint32_t baudrate = 115200;
   uint64_t uptime   = 1000000000ULL;
   size_t   len      = data.size();
   int      err      = -1;

   char buffer[U64_STR_SIZE];

   // Correct
   log_d("pin=%u addr=0x%04x baud=%" PRIu32 " up=%llu len=%lu err=%d",
         pin, addr, baudrate, uptime, (unsigned long)len, err);

   // Wrong – uses PRIu8/PRIu16 (breaks NEWLIB-nano), %zu and %llu (breaks NEWLIB-nano),
   //         and %lu for uint32_t (wrong on PICOLIBC/LP64)
   log_d("pin=%" PRIu8 " addr=0x%04" PRIu16 " baud=%lu up=%s len=%zu err=%d",
         pin, addr, baudrate, u64_to_str(uptime, buffer), len, err);
