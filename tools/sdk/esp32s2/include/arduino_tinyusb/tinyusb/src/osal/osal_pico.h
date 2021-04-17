/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
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

#ifndef _TUSB_OSAL_PICO_H_
#define _TUSB_OSAL_PICO_H_

#include "pico/time.h"
#include "pico/sem.h"
#include "pico/mutex.h"
#include "pico/critical_section.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// TASK API
//--------------------------------------------------------------------+
static inline void osal_task_delay(uint32_t msec)
{
  sleep_ms(msec);
}

//--------------------------------------------------------------------+
// Binary Semaphore API
//--------------------------------------------------------------------+
typedef struct semaphore osal_semaphore_def_t, *osal_semaphore_t;

static inline osal_semaphore_t osal_semaphore_create(osal_semaphore_def_t* semdef)
{
  sem_init(semdef, 0, 255);
  return semdef;
}

static inline bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr)
{
  sem_release(sem_hdl);
  return true;
}

static inline bool osal_semaphore_wait (osal_semaphore_t sem_hdl, uint32_t msec)
{
  return sem_acquire_timeout_ms(sem_hdl, msec);
}

static inline void osal_semaphore_reset(osal_semaphore_t sem_hdl)
{
  sem_reset(sem_hdl, 0);
}

//--------------------------------------------------------------------+
// MUTEX API
// Within tinyusb, mutex is never used in ISR context
//--------------------------------------------------------------------+
typedef struct mutex osal_mutex_def_t, *osal_mutex_t;

static inline osal_mutex_t osal_mutex_create(osal_mutex_def_t* mdef)
{
    mutex_init(mdef);
    return mdef;
}

static inline bool osal_mutex_lock (osal_mutex_t mutex_hdl, uint32_t msec)
{
    return mutex_enter_timeout_ms(mutex_hdl, msec);
}

static inline bool osal_mutex_unlock(osal_mutex_t mutex_hdl)
{
    mutex_exit(mutex_hdl);
    return true;
}

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+
#include "common/tusb_fifo.h"

#if TUSB_OPT_HOST_ENABLED
extern void hcd_int_disable(uint8_t rhport);
extern void hcd_int_enable(uint8_t rhport);
#endif

typedef struct
{
    tu_fifo_t ff;
    struct critical_section critsec; // osal_queue may be used in IRQs, so need critical section
} osal_queue_def_t;

typedef osal_queue_def_t* osal_queue_t;

// role device/host is used by OS NONE for mutex (disable usb isr) only
#define OSAL_QUEUE_DEF(_role, _name, _depth, _type)       \
  uint8_t _name##_buf[_depth*sizeof(_type)];              \
  osal_queue_def_t _name = {                              \
    .ff = TU_FIFO_INIT(_name##_buf, _depth, _type, false) \
  }

// lock queue by disable USB interrupt
static inline void _osal_q_lock(osal_queue_t qhdl)
{
    critical_section_enter_blocking(&qhdl->critsec);
}

// unlock queue
static inline void _osal_q_unlock(osal_queue_t qhdl)
{
    critical_section_exit(&qhdl->critsec);
}

static inline osal_queue_t osal_queue_create(osal_queue_def_t* qdef)
{
  critical_section_init(&qdef->critsec);
  tu_fifo_clear(&qdef->ff);
  return (osal_queue_t) qdef;
}

static inline bool osal_queue_receive(osal_queue_t qhdl, void* data)
{
  // TODO: revisit... docs say that mutexes are never used from IRQ context,
  //  however osal_queue_recieve may be. therefore my assumption is that
  //  the fifo mutex is not populated for queues used from an IRQ context
  //assert(!qhdl->ff.mutex);

  _osal_q_lock(qhdl);
  bool success = tu_fifo_read(&qhdl->ff, data);
  _osal_q_unlock(qhdl);

  return success;
}

static inline bool osal_queue_send(osal_queue_t qhdl, void const * data, bool in_isr)
{
  // TODO: revisit... docs say that mutexes are never used from IRQ context,
  //  however osal_queue_recieve may be. therefore my assumption is that
  //  the fifo mutex is not populated for queues used from an IRQ context
  //assert(!qhdl->ff.mutex);

  _osal_q_lock(qhdl);
  bool success = tu_fifo_write(&qhdl->ff, data);
  _osal_q_unlock(qhdl);

  TU_ASSERT(success);

  return success;
}

static inline bool osal_queue_empty(osal_queue_t qhdl)
{
  // TODO: revisit; whether this is true or not currently, tu_fifo_empty is a single
  //  volatile read.

  // Skip queue lock/unlock since this function is primarily called
  // with interrupt disabled before going into low power mode
  return tu_fifo_empty(&qhdl->ff);
}

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_OSAL_PICO_H_ */
