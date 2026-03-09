/* Console Library Validation Test
 *
 * Phase 1: Interactive serial test — the Python test script sends commands
 *          over serial and verifies the Console responds correctly.
 *          Uses a manual input loop (Serial.read + Console.run) to test
 *          the serial transport end-to-end without the REPL task.
 * Phase 2: Unity tests — programmatic API validation including history
 *          persistence, nested run(), splitArgv, etc.
 */

#include <Arduino.h>
#include <Console.h>
#include <LittleFS.h>
#include <unity.h>
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"

#define HISTORY_PATH "/console_history.txt"

// ---------------------------------------------------------------------------
// Phase 1 — Interactive serial test commands
// ---------------------------------------------------------------------------

static volatile bool s_startUnity = false;
static volatile bool s_replTaskActive = false;

static int cmd_ping(int argc, char **argv) {
  Serial.println("pong");
  return 0;
}

static int cmd_run_tests(int argc, char **argv) {
  s_startUnity = true;
  return 0;
}

// Switches Phase 1 from the manual input loop to the REPL task so the Python
// test script can verify that the task reads commands and dispatches them.
// Prints "REPL_TASK_STARTED" before spawning the task so the Python script can
// safely wait for that marker before sending the first REPL command.
static int cmd_test_repl(int argc, char **argv) {
  s_replTaskActive = true;
  Serial.println("REPL_TASK_STARTED");
  Serial.flush();
  Console.attachToSerial(true);
  return 0;
}

// ---------------------------------------------------------------------------
// Phase 2 — Unity test command handlers
// ---------------------------------------------------------------------------

static int s_lastArgc = 0;
static String s_lastArgs[8];

static int cmd_echo(int argc, char **argv) {
  s_lastArgc = argc;
  for (int i = 0; i < argc && i < 8; i++) {
    s_lastArgs[i] = argv[i];
  }
  for (int i = 1; i < argc; i++) {
    if (i > 1) {
      Serial.print(' ');
    }
    Serial.print(argv[i]);
  }
  Serial.println();
  return 0;
}

static int cmd_add(int argc, char **argv) {
  if (argc != 3) {
    return 1;
  }
  int a = atoi(argv[1]);
  int b = atoi(argv[2]);
  Serial.println(a + b);
  return 0;
}

static int cmd_fail(int argc, char **argv) {
  return 42;
}

static int s_ctxValue = 0;

static int cmd_with_context(void *context, int argc, char **argv) {
  s_ctxValue = *(int *)context;
  return 0;
}

static int cmd_outer(int argc, char **argv) {
  if (argc < 2) {
    return 1;
  }
  String action = argv[1];
  if (action == "call_echo") {
    return Console.run("echo nested_works");
  }
  return 1;
}

// ---------------------------------------------------------------------------
// Unity setUp / tearDown
// ---------------------------------------------------------------------------

void setUp(void) {
  s_lastArgc = 0;
  for (int i = 0; i < 8; i++) {
    s_lastArgs[i] = "";
  }
  s_ctxValue = 0;
}

void tearDown(void) {}

// ---------------------------------------------------------------------------
// Unity test cases
// ---------------------------------------------------------------------------

void test_begin_end(void) {
  TEST_ASSERT_TRUE(Console.begin());
  Console.end();

  Console.end();

  TEST_ASSERT_TRUE(Console.begin());
  Console.end();
}

void test_begin_twice(void) {
  TEST_ASSERT_TRUE(Console.begin());
  TEST_ASSERT_TRUE(Console.begin());
  Console.end();
}

void test_add_remove_cmd(void) {
  Console.begin();

  TEST_ASSERT_TRUE(Console.addCmd("echo", "Echo args", cmd_echo));
  TEST_ASSERT_EQUAL(0, Console.run("echo hello"));
  TEST_ASSERT_EQUAL(2, s_lastArgc);
  TEST_ASSERT_EQUAL_STRING("hello", s_lastArgs[1].c_str());

  TEST_ASSERT_TRUE(Console.removeCmd("echo"));
  TEST_ASSERT_EQUAL(-1, Console.run("echo hello"));

  Console.end();
}

