/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 tfx2001 (2479727366@qq.com)
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

#ifndef _TUSB_OSAL_RTTHREAD_H_
#define _TUSB_OSAL_RTTHREAD_H_

// RT-Thread Headers
#include "rtthread.h"

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// TASK API
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline void osal_task_delay(uint32_t msec) {
  rt_thread_mdelay(msec);
}

//--------------------------------------------------------------------+
// Semaphore API
//--------------------------------------------------------------------+
typedef struct rt_semaphore osal_semaphore_def_t;
typedef rt_sem_t osal_semaphore_t;

TU_ATTR_ALWAYS_INLINE static inline osal_semaphore_t
osal_semaphore_create(osal_semaphore_def_t *semdef) {
    rt_sem_init(semdef, "tusb", 0, RT_IPC_FLAG_PRIO);
    return semdef;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_post(osal_semaphore_t sem_hdl, bool in_isr) {
    (void) in_isr;
    return rt_sem_release(sem_hdl) == RT_EOK;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_semaphore_wait(osal_semaphore_t sem_hdl, uint32_t msec) {
    return rt_sem_take(sem_hdl, rt_tick_from_millisecond((rt_int32_t) msec)) == RT_EOK;
}

TU_ATTR_ALWAYS_INLINE static inline void osal_semaphore_reset(osal_semaphore_t const sem_hdl) {
    // TODO: implement
}

//--------------------------------------------------------------------+
// MUTEX API (priority inheritance)
//--------------------------------------------------------------------+
typedef struct rt_mutex osal_mutex_def_t;
typedef rt_mutex_t osal_mutex_t;

TU_ATTR_ALWAYS_INLINE static inline osal_mutex_t osal_mutex_create(osal_mutex_def_t *mdef) {
    rt_mutex_init(mdef, "tusb", RT_IPC_FLAG_PRIO);
    return mdef;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_lock(osal_mutex_t mutex_hdl, uint32_t msec) {
    return rt_mutex_take(mutex_hdl, rt_tick_from_millisecond((rt_int32_t) msec)) == RT_EOK;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_mutex_unlock(osal_mutex_t mutex_hdl) {
    return rt_mutex_release(mutex_hdl) == RT_EOK;
}

//--------------------------------------------------------------------+
// QUEUE API
//--------------------------------------------------------------------+

// role device/host is used by OS NONE for mutex (disable usb isr) only
#define OSAL_QUEUE_DEF(_int_set, _name, _depth, _type) \
    static _type _name##_##buf[_depth]; \
    osal_queue_def_t _name = { .depth = _depth, .item_sz = sizeof(_type), .buf = _name##_##buf };

typedef struct {
    uint16_t depth;
    uint16_t item_sz;
    void *buf;

    struct rt_messagequeue sq;
} osal_queue_def_t;

typedef rt_mq_t osal_queue_t;

TU_ATTR_ALWAYS_INLINE static inline osal_queue_t osal_queue_create(osal_queue_def_t *qdef) {
    rt_mq_init(&(qdef->sq), "tusb", qdef->buf, qdef->item_sz,
               qdef->item_sz * qdef->depth, RT_IPC_FLAG_PRIO);
    return &(qdef->sq);
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_receive(osal_queue_t qhdl, void *data, uint32_t msec) {

    rt_tick_t tick = rt_tick_from_millisecond((rt_int32_t) msec);
    return rt_mq_recv(qhdl, data, qhdl->msg_size, tick) == RT_EOK;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_send(osal_queue_t qhdl, void const *data, bool in_isr) {
    (void) in_isr;
    return rt_mq_send(qhdl, (void *)data, qhdl->msg_size) == RT_EOK;
}

TU_ATTR_ALWAYS_INLINE static inline bool osal_queue_empty(osal_queue_t qhdl) {
    return (qhdl->entry) == 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_OSAL_RTTHREAD_H_ */
