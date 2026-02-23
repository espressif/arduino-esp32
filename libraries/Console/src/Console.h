/*
 * Console Arduino Library — wrapper for the ESP-IDF console component
 *
 * Provides an Arduino-style API for interactive command-line consoles with
 * full REPL support, tab completion, command history, argtable3 argument
 * parsing, and context-aware commands.
 *
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "Arduino.h"
#include "FS.h"
#include "LittleFS.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_console.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"

/**
 * @brief Callback type for a console command.
 *
 * @param argc  Number of arguments (including the command name at argv[0]).
 * @param argv  Array of null-terminated argument strings.
 * @return      0 on success, non-zero on error.
 */
typedef int (*ConsoleCommandFunc)(int argc, char **argv);

/**
 * @brief Callback type for a context-aware console command.
 *
 * @param context  User-supplied context pointer passed at registration.
 * @param argc     Number of arguments.
 * @param argv     Array of null-terminated argument strings.
 * @return         0 on success, non-zero on error.
 */
typedef int (*ConsoleCommandFuncWithCtx)(void *context, int argc, char **argv);

class ConsoleClass {
public:
  ConsoleClass();

  // ---------------------------------------------------------------------------
  // Lifecycle
  // ---------------------------------------------------------------------------

  /**
   * @brief Initialise the console.
   *
   * Must be called before any other method.  Sets up the esp_console module and
   * configures linenoise tab-completion and hints.
   *
   * @param maxCmdLen  Maximum length of a single command line (bytes). Default 256.
   * @param maxArgs    Maximum number of whitespace-separated tokens per line. Default 32.
   * @return           true on success.
   */
  bool begin(size_t maxCmdLen = 256, size_t maxArgs = 32);

  /**
   * @brief De-initialise the console.
   *
   * Stops the REPL task if running, saves history, and releases esp_console resources.
   */
  void end();

  // ---------------------------------------------------------------------------
  // Configuration — call before begin()
  // ---------------------------------------------------------------------------

  /** @brief Set the REPL prompt string. Default: "esp> ". */
  void setPrompt(const char *prompt);
  void setPrompt(const String &prompt) {
    setPrompt(prompt.c_str());
  }

  /** @brief Set the maximum number of history entries kept in RAM. Default: 32. */
  void setMaxHistory(uint32_t maxLen);

  /**
   * @brief Set a filesystem and path for persistent command history.
   *
   * When set, history is loaded at begin() and saved after every command.
   * The filesystem must be mounted before calling begin().
   *
   * @param path  File path relative to the filesystem root (e.g. "/history.txt").
   * @param fs    Arduino filesystem object. Default: LittleFS. Also accepts SPIFFS, FFat, etc.
   */
  void setHistoryFile(const char *path, fs::FS &fs = LittleFS);
  void setHistoryFile(const String &path, fs::FS &fs = LittleFS) {
    setHistoryFile(path.c_str(), fs);
  }

  /** @brief Set the REPL background task stack size in bytes. Default: 4096. */
  void setTaskStackSize(uint32_t size);

  /** @brief Set the REPL background task priority. Default: 2. */
  void setTaskPriority(uint32_t priority);

  /** @brief Pin the REPL task to a specific core. Default: tskNO_AFFINITY. */
  void setTaskCore(BaseType_t core);

  /**
   * @brief Route all Console PSRAM-eligible allocations to external SPI RAM.
   *
   * When enabled:
   * - The esp_console command registry (@c heap_alloc_caps in
   *   @c esp_console_config_t) is allocated from PSRAM.
   * - The REPL FreeRTOS task stack is allocated from PSRAM via
   *   @c xTaskCreatePinnedToCoreWithCaps.
   *
   * This frees internal SRAM for other uses at the cost of slightly higher
   * latency for stack and heap accesses.  If PSRAM is not available at
   * runtime, the library automatically falls back to internal RAM.
   *
   * Must be called before begin() to affect the heap allocation, and before
   * beginRepl() to affect the task stack.
   *
   * @param enable  true to use PSRAM, false for internal RAM (default).
   */
  void usePsram(bool enable);

  // ---------------------------------------------------------------------------
  // Command registration
  // ---------------------------------------------------------------------------

  /**
   * @brief Register a command with a simple callback.
   *
   * @param name  Command name (no spaces). Must remain valid until end().
   * @param help  Help text shown by the built-in "help" command.
   * @param func  Command handler.
   * @return      true on success.
   */
  bool addCmd(const char *name, const char *help, ConsoleCommandFunc func);
  bool addCmd(const String &name, const String &help, ConsoleCommandFunc func) {
    return addCmd(name.c_str(), help.c_str(), func);
  }

  /**
   * @brief Register a command with an explicit hint string.
   *
   * @param hint  Short argument hint shown inline during typing (e.g. "<pin> <value>").
   */
  bool addCmd(const char *name, const char *help, const char *hint, ConsoleCommandFunc func);
  bool addCmd(const String &name, const String &help, const String &hint, ConsoleCommandFunc func) {
    return addCmd(name.c_str(), help.c_str(), hint.c_str(), func);
  }

  /**
   * @brief Register a command with an argtable3 argument table.
   *
   * The argtable is used to auto-generate the hint string.  Declare the table
   * as a struct ending with arg_end() and pass a pointer to it.
   *
   * @param argtable  Pointer to an argtable3 argument table (ends with arg_end).
   */
  bool addCmd(const char *name, const char *help, void *argtable, ConsoleCommandFunc func);

  /**
   * @brief Register a context-aware command.
   *
   * @param ctx  Pointer passed verbatim as the first argument to func.
   */
  bool addCmdWithContext(const char *name, const char *help, ConsoleCommandFuncWithCtx func, void *ctx);
  bool addCmdWithContext(const String &name, const String &help, ConsoleCommandFuncWithCtx func, void *ctx) {
    return addCmdWithContext(name.c_str(), help.c_str(), func, ctx);
  }