void test_add_cmd_with_hint(void) {
  Console.begin();

  TEST_ASSERT_TRUE(Console.addCmd("add", "Add numbers", "<a> <b>", cmd_add));
  TEST_ASSERT_EQUAL(0, Console.run("add 3 7"));
  TEST_ASSERT_EQUAL(-1, Console.run("nonexistent"));

  Console.end();
}

void test_add_cmd_with_context(void) {
  Console.begin();

  int ctx = 99;
  TEST_ASSERT_TRUE(Console.addCmdWithContext("ctx", "Context test", cmd_with_context, &ctx));
  TEST_ASSERT_EQUAL(0, Console.run("ctx"));
  TEST_ASSERT_EQUAL(99, s_ctxValue);

  Console.end();
}

void test_run_return_codes(void) {
  Console.begin();

  Console.addCmd("ok", "Returns 0", cmd_echo);
  Console.addCmd("fail", "Returns 42", cmd_fail);

  TEST_ASSERT_EQUAL(0, Console.run("ok test"));
  TEST_ASSERT_EQUAL(42, Console.run("fail"));
  TEST_ASSERT_EQUAL(-1, Console.run("no_such_command"));
  TEST_ASSERT_EQUAL(0, Console.run(""));

  Console.end();
}

void test_run_before_begin(void) {
  Console.end();
  TEST_ASSERT_EQUAL(-1, Console.run("anything"));
}

void test_nested_run(void) {
  Console.begin();

  Console.addCmd("echo", "Echo args", cmd_echo);
  Console.addCmd("outer", "Calls echo via run()", cmd_outer);

  TEST_ASSERT_EQUAL(0, Console.run("outer call_echo"));
  TEST_ASSERT_EQUAL_STRING("nested_works", s_lastArgs[1].c_str());

  Console.end();
}

void test_help_cmd(void) {
  Console.begin();

  TEST_ASSERT_TRUE(Console.addHelpCmd());
  TEST_ASSERT_EQUAL(0, Console.run("help"));

  TEST_ASSERT_TRUE(Console.removeHelpCmd());
  TEST_ASSERT_EQUAL(-1, Console.run("help"));

  Console.end();
}

void test_split_argv(void) {
  char line1[] = "hello world foo";
  char *argv[8];
  size_t argc;

  argc = Console.splitArgv(line1, argv, 8);
  TEST_ASSERT_EQUAL(3, argc);
  TEST_ASSERT_EQUAL_STRING("hello", argv[0]);
  TEST_ASSERT_EQUAL_STRING("world", argv[1]);
  TEST_ASSERT_EQUAL_STRING("foo", argv[2]);

  char line2[] = "\"quoted arg\" simple";
  argc = Console.splitArgv(line2, argv, 8);
  TEST_ASSERT_EQUAL(2, argc);
  TEST_ASSERT_EQUAL_STRING("quoted arg", argv[0]);
  TEST_ASSERT_EQUAL_STRING("simple", argv[1]);

  char line3[] = "";
  argc = Console.splitArgv(line3, argv, 8);
  TEST_ASSERT_EQUAL(0, argc);
}

void test_plain_mode(void) {
  Console.begin();

  Console.setPlainMode(true);
  TEST_ASSERT_TRUE(Console.isPlainMode());

  Console.setPlainMode(false);
  TEST_ASSERT_FALSE(Console.isPlainMode());

  Console.end();
}

void test_configuration(void) {
  Console.setPrompt("test> ");
  Console.setMaxHistory(16);
  Console.setTaskStackSize(8192);
  Console.setTaskPriority(3);
  Console.setTaskCore(0);

  TEST_ASSERT_TRUE(Console.begin());

  Console.setMaxHistory(64);

  Console.end();
}

