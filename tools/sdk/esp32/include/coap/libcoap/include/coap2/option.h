/*
 * option.h -- helpers for handling options in CoAP PDUs
 *
 * Copyright (C) 2010-2013 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/**
 * @file option.h
 * @brief Helpers for handling options in CoAP PDUs
 */

#ifndef COAP_OPTION_H_
#define COAP_OPTION_H_

#include "bits.h"
#include "pdu.h"

/**
 * Use byte-oriented access methods here because sliding a complex struct
 * coap_opt_t over the data buffer may cause bus error on certain platforms.
 */
typedef uint8_t coap_opt_t;
#define PCHAR(p) ((coap_opt_t *)(p))

/**
 * Representation of CoAP options.
 */
typedef struct {
  uint16_t delta;
  size_t length;
  const uint8_t *value;
} coap_option_t;

/**
 * Parses the option pointed to by @p opt into @p result. This function returns
 * the number of bytes that have been parsed, or @c 0 on error. An error is
 * signaled when illegal delta or length values are encountered or when option
 * parsing would result in reading past the option (i.e. beyond opt + length).
 *
 * @param opt    The beginning of the option to parse.
 * @param length The maximum length of @p opt.
 * @param result A pointer to the coap_option_t structure that is filled with
 *               actual values iff coap_opt_parse() > 0.
 * @return       The number of bytes parsed or @c 0 on error.
 */
size_t coap_opt_parse(const coap_opt_t *opt,
                      size_t length,
                      coap_option_t *result);

/**
 * Returns the size of the given option, taking into account a possible option
 * jump.
 *
 * @param opt An option jump or the beginning of the option.
 * @return    The number of bytes between @p opt and the end of the option
 *            starting at @p opt. In case of an error, this function returns
 *            @c 0 as options need at least one byte storage space.
 */
size_t coap_opt_size(const coap_opt_t *opt);

/**
 * @defgroup opt_filter Option Filters
 * API functions for access option filters
 * @{
 */

/**
 * The number of option types below 256 that can be stored in an
 * option filter. COAP_OPT_FILTER_SHORT + COAP_OPT_FILTER_LONG must be
 * at most 16. Each coap_option_filter_t object reserves
 * ((COAP_OPT_FILTER_SHORT + 1) / 2) * 2 bytes for short options.
 */
#define COAP_OPT_FILTER_SHORT 6

/**
 * The number of option types above 255 that can be stored in an
 * option filter. COAP_OPT_FILTER_SHORT + COAP_OPT_FILTER_LONG must be
 * at most 16. Each coap_option_filter_t object reserves
 * COAP_OPT_FILTER_LONG * 2 bytes for short options.
 */
#define COAP_OPT_FILTER_LONG  2

/* Ensure that COAP_OPT_FILTER_SHORT and COAP_OPT_FILTER_LONG are set
 * correctly. */
#if (COAP_OPT_FILTER_SHORT + COAP_OPT_FILTER_LONG > 16)
#error COAP_OPT_FILTER_SHORT + COAP_OPT_FILTER_LONG must be less or equal 16
#endif /* (COAP_OPT_FILTER_SHORT + COAP_OPT_FILTER_LONG > 16) */

/** The number of elements in coap_opt_filter_t. */
#define COAP_OPT_FILTER_SIZE                                        \
  (((COAP_OPT_FILTER_SHORT + 1) >> 1) + COAP_OPT_FILTER_LONG) +1

/**
 * Fixed-size vector we use for option filtering. It is large enough
 * to hold COAP_OPT_FILTER_SHORT entries with an option number between
 * 0 and 255, and COAP_OPT_FILTER_LONG entries with an option number
 * between 256 and 65535. Its internal structure is
 *
 * @code
struct {
  uint16_t mask;
  uint16_t long_opts[COAP_OPT_FILTER_LONG];
  uint8_t short_opts[COAP_OPT_FILTER_SHORT];
}
 * @endcode
 *
 * The first element contains a bit vector that indicates which fields
 * in the remaining array are used. The first COAP_OPT_FILTER_LONG
 * bits correspond to the long option types that are stored in the
 * elements from index 1 to COAP_OPT_FILTER_LONG. The next
 * COAP_OPT_FILTER_SHORT bits correspond to the short option types
 * that are stored in the elements from index COAP_OPT_FILTER_LONG + 1
 * to COAP_OPT_FILTER_LONG + COAP_OPT_FILTER_SHORT. The latter
 * elements are treated as bytes.
 */
