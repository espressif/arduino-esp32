/*
 * ConsoleSysInfo — System information CLI commands.
 *
 * Implements a small set of diagnostic commands inspired by common Unix tools:
 *
 *   uptime  — elapsed time since boot
 *   heap    — free and minimum heap memory
 *   tasks   — list FreeRTOS tasks with state and stack watermark
 *   restart — restart the device
 *   version — firmware and IDF version strings
 *
 * Created by lucasssvaz
 */

#include <Arduino.h>
#include <Console.h>

// ---------------------------------------------------------------------------
// uptime
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
// heap
// ---------------------------------------------------------------------------
static int cmd_heap(int argc, char **argv) {
  printf("Free heap    : %7lu bytes\n", (unsigned long)ESP.getFreeHeap());
  printf("Min free heap: %7lu bytes\n", (unsigned long)ESP.getMinFreeHeap());
  printf("Max alloc    : %7lu bytes\n", (unsigned long)ESP.getMaxAllocHeap());
  return 0;
}

// ---------------------------------------------------------------------------
// tasks
// ---------------------------------------------------------------------------
static int cmd_tasks(int argc, char **argv) {
  uint32_t nTasks = uxTaskGetNumberOfTasks();
  TaskStatus_t *pxTaskStatusArray = (TaskStatus_t *)malloc(nTasks * sizeof(TaskStatus_t));
  if (!pxTaskStatusArray) {
    printf("Out of memory\n");
    return 1;
  }

  uint32_t ulTotalRunTime = 0;
  nTasks = uxTaskGetSystemState(pxTaskStatusArray, nTasks, &ulTotalRunTime);

  printf("%-20s %8s %5s %5s %4s\n", "Name", "State", "Prio", "Stack", "Core");
  printf("%-20s %8s %5s %5s %4s\n", "--------------------", "--------", "-----", "-----", "----");

  static const char *stateNames[] = {"Running", "Ready", "Blocked", "Suspend", "Deleted", "Invalid"};

  for (uint32_t i = 0; i < nTasks; i++) {
    TaskStatus_t *t = &pxTaskStatusArray[i];
    int state = (int)t->eCurrentState;
    if (state > 5) state = 5;
    int core = (int)t->xCoreID;
    char coreStr[4];
    if (core == tskNO_AFFINITY) {
      snprintf(coreStr, sizeof(coreStr), "any");
    } else {
      snprintf(coreStr, sizeof(coreStr), "%" PRIu8, (uint8_t)core);
    }
    printf("%-20s %8s %5lu %5lu %4s\n", t->pcTaskName, stateNames[state], (unsigned long)t->uxCurrentPriority, (unsigned long)t->usStackHighWaterMark, coreStr);
  }

  free(pxTaskStatusArray);
  return 0;
}

// ---------------------------------------------------------------------------
// version
// ---------------------------------------------------------------------------
static int cmd_version(int argc, char **argv) {
  printf("Arduino-ESP32 : %s\n", ESP_ARDUINO_VERSION_STR);
  printf("ESP-IDF       : %s\n", esp_get_idf_version());
  printf("Chip          : %s rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
  printf("CPU freq      : %lu MHz\n", (unsigned long)ESP.getCpuFreqMHz());
  printf("Flash size    : %lu bytes\n", (unsigned long)ESP.getFlashChipSize());
  return 0;
}

// ---------------------------------------------------------------------------
// restart
// ---------------------------------------------------------------------------
static int cmd_restart(int argc, char **argv) {
  printf("Restarting in 1 second...\n");
  fflush(stdout);
  delay(1000);
  ESP.restart();
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

  // Configure Console prompt
  Console.setPrompt("sysinfo> ");

  // Initialize Console
  if (!Console.begin()) {
    Serial.println("Console init failed");
    return;
  }

  // Add commands
  Console.addCmd("uptime",  "Show time since boot",          cmd_uptime);
  Console.addCmd("heap",    "Show heap memory statistics",   cmd_heap);
  Console.addCmd("tasks",   "List FreeRTOS tasks",           cmd_tasks);
  Console.addCmd("version", "Print firmware/chip version",   cmd_version);
  Console.addCmd("restart", "Restart the device",            cmd_restart);

  // Add built-in help command
  Console.addHelpCmd();

  // Begin Read-Eval-Print Loop
  Console.attachToSerial(true);
}

void loop() {
  // Loop task is not used in this example, so we delete it to free up resources
  vTaskDelete(NULL);
}
