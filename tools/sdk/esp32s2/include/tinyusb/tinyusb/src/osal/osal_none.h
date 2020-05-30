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

#ifndef _TUSB_OSAL_NONE_H_
#define _TUSB_OSAL_NONE_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// TASK API
//--------------------------------------------------------------------+
static inline void osal_task_delay(uint32_t msec)
{
  (void) msec;
  // TODO only used by Host stack, will implement using SOF

//  uint32_t start = tusb_hal_millis();
//  while ( ( tusb_hal_millis() - start ) < msec ) {}
}

//--------------------------------------------------------------------+
// Binary Semaphore API
//--------------------------------------------------------------------+
typedef struct
{
  volatile uint16_t count;
}osal_semaphore_def_t;

typedef osal_semaphore_def_t* osal_semaphore_t;

static inline osal_semaphore_t osal_semaphore_create(osal_semaphore_def_t* semdef)
{
  semdef->count = 0;
  return semdef;
}

static inline bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr)
{
  (void) in_isr;
  sem_hdl->count++;
  return true;
}

// TODO blocking for now
static inline bool osal_semaphore_wait (osal_semaphore_t sem_hdl, uint32_t msec)
{
  (void) msec;

  while (sem_hdl->count == 0) { }
  sem_hdl->count--;

  return true;
}

static inline void osal_semaphore_reset(osal_semaphore_t sem_hdl)
{
  sem_hdl->count = 0;
}

//--------------------------------------------------------------------+
// MUTEX API
// Within tinyusb, mutex is never used in ISR context
//--------------------------------------------------------------------+
typedef osal_semaphore_def_t osal_mutex_def_t;
typedef osal_semaphore_t osal_mutex_t;

static inline osal_mutex_t osal_mutex_create(osal_mutex_def_t* mdef)
{
  mdef->count = 1;
  return mdef;
}

static inline bool osal_mutex_lock (osal_mutex_t mutex_hdl, uint32_t msec)
{
  return osal_semaphore_wait(mutex_hdl, msec);
}

static inline bool osal_mutex_unlock(osal_mutex_t mutex_hdl)
{
  return osal_semaphore_post(mutex_hdl, false);
}

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+
#include "common/tusb_fifo.h"

// extern to avoid including dcd.h and hcd.h
#if TUSB_OPT_DEVICE_ENABLED
extern void dcd_int_disable(uint8_t rhport);
extern void dcd_int_enable(uint8_t rhport);
#endif

#if TUSB_OPT_HOST_ENABLED
extern void hcd_int_disable(uint8_t rhport);
extern void hcd_int_enable(uint8_t rhport);
#endif

typedef struct
{
    uint8_t role; // device or host
    tu_fifo_t ff;
}osal_queue_def_t;

typedef osal_queue_def_t* osal_queue_t;

// role device/host is used by OS NONE for mutex (disable usb isr) only
#define OSAL_QUEUE_DEF(_role, _name, _depth, _type) \
  uint8_t _name##_buf[_depth*sizeof(_type)];        \
  osal_queue_def_t _name = {                        \
    .role = _role,                                  \
    .ff = {                                         \
      .buffer       = _name##_buf,                  \
      .depth        = _depth,                       \
      .item_size    = sizeof(_type),                \
      .overwritable = false,                        \
    }\
  }

// lock queue by disable USB interrupt
static inline void _osal_q_lock(osal_queue_t qhdl)
{
  (void) qhdl;

#if TUSB_OPT_DEVICE_ENABLED
  if (qhdl->role == OPT_MODE_DEVICE) dcd_int_disable(TUD_OPT_RHPORT);
#endif

#if TUSB_OPT_HOST_ENABLED
  if (qhdl->role == OPT_MODE_HOST) hcd_int_disable(TUH_OPT_RHPORT);
#endif
}

// unlock queue
static inline void _osal_q_unlock(osal_queue_t qhdl)
{
  (void) qhdl;

#if TUSB_OPT_DEVICE_ENABLED
  if (qhdl->role == OPT_MODE_DEVICE) dcd_int_enable(TUD_OPT_RHPORT);
#endif

#if TUSB_OPT_HOST_ENABLED
  if (qhdl->role == OPT_MODE_HOST) hcd_int_enable(TUH_OPT_RHPORT);
#endif
}

static inline osal_queue_t osal_queue_create(osal_queue_def_t* qdef)
{
  tu_fifo_clear(&qdef->ff);
  return (osal_queue_t) qdef;
}

static inline bool osal_queue_receive(osal_queue_t qhdl, void* data)
{
  _osal_q_lock(qhdl);
  bool success = tu_fifo_read(&qhdl->ff, data);
  _osal_q_unlock(qhdl);

  return success;
}

static inline bool osal_queue_send(osal_queue_t qhdl, void const * data, bool in_isr)
{
  if (!in_isr) {
    _osal_q_lock(qhdl);
  }

  bool success = tu_fifo_write(&qhdl->ff, data);

  if (!in_isr) {
    _osal_q_unlock(qhdl);
  }

  TU_ASSERT(success);

  return success;
}

static inline bool osal_queue_empty(osal_queue_t qhdl)
{
  // Skip queue lock/unlock since this function is primarily called
  // with interrupt disabled before going into low power mode
  return tu_fifo_empty(&qhdl->ff);
}

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_OSAL_NONE_H_ */