typedef uint16_t coap_opt_filter_t[COAP_OPT_FILTER_SIZE];

/** Pre-defined filter that includes all options. */
#define COAP_OPT_ALL NULL

/**
 * Clears filter @p f.
 *
 * @param f The filter to clear.
 */
COAP_STATIC_INLINE void
coap_option_filter_clear(coap_opt_filter_t f) {
  memset(f, 0, sizeof(coap_opt_filter_t));
}

/**
 * Sets the corresponding entry for @p type in @p filter. This
 * function returns @c 1 if bit was set or @c 0 on error (i.e. when
 * the given type does not fit in the filter).
 *
 * @param filter The filter object to change.
 * @param type   The type for which the bit should be set.
 *
 * @return       @c 1 if bit was set, @c 0 otherwise.
 */
int coap_option_filter_set(coap_opt_filter_t filter, uint16_t type);

/**
 * Clears the corresponding entry for @p type in @p filter. This
 * function returns @c 1 if bit was set or @c 0 on error (i.e. when
 * the given type does not fit in the filter).
 *
 * @param filter The filter object to change.
 * @param type   The type that should be cleared from the filter.
 *
 * @return       @c 1 if bit was set, @c 0 otherwise.
 */
int coap_option_filter_unset(coap_opt_filter_t filter, uint16_t type);

/**
 * Checks if @p type is contained in @p filter. This function returns
 * @c 1 if found, @c 0 if not, or @c -1 on error (i.e. when the given
 * type does not fit in the filter).
 *
 * @param filter The filter object to search.
 * @param type   The type to search for.
 *
 * @return       @c 1 if @p type was found, @c 0 otherwise, or @c -1 on error.
 */
int coap_option_filter_get(coap_opt_filter_t filter, uint16_t type);

/**
 * Sets the corresponding bit for @p type in @p filter. This function returns @c
 * 1 if bit was set or @c -1 on error (i.e. when the given type does not fit in
 * the filter).
 *
 * @deprecated Use coap_option_filter_set() instead.
 *
 * @param filter The filter object to change.
 * @param type   The type for which the bit should be set.
 *
 * @return       @c 1 if bit was set, @c -1 otherwise.
 */
COAP_STATIC_INLINE int
coap_option_setb(coap_opt_filter_t filter, uint16_t type) {
  return coap_option_filter_set(filter, type) ? 1 : -1;
}

/**
 * Clears the corresponding bit for @p type in @p filter. This function returns
 * @c 1 if bit was cleared or @c -1 on error (i.e. when the given type does not
 * fit in the filter).
 *
 * @deprecated Use coap_option_filter_unset() instead.
 *
 * @param filter The filter object to change.
 * @param type   The type for which the bit should be cleared.
 *
 * @return       @c 1 if bit was set, @c -1 otherwise.
 */
COAP_STATIC_INLINE int
coap_option_clrb(coap_opt_filter_t filter, uint16_t type) {
  return coap_option_filter_unset(filter, type) ? 1 : -1;
}

/**
 * Gets the corresponding bit for @p type in @p filter. This function returns @c
 * 1 if the bit is set @c 0 if not, or @c -1 on error (i.e. when the given type
 * does not fit in the filter).
 *
 * @deprecated Use coap_option_filter_get() instead.
 *
 * @param filter The filter object to read bit from.
 * @param type   The type for which the bit should be read.
 *
 * @return       @c 1 if bit was set, @c 0 if not, @c -1 on error.
 */
COAP_STATIC_INLINE int
coap_option_getb(coap_opt_filter_t filter, uint16_t type) {
  return coap_option_filter_get(filter, type);
}

/**
 * Iterator to run through PDU options. This object must be
 * initialized with coap_option_iterator_init(). Call
 * coap_option_next() to walk through the list of options until
 * coap_option_next() returns @c NULL.
 *
 * @code
 * coap_opt_t *option;
 * coap_opt_iterator_t opt_iter;
 * coap_option_iterator_init(pdu, &opt_iter, COAP_OPT_ALL);
 *
 * while ((option = coap_option_next(&opt_iter))) {
 *   ... do something with option ...
 * }
 * @endcode
 */
