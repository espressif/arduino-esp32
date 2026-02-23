/*
 * ConsoleFS — File system CLI commands over LittleFS.
 *
 * Implements a minimal shell for browsing and managing the LittleFS partition:
 *
 *   ls   [path]           — list directory contents
 *   cat  <file>           — print file contents
 *   rm   <file>           — delete a file
 *   mkdir <dir>           — create a directory
 *   write <file> <text>   — write (overwrite) text to a file
 *   df                    — show filesystem usage
 *
 * Requires LittleFS to be mounted (LittleFS.begin() called in setup).
 *
 * Created by lucasssvaz
 */

#include <Arduino.h>
#include <Console.h>
#include <LittleFS.h>

// Ensure path starts with '/' so LittleFS.open() accepts it.
// Returns a pointer to either the original string or a static buffer.
static const char *normalizePath(const char *path) {
  if (path[0] == '/') {
    return path;
  }
  static char buf[256];
  snprintf(buf, sizeof(buf), "/%s", path);
  return buf;
}

// ---------------------------------------------------------------------------
// ls [path]
// ---------------------------------------------------------------------------
static int cmd_ls(int argc, char **argv) {
  const char *path = (argc > 1) ? normalizePath(argv[1]) : "/";

  File dir = LittleFS.open(path);
  if (!dir || !dir.isDirectory()) {
    printf("ls: %s: not a directory\n", path);
    return 1;
  }

  File entry = dir.openNextFile();
  while (entry) {
    if (entry.isDirectory()) {
      printf("d  %s/\n", entry.name());
    } else {
      printf("f  %-40s  %zu bytes\n", entry.name(), entry.size());
    }
    entry = dir.openNextFile();
  }
  return 0;
}

// ---------------------------------------------------------------------------
// cat <file>
// ---------------------------------------------------------------------------
static int cmd_cat(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: cat <file>\n");
    return 1;
  }

  const char *path = normalizePath(argv[1]);
  File f = LittleFS.open(path, "r");
  if (!f) {
    printf("cat: %s: no such file\n", path);
    return 1;
  }

  while (f.available()) {
    int c = f.read();
    putchar(c);
  }
  f.close();
  printf("\n");
  return 0;
}

// ---------------------------------------------------------------------------
// rm <file>
// ---------------------------------------------------------------------------
static int cmd_rm(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: rm <file>\n");
    return 1;
  }

  const char *path = normalizePath(argv[1]);
  if (!LittleFS.exists(path)) {
    printf("rm: %s: no such file\n", path);
    return 1;
  }

  if (!LittleFS.remove(path)) {
    printf("rm: %s: failed\n", path);
    return 1;
  }

  printf("removed '%s'\n", path);
  return 0;
}

// ---------------------------------------------------------------------------
// mkdir <dir>
// ---------------------------------------------------------------------------
static int cmd_mkdir(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: mkdir <dir>\n");
    return 1;
  }

  const char *path = normalizePath(argv[1]);
  if (!LittleFS.mkdir(path)) {
    printf("mkdir: %s: failed\n", path);
    return 1;
  }

  printf("created '%s'\n", path);
  return 0;
}

// ---------------------------------------------------------------------------
// write <file> <text...>
// ---------------------------------------------------------------------------
static int cmd_write(int argc, char **argv) {
  if (argc < 3) {
    printf("Usage: write <file> <text...>\n");
    return 1;
  }

  const char *path = normalizePath(argv[1]);
  File f = LittleFS.open(path, "w");
  if (!f) {
    printf("write: %s: cannot open\n", path);
    return 1;
  }

  for (int i = 2; i < argc; i++) {
    f.print(argv[i]);
    if (i < argc - 1) {
      f.print(' ');
    }
  }
  f.println();
  f.close();
  printf("written to '%s'\n", path);
  return 0;
}

// ---------------------------------------------------------------------------
// df
// ---------------------------------------------------------------------------
static int cmd_df(int argc, char **argv) {
  size_t total = LittleFS.totalBytes();
  size_t used  = LittleFS.usedBytes();
  size_t free_ = total - used;
  printf("Filesystem  Size     Used     Free     Use%%\n");
  printf("LittleFS    %-8zu %-8zu %-8zu %3zu%%\n", total, used, free_, total ? (used * 100 / total) : 0);
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
    Serial.println("LittleFS mount failed");
    return;
  }
  Serial.println("LittleFS mounted");

  // Configure Console prompt
  Console.setPrompt("fs> ");

  // Initialize Console
  if (!Console.begin()) {
    Serial.println("Console init failed");
    return;
  }

  // Add commands
  Console.addCmd("ls",    "List directory contents",      "[path]",           cmd_ls);
  Console.addCmd("cat",   "Print file contents",          "<file>",           cmd_cat);
  Console.addCmd("rm",    "Delete a file",                "<file>",           cmd_rm);
  Console.addCmd("mkdir", "Create a directory",           "<dir>",            cmd_mkdir);
  Console.addCmd("write", "Write text to a file",         "<file> <text...>", cmd_write);
  Console.addCmd("df",    "Show filesystem usage",                            cmd_df);

  // Add built-in help command
  Console.addHelpCmd();

  // Begin Read-Eval-Print Loop
  Console.attachToSerial(true);
}

void loop() {
  // Loop task is not used in this example, so we delete it to free up resources
  vTaskDelete(NULL);
}
