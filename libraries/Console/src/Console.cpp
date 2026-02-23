/*
 * Console Arduino Library — implementation
 *
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "Arduino.h"
#include "Console.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_console.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"

// Linenoise read function that reads from Arduino Serial instead of VFS stdin.
// Serial.begin() installs the UART/USB driver whose ISR drains the hardware
// FIFO into a ring buffer.  VFS simplified read() also reads from the hardware
// FIFO, causing a race that loses bytes.  By reading through Serial we go via
// the same ring buffer the ISR fills, eliminating the conflict and working on
// every transport (UART, USB CDC, HWCDC) without transport-specific VFS calls.
//
// Line-ending normalisation: \r → \n, and \n immediately after \r is dropped.
// This handles terminals that send \r only, \n only, or \r\n.
static ssize_t _serialRead(int fd, void *buf, size_t count) {
  (void)fd;
  static bool s_prevCR = false;
  uint8_t *p = (uint8_t *)buf;
  size_t n = 0;
  while (n < count) {
    if (Serial.available()) {
      uint8_t c = Serial.read();
      if (c == '\n' && s_prevCR) {
        s_prevCR = false;
        continue;
      }
      s_prevCR = (c == '\r');
      if (c == '\r') {
        c = '\n';
      }
      p[n++] = c;
    } else {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
  return (ssize_t)n;
}

// VT100 probe that reads through Serial instead of VFS stdin.
// linenoiseProbe() uses raw read() on stdin which races with the UART ISR,
// so we implement our own using Serial for both write and read.
static bool _probeVT100() {
  // Drain any stale bytes in Serial's buffer
  while (Serial.available()) {
    Serial.read();
  }

  // Send Device Status Request (ESC[5n)
  Serial.print("\x1b[5n");
  Serial.flush();

  // Wait for response: ESC[0n (OK) or ESC[3n (not OK)
  const int retry_ms = 10;
  int timeout_ms = 500;
  size_t read_bytes = 0;
  char response[4] = {};

  while (timeout_ms > 0 && read_bytes < 4) {
    vTaskDelay(pdMS_TO_TICKS(retry_ms));
    timeout_ms -= retry_ms;
    while (Serial.available() && read_bytes < 4) {
      char c = Serial.read();
      if (read_bytes == 0 && c != '\x1b') {
        continue;
      }
      response[read_bytes++] = c;
    }
  }

  return (read_bytes >= 4 && response[0] == '\x1b' && response[1] == '[' && response[2] == '0' && response[3] == 'n');
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ConsoleClass::ConsoleClass()
  : _initialized(false), _replStarted(false), _replTaskHandle(NULL), _prompt("esp> "), _maxHistory(32), _historyVfsPath(NULL), _historyFs(NULL),
    _taskStackSize(4096), _taskPriority(2), _taskCore(tskNO_AFFINITY), _usePsram(false), _taskStackInPsram(false), _forceMode(false) {}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool ConsoleClass::begin(size_t maxCmdLen, size_t maxArgs) {
  if (_initialized) {
    log_w("Console already initialized");
    return true;
  }

  if (_usePsram && !psramFound()) {
    log_w("PSRAM not available, falling back to internal RAM");
    _usePsram = false;
  }

  esp_console_config_t cfg = ESP_CONSOLE_CONFIG_DEFAULT();
  cfg.max_cmdline_length = maxCmdLen;
  cfg.max_cmdline_args = maxArgs;
  if (_usePsram) {
    cfg.heap_alloc_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
  }

  esp_err_t err = esp_console_init(&cfg);
  if (err != ESP_OK) {
    log_e("esp_console_init failed: %s", esp_err_to_name(err));
    return false;
  }

  // Wire up linenoise tab-completion and hints from esp_console
  linenoiseSetCompletionCallback(&esp_console_get_completion);
  linenoiseSetHintsCallback((linenoiseHintsCallback *)&esp_console_get_hint);

  // History
  linenoiseHistorySetMaxLen((int)_maxHistory);
  if (_historyVfsPath) {
    linenoiseHistoryLoad(_historyVfsPath);
  }

  _initialized = true;
  return true;
}

void ConsoleClass::end() {
  if (!_initialized) {
    return;
  }

  stopRepl();

  if (_historyVfsPath) {
    linenoiseHistorySave(_historyVfsPath);
  }
  linenoiseHistoryFree();

  esp_err_t err = esp_console_deinit();
  if (err != ESP_OK) {
    log_e("esp_console_deinit failed: %s", esp_err_to_name(err));
  }

  free(_historyVfsPath);
  _historyVfsPath = NULL;
  _historyFs = NULL;
  _initialized = false;
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void ConsoleClass::setPrompt(const char *prompt) {
  _prompt = prompt;
}

void ConsoleClass::setMaxHistory(uint32_t maxLen) {
  _maxHistory = maxLen;
  if (_initialized) {
    linenoiseHistorySetMaxLen((int)maxLen);
  }
}

void ConsoleClass::setHistoryFile(const char *path, fs::FS &fs) {
  free(_historyVfsPath);
  _historyVfsPath = NULL;
  _historyFs = &fs;

  const char *mp = fs.mountpoint();
  if (!mp || !path) {
    return;
  }

  size_t mpLen = strlen(mp);
  size_t pathLen = strlen(path);
  _historyVfsPath = (char *)malloc(mpLen + pathLen + 1);
  if (_historyVfsPath) {
    strcpy(_historyVfsPath, mp);
    strcat(_historyVfsPath, path);
  }
}

void ConsoleClass::setTaskStackSize(uint32_t size) {
  _taskStackSize = size;
}

void ConsoleClass::setTaskPriority(uint32_t priority) {
  _taskPriority = priority;
}

void ConsoleClass::setTaskCore(BaseType_t core) {
  _taskCore = core;
}

void ConsoleClass::usePsram(bool enable) {
  _usePsram = enable;
}

// ---------------------------------------------------------------------------
// Command registration
// ---------------------------------------------------------------------------

bool ConsoleClass::addCmd(const char *name, const char *help, ConsoleCommandFunc func) {
  return addCmd(name, help, (const char *)NULL, func);
}

bool ConsoleClass::addCmd(const char *name, const char *help, const char *hint, ConsoleCommandFunc func) {
  if (!_initialized) {
    log_e("Console not initialized");
    return false;
  }
  esp_console_cmd_t cmd = {};
  cmd.command = name;
  cmd.help = help;
  cmd.hint = hint;
  cmd.func = func;
  esp_err_t err = esp_console_cmd_register(&cmd);
  if (err != ESP_OK) {
    log_e("Failed to register command '%s': %s", name, esp_err_to_name(err));
    return false;
  }
  return true;
}

bool ConsoleClass::addCmd(const char *name, const char *help, void *argtable, ConsoleCommandFunc func) {
  if (!_initialized) {
    log_e("Console not initialized");
    return false;
  }
  esp_console_cmd_t cmd = {};
  cmd.command = name;
  cmd.help = help;
  cmd.hint = NULL;  // auto-generated from argtable
  cmd.func = func;
  cmd.argtable = argtable;
  esp_err_t err = esp_console_cmd_register(&cmd);
  if (err != ESP_OK) {
    log_e("Failed to register command '%s': %s", name, esp_err_to_name(err));
    return false;
  }
  return true;
}

bool ConsoleClass::addCmdWithContext(const char *name, const char *help, ConsoleCommandFuncWithCtx func, void *ctx) {
  return addCmdWithContext(name, help, (const char *)NULL, func, ctx);
}

bool ConsoleClass::addCmdWithContext(const char *name, const char *help, const char *hint, ConsoleCommandFuncWithCtx func, void *ctx) {
  if (!_initialized) {
    log_e("Console not initialized");
    return false;
  }
  esp_console_cmd_t cmd = {};
  cmd.command = name;
  cmd.help = help;
  cmd.hint = hint;
  cmd.func_w_context = func;
  cmd.context = ctx;
  esp_err_t err = esp_console_cmd_register(&cmd);
  if (err != ESP_OK) {
    log_e("Failed to register command '%s': %s", name, esp_err_to_name(err));
    return false;
  }
  return true;
}

bool ConsoleClass::addCmdWithContext(const char *name, const char *help, void *argtable, ConsoleCommandFuncWithCtx func, void *ctx) {
  if (!_initialized) {
    log_e("Console not initialized");
    return false;
  }
  esp_console_cmd_t cmd = {};
  cmd.command = name;
  cmd.help = help;
  cmd.hint = NULL;
  cmd.func_w_context = func;
  cmd.context = ctx;
  cmd.argtable = argtable;
  esp_err_t err = esp_console_cmd_register(&cmd);
  if (err != ESP_OK) {
    log_e("Failed to register command '%s': %s", name, esp_err_to_name(err));
    return false;
  }
  return true;
}

bool ConsoleClass::removeCmd(const char *name) {
  if (!_initialized) {
    log_e("Console not initialized");
    return false;
  }
  esp_err_t err = esp_console_cmd_deregister(name);
  if (err != ESP_OK) {
    log_e("Failed to deregister command '%s': %s", name, esp_err_to_name(err));
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Built-in help command
// ---------------------------------------------------------------------------

bool ConsoleClass::addHelpCmd() {
  if (!_initialized) {
    log_e("Console not initialized");
    return false;
  }
  esp_err_t err = esp_console_register_help_command();
  if (err != ESP_OK) {
    log_e("Failed to register help command: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

bool ConsoleClass::removeHelpCmd() {
  if (!_initialized) {
    log_e("Console not initialized");
    return false;
  }
  esp_err_t err = esp_console_deregister_help_command();
  if (err != ESP_OK) {
    log_e("Failed to deregister help command: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

bool ConsoleClass::setHelpVerboseLevel(int level) {
  if (!_initialized) {
    log_e("Console not initialized");
    return false;
  }
  if (level < 0 || level >= ESP_CONSOLE_HELP_VERBOSE_LEVEL_MAX_NUM) {
    log_e("Invalid verbose level: %d", level);
    return false;
  }
  esp_err_t err = esp_console_set_help_verbose_level((esp_console_help_verbose_level_e)level);
  if (err != ESP_OK) {
    log_e("Failed to set help verbose level: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// REPL task
// ---------------------------------------------------------------------------

void ConsoleClass::_replTask(void *arg) {
  ConsoleClass *self = (ConsoleClass *)arg;

  // Auto-detect VT100 support unless the user already forced plain mode.
  // Uses a custom probe that reads through Serial instead of VFS stdin,
  // avoiding the race between the UART ISR and VFS read().
  if (!self->_forceMode) {
    log_d("Probing for VT100 support");
    bool vt100 = _probeVT100();
    log_d("VT100 probe result: %s", vt100 ? "supported" : "not supported");
    linenoiseSetDumbMode(!vt100);
  }

  // Install our Serial-based read function for linenoise.  This bypasses the
  // VFS stdin path entirely, reading from the same ring buffer that the
  // UART/USB driver ISR fills.  This avoids the race between the ISR and VFS
  // simplified read() that causes lost bytes, and works for UART, USB CDC,
  // and HWCDC.
  linenoiseSetReadFunction(_serialRead);

  while (self->_replStarted) {
    char *line = linenoise(self->_prompt);
    if (line == NULL) {
      // EOF or error — yield and retry
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    if (strlen(line) > 0) {
      linenoiseHistoryAdd(line);
      if (self->_historyVfsPath) {
        linenoiseHistorySave(self->_historyVfsPath);
      }

      int ret = 0;
      esp_err_t err = esp_console_run(line, &ret);
      switch (err) {
        case ESP_OK:           break;
        case ESP_ERR_NOT_FOUND: printf("Unknown command. Type 'help' for a list of commands.\n"); break;
        case ESP_ERR_INVALID_ARG: break;  // empty line
        default: printf("Command error: %s\n", esp_err_to_name(err)); break;
      }

      // Flush any command output that did not end with a newline. stdout is
      // line-buffered by default in ESP-IDF, so partial lines would otherwise
      // stay invisible until the next newline or buffer fill.
      fflush(stdout);
    }

    linenoiseFree(line);
  }

  if (self->_usePsram && self->_taskStackInPsram) {
    vTaskDeleteWithCaps(NULL);
  } else {
    vTaskDelete(NULL);
  }
}

bool ConsoleClass::beginRepl() {
  if (!_initialized) {
    log_e("Console not initialized — call begin() first");
    return false;
  }
  if (_replStarted) {
    log_w("REPL already running");
    return true;
  }

  _replStarted = true;

  BaseType_t ret;

#ifdef CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM
  _taskStackInPsram = _usePsram;
#else
  _taskStackInPsram = false;
#endif

  if (_taskStackInPsram) {
    ret = xTaskCreatePinnedToCoreWithCaps(_replTask, "console_repl", _taskStackSize, this, _taskPriority, &_replTaskHandle, _taskCore, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  } else {
    ret = xTaskCreatePinnedToCore(_replTask, "console_repl", _taskStackSize, this, _taskPriority, &_replTaskHandle, _taskCore);
  }
  if (ret != pdPASS) {
    log_e("Failed to create REPL task");
    _replStarted = false;
    return false;
  }
  return true;
}

bool ConsoleClass::stopRepl() {
  if (!_replStarted) {
    return true;
  }
  _replStarted = false;
  // The task will exit on its next iteration; give it time to clean up
  vTaskDelay(pdMS_TO_TICKS(100));
  _replTaskHandle = NULL;
  return true;
}

// ---------------------------------------------------------------------------
// Manual command execution
// ---------------------------------------------------------------------------

int ConsoleClass::run(const char *cmdline) {
  if (!_initialized) {
    log_e("Console not initialized");
    return -1;
  }
  int ret = 0;
  esp_err_t err = esp_console_run(cmdline, &ret);
  if (err == ESP_ERR_NOT_FOUND) {
    log_w("Unknown command: %s", cmdline);
    return -1;
  } else if (err == ESP_ERR_INVALID_ARG) {
    return 0;  // empty line
  } else if (err != ESP_OK) {
    log_e("esp_console_run error: %s", esp_err_to_name(err));
    return -1;
  }
  return ret;
}

// ---------------------------------------------------------------------------
// linenoise utilities
// ---------------------------------------------------------------------------

void ConsoleClass::clearScreen() {
  linenoiseClearScreen();
}

void ConsoleClass::setMultiLine(bool enable) {
  linenoiseSetMultiLine(enable ? 1 : 0);
}

void ConsoleClass::setPlainMode(bool enable, bool force) {
  _forceMode = force;
  linenoiseSetDumbMode(enable ? 1 : 0);
}

bool ConsoleClass::isPlainMode() {
  return linenoiseIsDumbMode();
}

// ---------------------------------------------------------------------------
// Argument splitting
// ---------------------------------------------------------------------------

size_t ConsoleClass::splitArgv(char *line, char **argv, size_t argv_size) {
  return esp_console_split_argv(line, argv, argv_size);
}

// ---------------------------------------------------------------------------
// Global instance
// ---------------------------------------------------------------------------

ConsoleClass Console;