void test_multiple_commands(void) {
  Console.begin();

  Console.addCmd("echo", "Echo", cmd_echo);
  Console.addCmd("add", "Add", "<a> <b>", cmd_add);
  Console.addCmd("fail", "Fail", cmd_fail);

  TEST_ASSERT_EQUAL(0, Console.run("echo a b c"));
  TEST_ASSERT_EQUAL(4, s_lastArgc);

  TEST_ASSERT_EQUAL(0, Console.run("add 10 20"));
  TEST_ASSERT_EQUAL(42, Console.run("fail"));

  Console.end();
}

void test_string_overloads(void) {
  Console.begin();

  String name = "echo";
  String help = "Echo args";
  TEST_ASSERT_TRUE(Console.addCmd(name, help, cmd_echo));

  String cmdline = "echo string_test";
  TEST_ASSERT_EQUAL(0, Console.run(cmdline));
  TEST_ASSERT_EQUAL_STRING("string_test", s_lastArgs[1].c_str());

  TEST_ASSERT_TRUE(Console.removeCmd(name));

  Console.end();
}

void test_history_save_to_file(void) {
  TEST_ASSERT_TRUE(LittleFS.begin(true));
  LittleFS.remove(HISTORY_PATH);

  Console.setHistoryFile(LittleFS, HISTORY_PATH);
  TEST_ASSERT_TRUE(Console.begin());

  Console.addCmd("echo", "Echo", cmd_echo);

  // Simulate REPL history entries via the linenoise API (exposed by Console.h)
  linenoiseHistoryAdd("echo first");
  linenoiseHistoryAdd("echo second");
  linenoiseHistoryAdd("echo third");

  // end() triggers linenoiseHistorySave to the configured file
  Console.end();

  TEST_ASSERT_TRUE_MESSAGE(LittleFS.exists(HISTORY_PATH), "History file was not created");

  File f = LittleFS.open(HISTORY_PATH, "r");
  TEST_ASSERT_TRUE_MESSAGE(f, "Failed to open history file");
  TEST_ASSERT_GREATER_THAN_MESSAGE(0, (int)f.size(), "History file is empty");

  String content = f.readString();
  f.close();

  TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, content.indexOf("echo first"), "Missing 'echo first' in history");
  TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, content.indexOf("echo second"), "Missing 'echo second' in history");
  TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, content.indexOf("echo third"), "Missing 'echo third' in history");

  LittleFS.remove(HISTORY_PATH);
  LittleFS.end();
}

void test_history_load_from_file(void) {
  TEST_ASSERT_TRUE(LittleFS.begin(true));

  // Pre-populate a history file
  {
    File f = LittleFS.open(HISTORY_PATH, FILE_WRITE);
    TEST_ASSERT_TRUE(f);
    f.println("echo loaded1");
    f.println("echo loaded2");
    f.close();
  }

  Console.setHistoryFile(LittleFS, HISTORY_PATH);
  TEST_ASSERT_TRUE(Console.begin());

  // begin() calls linenoiseHistoryLoad which reads the existing file.
  // Add a new entry and save via end().
  linenoiseHistoryAdd("echo loaded3");

  Console.end();

  // Verify the file now contains both pre-existing and new entries
  File f = LittleFS.open(HISTORY_PATH, "r");
  TEST_ASSERT_TRUE(f);
  String content = f.readString();
  f.close();

  TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, content.indexOf("echo loaded1"), "Missing pre-loaded entry 1");
  TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, content.indexOf("echo loaded2"), "Missing pre-loaded entry 2");
  TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, content.indexOf("echo loaded3"), "Missing new entry");

  LittleFS.remove(HISTORY_PATH);
  LittleFS.end();
}

