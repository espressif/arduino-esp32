/*
 * prng.h -- Pseudo Random Numbers
 *
 * Copyright (C) 2010-2011 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file prng.h
 * @brief Pseudo Random Numbers
 */

#ifndef COAP_PRNG_H_
#define COAP_PRNG_H_

/**
 * @defgroup prng Pseudo Random Numbers
 * API functions for gerating pseudo random numbers
 * @{
 */

#if defined(WITH_CONTIKI)
#include <string.h>

/**
 * Fills \p buf with \p len random bytes. This is the default implementation for
 * prng(). You might want to change prng() to use a better PRNG on your specific
 * platform.
 */
COAP_STATIC_INLINE int
contiki_prng_impl(unsigned char *buf, size_t len) {
  uint16_t v = random_rand();
  while (len > sizeof(v)) {
    memcpy(buf, &v, sizeof(v));
    len -= sizeof(v);
    buf += sizeof(v);
    v = random_rand();
  }

  memcpy(buf, &v, len);
  return 1;
}

#define prng(Buf,Length) contiki_prng_impl((Buf), (Length))
#define prng_init(Value) random_init((uint16_t)(Value))
#elif defined(WITH_LWIP) && defined(LWIP_RAND)
COAP_STATIC_INLINE int
lwip_prng_impl(unsigned char *buf, size_t len) {
  u32_t v = LWIP_RAND();
  while (len > sizeof(v)) {
    memcpy(buf, &v, sizeof(v));
    len -= sizeof(v);
    buf += sizeof(v);
    v = LWIP_RAND();
  }

  memcpy(buf, &v, len);
  return 1;
}

#define prng(Buf,Length) lwip_prng_impl((Buf), (Length))
#define prng_init(Value)
#elif defined(_WIN32)
#define prng_init(Value)
errno_t __cdecl rand_s( _Out_ unsigned int* _RandomValue );
 /**
 * Fills \p buf with \p len random bytes. This is the default implementation for
 * prng(). You might want to change prng() to use a better PRNG on your specific
 * platform.
 */
COAP_STATIC_INLINE int
coap_prng_impl( unsigned char *buf, size_t len ) {
        while ( len != 0 ) {
                uint32_t r = 0;
                size_t i;
                if ( rand_s( &r ) != 0 )
                        return 0;
                for ( i = 0; i < len && i < 4; i++ ) {
                        *buf++ = (uint8_t)r;
                        r >>= 8;
                }
                len -= i;
        }
        return 1;
}

#else
#include <stdlib.h>

 /**
 * Fills \p buf with \p len random bytes. This is the default implementation for
 * prng(). You might want to change prng() to use a better PRNG on your specific
 * platform.
 */
COAP_STATIC_INLINE int
coap_prng_impl( unsigned char *buf, size_t len ) {
        while ( len-- )
                *buf++ = rand() & 0xFF;
        return 1;
}
#endif


#ifndef prng
/**
 * Fills \p Buf with \p Length bytes of random data.
 *
 * @hideinitializer
 */
#define prng(Buf,Length) coap_prng_impl((Buf), (Length))
#endif

#ifndef prng_init
/**
 * Called to set the PRNG seed. You may want to re-define this to allow for a
 * better PRNG.
 *
 * @hideinitializer
 */
#define prng_init(Value) srand((unsigned long)(Value))
#endif

/** @} */

#endif /* COAP_PRNG_H_ */
