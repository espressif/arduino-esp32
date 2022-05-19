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

#ifndef OSAL_MYNEWT_H_
#define OSAL_MYNEWT_H_

#include "os/os.h"

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// TASK API
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline void osal_task_delay(uint32_t msec)
{
  os_time_delay( os_time_ms_to_ticks32(msec) );
}

//--------------------------------------------------------------------+
// Semaphore API
//--------------------------------------------------------------------+
typedef struct os_sem  osal_semaphore_def_t;
typedef struct os_sem* osal_semaphore_t;

TU_ATTR_ALWAYS_INLINE static inline osal_semaphore_t osal_semaphore_create(osal_semaphore_def_t* semdef)
{
  return (os_sem_init(semdef, 0) == OS_OK) ? (osal_semaphore_t) semdef : NULL;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr)
{
  (void) in_isr;
  return os_sem_release(sem_hdl) == OS_OK;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_wait(osal_semaphore_t sem_hdl, uint32_t msec)
{
  uint32_t const ticks = (msec == OSAL_TIMEOUT_WAIT_FOREVER) ? OS_TIMEOUT_NEVER : os_time_ms_to_ticks32(msec);
  return os_sem_pend(sem_hdl, ticks) == OS_OK;
}

static inline void osal_semaphore_reset(osal_semaphore_t sem_hdl)
{
  // TODO implement later
}

//--------------------------------------------------------------------+
// MUTEX API (priority inheritance)
//--------------------------------------------------------------------+
typedef struct os_mutex osal_mutex_def_t;
typedef struct os_mutex* osal_mutex_t;

TU_ATTR_ALWAYS_INLINE static inline osal_mutex_t osal_mutex_create(osal_mutex_def_t* mdef)
{
  return (os_mutex_init(mdef) == OS_OK) ? (osal_mutex_t) mdef : NULL;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_lock(osal_mutex_t mutex_hdl, uint32_t msec)
{
  uint32_t const ticks = (msec == OSAL_TIMEOUT_WAIT_FOREVER) ? OS_TIMEOUT_NEVER : os_time_ms_to_ticks32(msec);
  return os_mutex_pend(mutex_hdl, ticks) == OS_OK;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_unlock(osal_mutex_t mutex_hdl)
{
  return os_mutex_release(mutex_hdl) == OS_OK;
}

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+

// role device/host is used by OS NONE for mutex (disable usb isr) only
#define OSAL_QUEUE_DEF(_int_set, _name, _depth, _type) \
  static _type _name##_##buf[_depth];\
  static struct os_event _name##_##evbuf[_depth];\
  osal_queue_def_t _name = { .depth = _depth, .item_sz = sizeof(_type), .buf = _name##_##buf, .evbuf =  _name##_##evbuf};\

typedef struct
{
  uint16_t depth;
  uint16_t item_sz;
  void*    buf;
  void*    evbuf;

  struct os_mempool mpool;
  struct os_mempool epool;

  struct os_eventq  evq;
}osal_queue_def_t;

typedef osal_queue_def_t* osal_queue_t;

TU_ATTR_ALWAYS_INLINE static inline osal_queue_t osal_queue_create(osal_queue_def_t* qdef)
{
  if ( OS_OK != os_mempool_init(&qdef->mpool, qdef->depth, qdef->item_sz, qdef->buf, "usbd queue") ) return NULL;
  if ( OS_OK != os_mempool_init(&qdef->epool, qdef->depth, sizeof(struct os_event), qdef->evbuf, "usbd evqueue") ) return NULL;

  os_eventq_init(&qdef->evq);
  return (osal_queue_t) qdef;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_receive(osal_queue_t qhdl, void* data, uint32_t msec)
{
  (void) msec; // os_eventq_get() does not take timeout, always behave as msec = WAIT_FOREVER

  struct os_event* ev;
  ev = os_eventq_get(&qhdl->evq);

  memcpy(data, ev->ev_arg, qhdl->item_sz); // copy message
  os_memblock_put(&qhdl->mpool, ev->ev_arg); // put back mem block
  os_memblock_put(&qhdl->epool, ev);         // put back ev block

  return true;
}

static inline bool osal_queue_send(osal_queue_t qhdl, void const * data, bool in_isr)
{
  (void) in_isr;

  // get a block from mem pool for data
  void* ptr = os_memblock_get(&qhdl->mpool);
  if (!ptr) return false;
  memcpy(ptr, data, qhdl->item_sz);

  // get a block from event pool to put into queue
  struct os_event* ev = (struct os_event*) os_memblock_get(&qhdl->epool);
  if (!ev)
  {
    os_memblock_put(&qhdl->mpool, ptr);
    return false;
  }
  tu_memclr(ev, sizeof(struct os_event));
  ev->ev_arg = ptr;

  os_eventq_put(&qhdl->evq, ev);

  return true;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_empty(osal_queue_t qhdl)
{
  return STAILQ_EMPTY(&qhdl->evq.evq_list);
}


#ifdef __cplusplus
 }
#endif

#endif /* OSAL_MYNEWT_H_ */
