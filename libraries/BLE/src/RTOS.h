/*
 * FreeRTOS.h
 *
 *  Created on: Feb 24, 2017
 *      Author: kolban
 */

#ifdef __cplusplus
#ifndef MAIN_FREERTOS_H_
#define MAIN_FREERTOS_H_
#include "Arduino.h"
#include <stdint.h>
#include <pthread.h>

#include <freertos/FreeRTOS.h>  // Include the base FreeRTOS definitions.
#include <freertos/task.h>      // Include the task definitions.
#include <freertos/semphr.h>    // Include the semaphore definitions.
#include <freertos/ringbuf.h>   // Include the ringbuffer definitions.

/**
 * @brief Interface to %FreeRTOS functions.
 */
class FreeRTOS {
public:
  static void sleep(uint32_t ms);
  static void startTask(void task(void *), String taskName, void *param = nullptr, uint32_t stackSize = 2048);
  static void deleteTask(TaskHandle_t pTask = nullptr);

  static uint32_t getTimeSinceStart();

  class Semaphore {
  public:
    Semaphore(String owner = "<Unknown>");
    ~Semaphore();
    void give();
    void give(uint32_t value);
    void giveFromISR();
    void setName(String name);
    bool take(String owner = "<Unknown>");
    bool take(uint32_t timeoutMs, String owner = "<Unknown>");
    String toString();
    uint32_t wait(String owner = "<Unknown>");
    bool timedWait(String owner = "<Unknown>", uint32_t timeoutMs = portMAX_DELAY);
    uint32_t value() {
      return m_value;
    };

  private:
    SemaphoreHandle_t m_semaphore;
    pthread_mutex_t m_pthread_mutex;
    String m_name;
    String m_owner;
    uint32_t m_value;
    bool m_usePthreads;
  };
};

/**
 * @brief Ringbuffer.
 */
class Ringbuffer {
public:
#ifdef ESP_IDF_VERSION_MAJOR
  Ringbuffer(size_t length, RingbufferType_t type = RINGBUF_TYPE_NOSPLIT);
#else
  Ringbuffer(size_t length, ringbuf_type_t type = RINGBUF_TYPE_NOSPLIT);
#endif
  ~Ringbuffer();

  void *receive(size_t *size, TickType_t wait = portMAX_DELAY);
  void returnItem(void *item);
  bool send(void *data, size_t length, TickType_t wait = portMAX_DELAY);

private:
  RingbufHandle_t m_handle;
};

#endif /* MAIN_FREERTOS_H_ */
#else
#include "freertos/FreeRTOS.h"
#endif