  bool addCmdWithContext(const char *name, const char *help, const char *hint, ConsoleCommandFuncWithCtx func, void *ctx);
  bool addCmdWithContext(const String &name, const String &help, const String &hint, ConsoleCommandFuncWithCtx func, void *ctx) {
    return addCmdWithContext(name.c_str(), help.c_str(), hint.c_str(), func, ctx);
  }

  bool addCmdWithContext(const char *name, const char *help, void *argtable, ConsoleCommandFuncWithCtx func, void *ctx);

  /**
   * @brief Unregister a previously registered command.
   *
   * @param name  Command name to remove.
   * @return      true on success.
   */
  bool removeCmd(const char *name);
  bool removeCmd(const String &name) {
    return removeCmd(name.c_str());
  }

  // ---------------------------------------------------------------------------
  // Built-in help command
  // ---------------------------------------------------------------------------

  /**
   * @brief Register the built-in "help" command.
   *
   * When called with no arguments, prints all registered commands with their
   * hints and help text.  When called with a command name, prints help for
   * that command only.
   *
   * @return true on success.
   */
  bool addHelpCmd();

  /** @brief Remove the built-in "help" command. */
  bool removeHelpCmd();

  /**
   * @brief Control how much detail the "help" command prints.
   *
   * @param level  0 = brief (name + hint only), 1 = verbose (includes help text).
   * @return       true on success.
   */
  bool setHelpVerboseLevel(int level);

  // ---------------------------------------------------------------------------
  // REPL — background FreeRTOS task
  // ---------------------------------------------------------------------------

  /**
   * @brief Start the REPL in a background FreeRTOS task.
   *
   * Works with UART and HWCDC automatically: the REPL reads input through
   * @c Serial and writes output to @c stdout. No transport-specific
   * configuration is required.
   *
   * @note USB OTG (CDC via TinyUSB / USBSerial) is not currently supported.
   *
   * At startup the task probes the terminal for VT100 support (by sending a
   * one-time Device Status Request).  If the terminal does not respond, plain
   * mode is enabled automatically.  Use setPlainMode() to override the probe.
   *
   * Requires begin() to have been called first.
   *
   * @return true on success.
   */
  bool beginRepl();

  /**
   * @brief Stop the REPL task.
   *
   * @return true on success.
   */
  bool stopRepl();

  // ---------------------------------------------------------------------------
  // Manual command execution (no REPL task)
  // ---------------------------------------------------------------------------

  /**
   * @brief Execute a command line string directly.
   *
   * Useful when building a custom input loop instead of using beginRepl(),
   * or for invoking commands from within other command handlers.
   *
   * @note When calling run() from inside a command handler, the caller's
   *       @c argv pointers are invalidated because the underlying IDF
   *       function shares a single parse buffer.  Copy any @c argv values
   *       you need into local variables @b before calling run().
   *
   * @param cmdline  Full command line (command name + arguments).
   * @return         The command's return code, or -1 on error.
   */
  int run(const char *cmdline);
  int run(const String &cmdline) {
    return run(cmdline.c_str());
  }

  // ---------------------------------------------------------------------------
  // linenoise utilities
  // ---------------------------------------------------------------------------

  /** @brief Clear the terminal screen. */
  void clearScreen();

  /**
   * @brief Enable or disable multi-line editing mode.
   *
   * In multi-line mode the prompt and input wrap across terminal rows.
   * Disabled by default.
   */
  void setMultiLine(bool enable);

  /**
   * @brief Enable or disable plain (non-VT100) input mode.
   *
   * In plain mode linenoise falls back to basic line input without cursor
   * movement, arrow-key history, or tab completion rendering.  Useful for
   * terminals that do not support ANSI/VT100 escape sequences.
   *
   * By default, beginRepl() probes the terminal at startup (sending a one-time
   * Device Status Request) and enables plain mode automatically if the terminal
   * does not respond.
   *
   * @param enable  true to enable plain mode, false to restore VT100 mode.
   * @param force   When true (default), the automatic VT100 probe at
   *                beginRepl() startup is skipped and the mode set here is
   *                kept unconditionally.  When false, the mode is set
   *                immediately but beginRepl() may still override it based
   *                on the probe result.
   */
  void setPlainMode(bool enable, bool force = true);

  /**
   * @brief Check whether plain (non-VT100) input mode is currently active.
   *
   * @return true if plain mode is enabled.
   */
  bool isPlainMode();

  // ---------------------------------------------------------------------------
  // Argument splitting utility
  // ---------------------------------------------------------------------------

  /**
   * @brief Split a command line string into argv-style tokens in place.
   *
   * Handles quoted strings and backslash escapes.  Modifies the input buffer.
   *
   * @param line       Null-terminated input string (modified in place).
   * @param argv       Output array of pointers to tokens.
   * @param argv_size  Size of the argv array (max tokens + 1 for NULL sentinel).
   * @return           Number of tokens found (argc).
   */
  static size_t splitArgv(char *line, char **argv, size_t argv_size);

private:
  static void _replTask(void *arg);

  bool         _initialized;
  bool         _replStarted;
  TaskHandle_t _replTaskHandle;
  const char  *_prompt;
  uint32_t     _maxHistory;
  char        *_historyVfsPath;
  fs::FS      *_historyFs;
  uint32_t     _taskStackSize;
  uint32_t     _taskPriority;
  BaseType_t   _taskCore;
  bool         _usePsram;
  bool         _taskStackInPsram;
  bool         _forceMode;
};

extern ConsoleClass Console;