typedef struct {
  size_t length;                /**< remaining length of PDU */
  uint16_t type;                /**< decoded option type */
  unsigned int bad:1;           /**< iterator object is ok if not set */
  unsigned int filtered:1;      /**< denotes whether or not filter is used */
  coap_opt_t *next_option;      /**< pointer to the unparsed next option */
  coap_opt_filter_t filter;     /**< option filter */
} coap_opt_iterator_t;

/**
 * Initializes the given option iterator @p oi to point to the beginning of the
 * @p pdu's option list. This function returns @p oi on success, @c NULL
 * otherwise (i.e. when no options exist). Note that a length check on the
 * option list must be performed before coap_option_iterator_init() is called.
 *
 * @param pdu    The PDU the options of which should be walked through.
 * @param oi     An iterator object that will be initilized.
 * @param filter An optional option type filter.
 *               With @p type != @c COAP_OPT_ALL, coap_option_next()
 *               will return only options matching this bitmask.
 *               Fence-post options @c 14, @c 28, @c 42, ... are always
 *               skipped.
 *
 * @return       The iterator object @p oi on success, @c NULL otherwise.
 */
coap_opt_iterator_t *coap_option_iterator_init(const coap_pdu_t *pdu,
                                               coap_opt_iterator_t *oi,
                                               const coap_opt_filter_t filter);

/**
 * Updates the iterator @p oi to point to the next option. This function returns
 * a pointer to that option or @c NULL if no more options exist. The contents of
 * @p oi will be updated. In particular, @c oi->n specifies the current option's
 * ordinal number (counted from @c 1), @c oi->type is the option's type code,
 * and @c oi->option points to the beginning of the current option itself. When
 * advanced past the last option, @c oi->option will be @c NULL.
 *
 * Note that options are skipped whose corresponding bits in the filter
 * specified with coap_option_iterator_init() are @c 0. Options with type codes
 * that do not fit in this filter hence will always be returned.
 *
 * @param oi The option iterator to update.
 *
 * @return   The next option or @c NULL if no more options exist.
 */
coap_opt_t *coap_option_next(coap_opt_iterator_t *oi);

/**
 * Retrieves the first option of type @p type from @p pdu. @p oi must point to a
 * coap_opt_iterator_t object that will be initialized by this function to
 * filter only options with code @p type. This function returns the first option
 * with this type, or @c NULL if not found.
 *
 * @param pdu  The PDU to parse for options.
 * @param type The option type code to search for.
 * @param oi   An iterator object to use.
 *
 * @return     A pointer to the first option of type @p type, or @c NULL if
 *             not found.
 */
coap_opt_t *coap_check_option(coap_pdu_t *pdu,
                              uint16_t type,
                              coap_opt_iterator_t *oi);

/**
 * Encodes the given delta and length values into @p opt. This function returns
 * the number of bytes that were required to encode @p delta and @p length or @c
 * 0 on error. Note that the result indicates by how many bytes @p opt must be
 * advanced to encode the option value.
 *
 * @param opt    The option buffer space where @p delta and @p length are
 *               written.
 * @param maxlen The maximum length of @p opt.
 * @param delta  The actual delta value to encode.
 * @param length The actual length value to encode.
 *
 * @return       The number of bytes used or @c 0 on error.
 */
size_t coap_opt_setheader(coap_opt_t *opt,
                          size_t maxlen,
                          uint16_t delta,
                          size_t length);

/**
 * Compute storage bytes needed for an option with given @p delta and
 * @p length
 *
 * @param delta  The option delta.
 * @param length The option length.
 *
 * @return       The number of bytes required to encode this option.
 */
size_t coap_opt_encode_size(uint16_t delta, size_t length);

/**
 * Encodes option with given @p delta into @p opt. This function returns the
 * number of bytes written to @p opt or @c 0 on error. This happens especially
 * when @p opt does not provide sufficient space to store the option value,
 * delta, and option jumps when required.
 *
 * @param opt    The option buffer space where @p val is written.
 * @param n      Maximum length of @p opt.
 * @param delta  The option delta.
 * @param val    The option value to copy into @p opt.
 * @param length The actual length of @p val.
 *
 * @return       The number of bytes that have been written to @p opt or @c 0 on
 *               error. The return value will always be less than @p n.
 */
