#include <Arduino.h>
#include "esp32-hal-ledc.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

static TaskHandle_t _tone_task = NULL;
static QueueHandle_t _tone_queue = NULL;
static int8_t _pin = -1;

typedef enum {
  TONE_START,
  TONE_END
} tone_cmd_t;

typedef struct {
  tone_cmd_t tone_cmd;
  uint8_t pin;
  unsigned int frequency;
  unsigned long duration;
} tone_msg_t;

static void tone_task(void*) {
  tone_msg_t tone_msg;
  while (1) {
    xQueueReceive(_tone_queue, &tone_msg, portMAX_DELAY);
    switch (tone_msg.tone_cmd) {
      case TONE_START:
        log_d("Task received from queue TONE_START: pin=%d, frequency=%u Hz, duration=%lu ms", tone_msg.pin, tone_msg.frequency, tone_msg.duration);

        if (_pin == -1) {
          if (ledcAttach(tone_msg.pin, tone_msg.frequency, 10) == 0) {
            log_e("Tone start failed");
            break;
          }
          _pin = tone_msg.pin;
        }
        ledcWriteTone(tone_msg.pin, tone_msg.frequency);

        if (tone_msg.duration) {
          delay(tone_msg.duration);
          ledcWriteTone(tone_msg.pin, 0);
        }
        break;

      case TONE_END:
        log_d("Task received from queue TONE_END: pin=%d", tone_msg.pin);
        ledcWriteTone(tone_msg.pin, 0);
        ledcDetach(tone_msg.pin);
        _pin = -1;
        break;

      default:;  // do nothing
    }            // switch
  }              // infinite loop
}

static int tone_init() {
  if (_tone_queue == NULL) {
    log_v("Creating tone queue");
    _tone_queue = xQueueCreate(128, sizeof(tone_msg_t));
    if (_tone_queue == NULL) {
      log_e("Could not create tone queue");
      return 0;  // ERR
    }
    log_v("Tone queue created");
  }

  if (_tone_task == NULL) {
    log_v("Creating tone task");
    xTaskCreate(
      tone_task,   // Function to implement the task
      "toneTask",  // Name of the task
      3500,        // Stack size in words
      NULL,        // Task input parameter
      1,           // Priority of the task
      &_tone_task  // Task handle.
    );
    if (_tone_task == NULL) {
      log_e("Could not create tone task");
      return 0;  // ERR
    }
    log_v("Tone task created");
  }
  return 1;  // OK
}

void noTone(uint8_t pin) {
  log_d("noTone was called");
  if (_pin == pin) {
    if (tone_init()) {
      tone_msg_t tone_msg = {
        .tone_cmd = TONE_END,
        .pin = pin,
        .frequency = 0,  // Ignored
        .duration = 0,   // Ignored
      };
      xQueueReset(_tone_queue);  // clear queue
      xQueueSend(_tone_queue, &tone_msg, portMAX_DELAY);
    }
  } else {
    log_e("Tone is not running on given pin %d", pin);
  }
}

// parameters:
// pin - pin number which will output the signal
// frequency - PWM frequency in Hz
// duration - time in ms - how long will the signal be outputted.
//   If not provided, or 0 you must manually call noTone to end output
void tone(uint8_t pin, unsigned int frequency, unsigned long duration) {
  log_d("pin=%d, frequency=%u Hz, duration=%lu ms", pin, frequency, duration);
  if (_pin == -1 || _pin == pin) {
    if (tone_init()) {
      tone_msg_t tone_msg = {
        .tone_cmd = TONE_START,
        .pin = pin,
        .frequency = frequency,
        .duration = duration,
      };
      xQueueSend(_tone_queue, &tone_msg, portMAX_DELAY);
      return;
    }
  } else {
    log_e("Tone is still running on pin %d, call noTone(%d) first!", _pin, _pin);
    return;
  }
}
