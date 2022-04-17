/*
 * coap_mutex.h -- mutex utilities
 *
 * Copyright (C) 2019 Jon Shallow <supjps-libcoap@jpshallow.com>
 *               2019 Olaf Bergmann <bergmann@tzi.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file coap_mutex.h
 * @brief COAP mutex mechanism wrapper
 */

#ifndef COAP_MUTEX_H_
#define COAP_MUTEX_H_

/*
 * Mutexes are currently only used if there is a constrained stack,
 * and large static variables (instead of the large variable being on
 * the stack) need to be protected.
 */
#if COAP_CONSTRAINED_STACK

#if defined(HAVE_PTHREAD_H) && defined(HAVE_PTHREAD_MUTEX_LOCK)
#include <pthread.h>

typedef pthread_mutex_t coap_mutex_t;
#define COAP_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define coap_mutex_lock(a) pthread_mutex_lock(a)
#define coap_mutex_trylock(a) pthread_mutex_trylock(a)
#define coap_mutex_unlock(a) pthread_mutex_unlock(a)

#elif defined(RIOT_VERSION)
/* use RIOT's mutex API */
#include <mutex.h>

typedef mutex_t coap_mutex_t;
#define COAP_MUTEX_INITIALIZER MUTEX_INIT
#define coap_mutex_lock(a) mutex_lock(a)
#define coap_mutex_trylock(a) mutex_trylock(a)
#define coap_mutex_unlock(a) mutex_unlock(a)

#else
/* define stub mutex functions */
typedef int coap_mutex_t;
#define COAP_MUTEX_INITIALIZER 0
#define coap_mutex_lock(a) *(a) = 1
#define coap_mutex_trylock(a) *(a) = 1
#define coap_mutex_unlock(a) *(a) = 0

#endif /* !RIOT_VERSION && !HAVE_PTHREAD_H && !HAVE_PTHREAD_MUTEX_LOCK */

#endif /* COAP_CONSTRAINED_STACK */

#endif /* COAP_MUTEX_H_ */
