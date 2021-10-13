/*
 * coap_time.h -- Clock Handling
 *
 * Copyright (C) 2010-2019 Olaf Bergmann <bergmann@tzi.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file coap_time.h
 * @brief Clock Handling
 */

#ifndef COAP_TIME_H_
#define COAP_TIME_H_

/**
 * @defgroup clock Clock Handling
 * Default implementation of internal clock.
 * @{
 */

#if defined(WITH_LWIP)

#include <stdint.h>
#include <lwip/sys.h>

/* lwIP provides ms in sys_now */
#define COAP_TICKS_PER_SECOND 1000

typedef uint32_t coap_tick_t;
typedef uint32_t coap_time_t;
typedef int32_t coap_tick_diff_t;

COAP_STATIC_INLINE void coap_ticks_impl(coap_tick_t *t) {
  *t = sys_now();
}

COAP_STATIC_INLINE void coap_clock_init_impl(void) {
}

#define coap_clock_init coap_clock_init_impl
#define coap_ticks coap_ticks_impl

COAP_STATIC_INLINE coap_time_t coap_ticks_to_rt(coap_tick_t t) {
  return t / COAP_TICKS_PER_SECOND;
}

COAP_STATIC_INLINE uint64_t coap_ticks_to_rt_us(coap_tick_t t) {
  return (uint64_t)t * 1000000 / COAP_TICKS_PER_SECOND;
}

#elif defined(WITH_CONTIKI)

#include "clock.h"

typedef clock_time_t coap_tick_t;
typedef clock_time_t coap_time_t;

/**
 * This data type is used to represent the difference between two clock_tick_t
 * values. This data type must have the same size in memory as coap_tick_t to
 * allow wrapping.
 */
typedef int coap_tick_diff_t;

#define COAP_TICKS_PER_SECOND CLOCK_SECOND

COAP_STATIC_INLINE void coap_clock_init(void) {
  clock_init();
}

COAP_STATIC_INLINE void coap_ticks(coap_tick_t *t) {
  *t = clock_time();
}

COAP_STATIC_INLINE coap_time_t coap_ticks_to_rt(coap_tick_t t) {
  return t / COAP_TICKS_PER_SECOND;
}

COAP_STATIC_INLINE uint64_t coap_ticks_to_rt_us(coap_tick_t t) {
  return (uint64_t)t * 1000000 / COAP_TICKS_PER_SECOND;
}

#elif defined(RIOT_VERSION)
#include <xtimer.h>

#ifdef XTIMER_HZ
#define COAP_TICKS_PER_SECOND (XTIMER_HZ)
#else /* XTIMER_HZ */
#define COAP_TICKS_PER_SECOND (XTIMER_HZ_BASE)
#endif /* XTIMER_HZ */

typedef uint64_t coap_tick_t;
typedef int64_t coap_tick_diff_t;
typedef uint32_t coap_time_t;

static inline void coap_clock_init(void) {}

static inline void coap_ticks(coap_tick_t *t) {
  *t = xtimer_now_usec64();
}

static inline coap_time_t coap_ticks_to_rt(coap_tick_t t) {
  return t / 1000000UL;
}

static inline uint64_t coap_ticks_to_rt_us(coap_tick_t t) {
  return t;
}

static inline coap_tick_t coap_ticks_from_rt_us(uint64_t t) {
  return t / 1000000UL;
}
#else /* !WITH_LWIP && !WITH_CONTIKI && !RIOT_VERSION */

#include <stdint.h>

/**
 * This data type represents internal timer ticks with COAP_TICKS_PER_SECOND
 * resolution.
 */
typedef uint64_t coap_tick_t;

/**
 * CoAP time in seconds since epoch.
 */
typedef time_t coap_time_t;

/**
 * This data type is used to represent the difference between two clock_tick_t
 * values. This data type must have the same size in memory as coap_tick_t to
 * allow wrapping.
 */
typedef int64_t coap_tick_diff_t;

/** Use ms resolution on POSIX systems */
#define COAP_TICKS_PER_SECOND ((coap_tick_t)(1000U))

/**
 * Initializes the internal clock.
 */
void coap_clock_init(void);

/**
 * Sets @p t to the internal time with COAP_TICKS_PER_SECOND resolution.
 */
void coap_ticks(coap_tick_t *t);

/**
 * Helper function that converts coap ticks to wallclock time. On POSIX, this
 * function returns the number of seconds since the epoch. On other systems, it
 * may be the calculated number of seconds since last reboot or so.
 *
 * @param t Internal system ticks.
 *
 * @return  The number of seconds that has passed since a specific reference
 *          point (seconds since epoch on POSIX).
 */
coap_time_t coap_ticks_to_rt(coap_tick_t t);

/**
* Helper function that converts coap ticks to POSIX wallclock time in us.
*
* @param t Internal system ticks.
*
* @return  The number of seconds that has passed since a specific reference
*          point (seconds since epoch on POSIX).
*/
uint64_t coap_ticks_to_rt_us(coap_tick_t t);

/**
* Helper function that converts POSIX wallclock time in us to coap ticks.
*
* @param t POSIX time is us
*
* @return  coap ticks
*/
coap_tick_t coap_ticks_from_rt_us(uint64_t t);
#endif

/**
 * Returns @c 1 if and only if @p a is less than @p b where less is defined on a
 * signed data type.
 */
COAP_STATIC_INLINE int coap_time_lt(coap_tick_t a, coap_tick_t b) {
  return ((coap_tick_diff_t)(a - b)) < 0;
}

/**
 * Returns @c 1 if and only if @p a is less than or equal @p b where less is
 * defined on a signed data type.
 */
COAP_STATIC_INLINE int coap_time_le(coap_tick_t a, coap_tick_t b) {
  return a == b || coap_time_lt(a,b);
}

/** @} */

#endif /* COAP_TIME_H_ */
