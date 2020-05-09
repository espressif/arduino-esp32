/*
 * coap_mutex.h -- mutex utilities
 *
 * Copyright (C) 2019 Jon Shallow <supjps-libcoap@jpshallow.com>
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

#if defined(RIOT_VERSION)

#include <mutex.h>

typedef mutex_t coap_mutex_t;
#define COAP_MUTEX_INITIALIZER MUTEX_INIT
#define coap_mutex_lock(a) mutex_lock(a)
#define coap_mutex_trylock(a) mutex_trylock(a)
#define coap_mutex_unlock(a) mutex_unlock(a)

#elif defined(WITH_CONTIKI)

/* CONTIKI does not support mutex */

typedef int coap_mutex_t;
#define COAP_MUTEX_INITIALIZER 0
#define coap_mutex_lock(a) *(a) = 1
#define coap_mutex_trylock(a) *(a) = 1
#define coap_mutex_unlock(a) *(a) = 0

#else /* ! RIOT_VERSION && ! WITH_CONTIKI */

#include <pthread.h>

typedef pthread_mutex_t coap_mutex_t;
#define COAP_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define coap_mutex_lock(a) pthread_mutex_lock(a)
#define coap_mutex_trylock(a) pthread_mutex_trylock(a)
#define coap_mutex_unlock(a) pthread_mutex_unlock(a)

#endif /* ! RIOT_VERSION && ! WITH_CONTIKI */

#endif /* COAP_MUTEX_H_ */