void test_attach_to_serial(void) {
  // Before begin(): attach must fail and state must be false
  Console.end();
  TEST_ASSERT_FALSE(Console.attachToSerial(true));
  TEST_ASSERT_FALSE(Console.isAttachedToSerial());

  Console.begin();

  // Detach when not attached — no-op, must return true
  TEST_ASSERT_TRUE(Console.attachToSerial(false));
  TEST_ASSERT_FALSE(Console.isAttachedToSerial());

  // Attach
  TEST_ASSERT_TRUE(Console.attachToSerial(true));
  TEST_ASSERT_TRUE(Console.isAttachedToSerial());

  // Attach again — no-op, must still return true
  TEST_ASSERT_TRUE(Console.attachToSerial(true));
  TEST_ASSERT_TRUE(Console.isAttachedToSerial());

  // Detach
  TEST_ASSERT_TRUE(Console.attachToSerial(false));
  TEST_ASSERT_FALSE(Console.isAttachedToSerial());

  // Restart cycle
  TEST_ASSERT_TRUE(Console.attachToSerial(true));
  TEST_ASSERT_TRUE(Console.isAttachedToSerial());
  TEST_ASSERT_TRUE(Console.attachToSerial(false));
  TEST_ASSERT_FALSE(Console.isAttachedToSerial());

  // end() while attached — must stop the task and clear the flag
  TEST_ASSERT_TRUE(Console.attachToSerial(true));
  Console.end();
  TEST_ASSERT_FALSE(Console.isAttachedToSerial());
}

// ---------------------------------------------------------------------------
// Phase 1 — Manual serial input loop
// ---------------------------------------------------------------------------

static String s_inputBuffer;

static void processSerialInput() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (s_inputBuffer.length() > 0) {
        Console.run(s_inputBuffer);
        fflush(stdout);
        s_inputBuffer = "";
      }
    } else {
      s_inputBuffer += c;
    }
  }
}

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  // Phase 1: Set up Console for interactive serial test
  Console.setPlainMode(true);
  Console.usePsram(true);
  Console.begin();
  Console.addCmd("ping", "Reply with pong", cmd_ping);
  Console.addCmd("run_tests", "Start Unity tests", cmd_run_tests);
  Console.addCmd("test_repl", "Switch to REPL task and signal ready", cmd_test_repl);
  Console.addHelpCmd();

  Serial.println("REPL_READY");
}

void loop() {
  if (s_startUnity) {
    // Phase 2: Tear down Phase 1 and run Unity tests.
    // Stop the REPL task if it is still running (run_tests may have been sent
    // through the REPL task rather than the manual loop).
    Console.attachToSerial(false);
    s_replTaskActive = false;
    Console.end();
    delay(100);

    // Pre-format LittleFS so the history tests don't log mount errors
    // on a fresh (unformatted) Wokwi flash partition.
    LittleFS.begin(true);
    LittleFS.end();

    UNITY_BEGIN();
    RUN_TEST(test_begin_end);
    RUN_TEST(test_begin_twice);
    RUN_TEST(test_add_remove_cmd);
    RUN_TEST(test_add_cmd_with_hint);
    RUN_TEST(test_add_cmd_with_context);
    RUN_TEST(test_run_return_codes);
    RUN_TEST(test_run_before_begin);
    RUN_TEST(test_nested_run);
    RUN_TEST(test_help_cmd);
    RUN_TEST(test_split_argv);
    RUN_TEST(test_plain_mode);
    RUN_TEST(test_configuration);
    RUN_TEST(test_multiple_commands);
    RUN_TEST(test_string_overloads);
    RUN_TEST(test_history_save_to_file);
    RUN_TEST(test_history_load_from_file);
    RUN_TEST(test_attach_to_serial);
    UNITY_END();

    vTaskDelete(NULL);
  }

  // While the REPL task is active it owns Serial; skip the manual loop to
  // avoid racing with the task for the same incoming bytes.
  if (!s_replTaskActive) {
    processSerialInput();
  }
  delay(1);
}
