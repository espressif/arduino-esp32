/*
 * bits.h -- bit vector manipulation
 *
 * Copyright (C) 2010-2011 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file bits.h
 * @brief Bit vector manipulation
 */

#ifndef COAP_BITS_H_
#define COAP_BITS_H_

#include <stdint.h>

/**
 * Sets the bit @p bit in bit-vector @p vec. This function returns @c 1 if bit
 * was set or @c -1 on error (i.e. when the given bit does not fit in the
 * vector).
 *
 * @param vec  The bit-vector to change.
 * @param size The size of @p vec in bytes.
 * @param bit  The bit to set in @p vec.
 *
 * @return     @c -1 if @p bit does not fit into @p vec, @c 1 otherwise.
 */
COAP_STATIC_INLINE int
bits_setb(uint8_t *vec, size_t size, uint8_t bit) {
  if (size <= ((size_t)bit >> 3))
    return -1;

  *(vec + (bit >> 3)) |= (uint8_t)(1 << (bit & 0x07));
  return 1;
}

/**
 * Clears the bit @p bit from bit-vector @p vec. This function returns @c 1 if
 * bit was cleared or @c -1 on error (i.e. when the given bit does not fit in
 * the vector).
 *
 * @param vec  The bit-vector to change.
 * @param size The size of @p vec in bytes.
 * @param bit  The bit to clear from @p vec.
 *
 * @return     @c -1 if @p bit does not fit into @p vec, @c 1 otherwise.
 */
COAP_STATIC_INLINE int
bits_clrb(uint8_t *vec, size_t size, uint8_t bit) {
  if (size <= ((size_t)bit >> 3))
    return -1;

  *(vec + (bit >> 3)) &= (uint8_t)(~(1 << (bit & 0x07)));
  return 1;
}

/**
 * Gets the status of bit @p bit from bit-vector @p vec. This function returns
 * @c 1 if the bit is set, @c 0 otherwise (even in case of an error).
 *
 * @param vec  The bit-vector to read from.
 * @param size The size of @p vec in bytes.
 * @param bit  The bit to get from @p vec.
 *
 * @return     @c 1 if the bit is set, @c 0 otherwise.
 */
COAP_STATIC_INLINE int
bits_getb(const uint8_t *vec, size_t size, uint8_t bit) {
  if (size <= ((size_t)bit >> 3))
    return -1;

  return (*(vec + (bit >> 3)) & (1 << (bit & 0x07))) != 0;
}

#endif /* COAP_BITS_H_ */