size_t coap_opt_encode(coap_opt_t *opt,
                       size_t n,
                       uint16_t delta,
                       const uint8_t *val,
                       size_t length);

/**
 * Decodes the delta value of the next option. This function returns the number
 * of bytes read or @c 0 on error. The caller of this function must ensure that
 * it does not read over the boundaries of @p opt (e.g. by calling
 * coap_opt_check_delta().
 *
 * @param opt The option to examine.
 *
 * @return    The number of bytes read or @c 0 on error.
 */
uint16_t coap_opt_delta(const coap_opt_t *opt);

/**
 * Returns the length of the given option. @p opt must point to an option jump
 * or the beginning of the option. This function returns @c 0 when @p opt is not
 * an option or the actual length of @p opt (which can be @c 0 as well).
 *
 * @note {The rationale for using @c 0 in case of an error is that in most
 * contexts, the result of this function is used to skip the next
 * coap_opt_length() bytes.}
 *
 * @param opt  The option whose length should be returned.
 *
 * @return     The option's length or @c 0 when undefined.
 */
uint16_t coap_opt_length(const coap_opt_t *opt);

/**
 * Returns a pointer to the value of the given option. @p opt must point to an
 * option jump or the beginning of the option. This function returns @c NULL if
 * @p opt is not a valid option.
 *
 * @param opt The option whose value should be returned.
 *
 * @return    A pointer to the option value or @c NULL on error.
 */
const uint8_t *coap_opt_value(const coap_opt_t *opt);

/** @} */

/**
 * Representation of chained list of CoAP options to install.
 *
 * @code
 * coap_optlist_t *optlist_chain = NULL;
 * coap_pdu_t *pdu = coap_new_pdu(session);
 *
 * ... other set up code ...
 * coap_insert_optlist(&optlist_chain, coap_new_optlist(COAP_OPTION_OBSERVE,
 *                    COAP_OBSERVE_ESTABLISH, NULL));
 *
 * coap_add_optlist_pdu(pdu, &optlist_chain);
 * ... other code ...
 * coap_delete_optlist(optlist_chain);
 * @endcode
 */
typedef struct coap_optlist_t {
  struct coap_optlist_t *next;  /**< next entry in the optlist chain */
  uint16_t number;              /**< the option number (no delta coding) */
  size_t length;                /**< the option value length */
  uint8_t *data;                /**< the option data */
} coap_optlist_t;

/**
 * Create a new optlist entry.
 *
 * @param number    The option number (COAP_OPTION_*)
 * @param length    The option length
 * @param data      The option value data
 *
 * @return          A pointer to the new optlist entry, or @c NULL if error
 */
coap_optlist_t *coap_new_optlist(uint16_t number,
                                 size_t length,
                                 const uint8_t *data);

/**
 * The current optlist of @p optlist_chain is first sorted (as per RFC7272
 * ordering requirements) and then added to the @p pdu.
 *
 * @param pdu  The pdu to add the options to from the chain list
 * @param optlist_chain The chained list of optlist to add to the pdu
 *
 * @return     @c 1 if succesful or @c 0 if failure;
 */
int coap_add_optlist_pdu(coap_pdu_t *pdu, coap_optlist_t** optlist_chain);

/**
 * Adds @p optlist to the given @p optlist_chain. The optlist_chain variable
 * be set to NULL before the initial call to coap_insert_optlist().
 * The optlist_chain will need to be deleted using coap_delete_optlist()
 * when no longer required.
 *
 * @param optlist_chain The chain to add optlist to
 * @param optlist  The optlist to add to the queue
 *
 * @return         @c 1 if successful, @c 0 otherwise.
 */
int coap_insert_optlist(coap_optlist_t **optlist_chain,
                        coap_optlist_t *optlist);

/**
 * Removes all entries from the @p optlist_chain, freeing off their
 * memory usage.
 *
 * @param optlist_chain The optlist chain to remove all the entries from
 */
void coap_delete_optlist(coap_optlist_t *optlist_chain);

#endif /* COAP_OPTION_H_ */
