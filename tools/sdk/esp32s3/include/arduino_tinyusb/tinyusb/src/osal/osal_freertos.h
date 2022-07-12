/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#ifndef _TUSB_OSAL_FREERTOS_H_
#define _TUSB_OSAL_FREERTOS_H_

// FreeRTOS Headers
#include TU_INCLUDE_PATH(CFG_TUSB_OS_INC_PATH,FreeRTOS.h)
#include TU_INCLUDE_PATH(CFG_TUSB_OS_INC_PATH,semphr.h)
#include TU_INCLUDE_PATH(CFG_TUSB_OS_INC_PATH,queue.h)
#include TU_INCLUDE_PATH(CFG_TUSB_OS_INC_PATH,task.h)

#ifdef __cplusplus
extern "C" {
#endif

TU_ATTR_ALWAYS_INLINE static inline uint32_t _osal_ms2tick(uint32_t msec)
{
  if (msec == OSAL_TIMEOUT_WAIT_FOREVER) return portMAX_DELAY;
  if (msec == 0) return 0;

  uint32_t ticks = pdMS_TO_TICKS(msec);

  // configTICK_RATE_HZ is less than 1000 and 1 tick > 1 ms
  // we still need to delay at least 1 tick
  if (ticks == 0) ticks =1 ;

  return ticks;
}

//--------------------------------------------------------------------+
// TASK API
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline void osal_task_delay(uint32_t msec)
{
  vTaskDelay( pdMS_TO_TICKS(msec) );
}

//--------------------------------------------------------------------+
// Semaphore API
//--------------------------------------------------------------------+
typedef StaticSemaphore_t osal_semaphore_def_t;
typedef SemaphoreHandle_t osal_semaphore_t;

TU_ATTR_ALWAYS_INLINE static inline osal_semaphore_t osal_semaphore_create(osal_semaphore_def_t* semdef)
{
  return xSemaphoreCreateBinaryStatic(semdef);
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr)
{
  if ( !in_isr )
  {
    return xSemaphoreGive(sem_hdl) != 0;
  }
  else
  {
    BaseType_t xHigherPriorityTaskWoken;
    BaseType_t res = xSemaphoreGiveFromISR(sem_hdl, &xHigherPriorityTaskWoken);

#if CFG_TUSB_MCU == OPT_MCU_ESP32S2 || CFG_TUSB_MCU == OPT_MCU_ESP32S3
    // not needed after https://github.com/espressif/esp-idf/commit/c5fd79547ac9b7bae06fa660e9f814d18d3390b7
    if ( xHigherPriorityTaskWoken ) portYIELD_FROM_ISR();
#else
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
#endif

    return res != 0;
  }
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_wait (osal_semaphore_t sem_hdl, uint32_t msec)
{
  return xSemaphoreTake(sem_hdl, _osal_ms2tick(msec));
}

TU_ATTR_ALWAYS_INLINE static inline void osal_semaphore_reset(osal_semaphore_t const sem_hdl)
{
  xQueueReset(sem_hdl);
}

//--------------------------------------------------------------------+
// MUTEX API (priority inheritance)
//--------------------------------------------------------------------+
typedef StaticSemaphore_t osal_mutex_def_t;
typedef SemaphoreHandle_t osal_mutex_t;

TU_ATTR_ALWAYS_INLINE static inline osal_mutex_t osal_mutex_create(osal_mutex_def_t* mdef)
{
  return xSemaphoreCreateMutexStatic(mdef);
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_lock (osal_mutex_t mutex_hdl, uint32_t msec)
{
  return osal_semaphore_wait(mutex_hdl, msec);
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_unlock(osal_mutex_t mutex_hdl)
{
  return xSemaphoreGive(mutex_hdl);
}

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+

// _int_set is not used with an RTOS
#define OSAL_QUEUE_DEF(_int_set, _name, _depth, _type) \
  static _type _name##_##buf[_depth];\
  osal_queue_def_t _name = { .depth = _depth, .item_sz = sizeof(_type), .buf = _name##_##buf };

typedef struct
{
  uint16_t depth;
  uint16_t item_sz;
  void*    buf;

  StaticQueue_t sq;
}osal_queue_def_t;

typedef QueueHandle_t osal_queue_t;

TU_ATTR_ALWAYS_INLINE static inline osal_queue_t osal_queue_create(osal_queue_def_t* qdef)
{
  return xQueueCreateStatic(qdef->depth, qdef->item_sz, (uint8_t*) qdef->buf, &qdef->sq);
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_receive(osal_queue_t qhdl, void* data, uint32_t msec)
{
  return xQueueReceive(qhdl, data, _osal_ms2tick(msec));
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_send(osal_queue_t qhdl, void const * data, bool in_isr)
{
  if ( !in_isr )
  {
    return xQueueSendToBack(qhdl, data, OSAL_TIMEOUT_WAIT_FOREVER) != 0;
  }
  else
  {
    BaseType_t xHigherPriorityTaskWoken;
    BaseType_t res = xQueueSendToBackFromISR(qhdl, data, &xHigherPriorityTaskWoken);

#if CFG_TUSB_MCU == OPT_MCU_ESP32S2 || CFG_TUSB_MCU == OPT_MCU_ESP32S3
    // not needed after https://github.com/espressif/esp-idf/commit/c5fd79547ac9b7bae06fa660e9f814d18d3390b7
    if ( xHigherPriorityTaskWoken ) portYIELD_FROM_ISR();
#else
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
#endif

    return res != 0;
  }
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_empty(osal_queue_t qhdl)
{
  return uxQueueMessagesWaiting(qhdl) == 0;
}

#ifdef __cplusplus
 }
#endif

#endif
