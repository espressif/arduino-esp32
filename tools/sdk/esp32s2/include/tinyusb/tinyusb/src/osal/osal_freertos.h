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
#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// TASK API
//--------------------------------------------------------------------+
static inline void osal_task_delay(uint32_t msec)
{
  vTaskDelay( pdMS_TO_TICKS(msec) );
}

//--------------------------------------------------------------------+
// Semaphore API
//--------------------------------------------------------------------+
typedef StaticSemaphore_t osal_semaphore_def_t;
typedef SemaphoreHandle_t osal_semaphore_t;

static inline osal_semaphore_t osal_semaphore_create(osal_semaphore_def_t* semdef)
{
  return xSemaphoreCreateBinaryStatic(semdef);
}

static inline bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr)
{
  if ( !in_isr )
  {
    return xSemaphoreGive(sem_hdl) != 0;
  }
  else
  {
    BaseType_t xHigherPriorityTaskWoken;
    BaseType_t res = xSemaphoreGiveFromISR(sem_hdl, &xHigherPriorityTaskWoken);

#if CFG_TUSB_MCU == OPT_MCU_ESP32S2
    if ( xHigherPriorityTaskWoken ) portYIELD_FROM_ISR();
#else
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
#endif

    return res != 0;
  }
}

static inline bool osal_semaphore_wait (osal_semaphore_t sem_hdl, uint32_t msec)
{
  uint32_t const ticks = (msec == OSAL_TIMEOUT_WAIT_FOREVER) ? portMAX_DELAY : pdMS_TO_TICKS(msec);
  return xSemaphoreTake(sem_hdl, ticks);
}

static inline void osal_semaphore_reset(osal_semaphore_t const sem_hdl)
{
  xQueueReset(sem_hdl);
}

//--------------------------------------------------------------------+
// MUTEX API (priority inheritance)
//--------------------------------------------------------------------+
typedef StaticSemaphore_t osal_mutex_def_t;
typedef SemaphoreHandle_t osal_mutex_t;

static inline osal_mutex_t osal_mutex_create(osal_mutex_def_t* mdef)
{
  return xSemaphoreCreateMutexStatic(mdef);
}

static inline bool osal_mutex_lock (osal_mutex_t mutex_hdl, uint32_t msec)
{
  return osal_semaphore_wait(mutex_hdl, msec);
}

static inline bool osal_mutex_unlock(osal_mutex_t mutex_hdl)
{
  return xSemaphoreGive(mutex_hdl);
}

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+

// role device/host is used by OS NONE for mutex (disable usb isr) only
#define OSAL_QUEUE_DEF(_role, _name, _depth, _type) \
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

static inline osal_queue_t osal_queue_create(osal_queue_def_t* qdef)
{
  return xQueueCreateStatic(qdef->depth, qdef->item_sz, (uint8_t*) qdef->buf, &qdef->sq);
}

static inline bool osal_queue_receive(osal_queue_t qhdl, void* data)
{
  return xQueueReceive(qhdl, data, portMAX_DELAY);
}

static inline bool osal_queue_send(osal_queue_t qhdl, void const * data, bool in_isr)
{
  if ( !in_isr )
  {
    return xQueueSendToBack(qhdl, data, OSAL_TIMEOUT_WAIT_FOREVER) != 0;
  }
  else
  {
    BaseType_t xHigherPriorityTaskWoken;
    BaseType_t res = xQueueSendToBackFromISR(qhdl, data, &xHigherPriorityTaskWoken);

#if CFG_TUSB_MCU == OPT_MCU_ESP32S2
    if ( xHigherPriorityTaskWoken ) portYIELD_FROM_ISR();
#else
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
#endif

    return res != 0;
  }
}

static inline bool osal_queue_empty(osal_queue_t qhdl)
{
  return uxQueueMessagesWaiting(qhdl) == 0;
}

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_OSAL_FREERTOS_H_ */
