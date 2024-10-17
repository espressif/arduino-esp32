/*
    CoreMark benchmark for ESP32 using Arduino's C++ environment with multithreading support.

    Based on https://github.com/PaulStoffregen/CoreMark/tree/master
    Modified to run on ESP32 by Lucas Saavedra Vaz, 2024.
*/

#include <Arduino.h>
#include <stdarg.h>

#include <esp_task_wdt.h>

// Timeout for the task watchdog timer
#define TWDT_TIMEOUT_S 20

// Number of runs to average
#define N_RUNS 3

// A way to call the C-only coremark function from Arduino's C++ environment
extern "C" int coremark_main(void);

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  // To avoid the watchdog timer from resetting the ESP32 while running CoreMark we
  // need to reconfigure it to have a longer timeout.
  esp_task_wdt_config_t config = {
    .timeout_ms = TWDT_TIMEOUT_S * 1000,
    .idle_core_mask = 0,
    .trigger_panic = false,
  };

  esp_task_wdt_reconfigure(&config);

  log_d("Starting CoreMark test");
  Serial.printf("Runs: %d\n", N_RUNS);
  Serial.printf("Cores: %d\n", CONFIG_SOC_CPU_CORES_NUM);
  Serial.flush();
  for (int i = 0; i < N_RUNS; i++) {
    Serial.printf("Run %d\n", i);
    coremark_main();
    Serial.flush();
  }
  log_d("CoreMark test finished");
}

void loop() {
  vTaskDelete(NULL);
}

// CoreMark calls this function to print results.
extern "C" int ee_printf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  for (; *format; format++) {
    if (*format == '%') {
      bool islong = false;
      format++;
      if (*format == '%') {
        Serial.print(*format);
        continue;
      }
      if (*format == '-') {
        format++;  // ignore size
      }
      while (*format >= '0' && *format <= '9') {
        format++;  // ignore size
      }
      if (*format == 'l') {
        islong = true;
        format++;
      }
      if (*format == '\0') {
        break;
      }
      if (*format == 's') {
        Serial.print((char *)va_arg(args, int));
      } else if (*format == 'f') {
        Serial.print(va_arg(args, double));
      } else if (*format == 'd') {
        if (islong) {
          Serial.print(va_arg(args, long));
        } else {
          Serial.print(va_arg(args, int));
        }
      } else if (*format == 'u') {
        if (islong) {
          Serial.print(va_arg(args, unsigned long));
        } else {
          Serial.print(va_arg(args, unsigned int));
        }
      } else if (*format == 'x') {
        if (islong) {
          Serial.print(va_arg(args, unsigned long), HEX);
        } else {
          Serial.print(va_arg(args, unsigned int), HEX);
        }
      } else if (*format == 'c') {
        Serial.print(va_arg(args, int));
      }
    } else {
      if (*format == '\n') {
        Serial.print('\r');
      }
      Serial.print(*format);
    }
  }
  va_end(args);
  return 1;
}

// CoreMark calls this function to measure elapsed time
extern "C" uint32_t Arduino_millis(void) {
  return millis();
}
