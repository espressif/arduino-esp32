/*
 * coap_debug.h -- debug utilities
 *
 * Copyright (C) 2010-2011,2014 Olaf Bergmann <bergmann@tzi.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef COAP_DEBUG_H_
#define COAP_DEBUG_H_

/**
 * @defgroup logging Logging Support
 * API functions for logging support
 * @{
 */

#ifndef COAP_DEBUG_FD
/**
 * Used for output for @c LOG_DEBUG to @c LOG_ERR.
 */
#define COAP_DEBUG_FD stdout
#endif

#ifndef COAP_ERR_FD
/**
 * Used for output for @c LOG_CRIT to @c LOG_EMERG.
 */
#define COAP_ERR_FD stderr
#endif

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
/**
 * Logging type.  One of LOG_* from @b syslog.
 */
typedef short coap_log_t;
/*
   LOG_DEBUG+2 gives ciphers in GnuTLS
   Use COAP_LOG_CIPHERS to output Cipher Info in OpenSSL etc.
 */
#define COAP_LOG_CIPHERS (LOG_DEBUG+2)
#else /* !HAVE_SYSLOG_H */
/** Pre-defined log levels akin to what is used in \b syslog
    with LOG_CIPHERS added. */

#if !defined(RIOT_VERSION)
typedef enum {
  LOG_EMERG=0, /**< Emergency */
  LOG_ALERT,   /**< Alert */
  LOG_CRIT,    /**< Critical */
  LOG_ERR,     /**< Error */
  LOG_WARNING, /**< Warning */
  LOG_NOTICE,  /**< Notice */
  LOG_INFO,    /**< Information */
  LOG_DEBUG,   /**< Debug */
  COAP_LOG_CIPHERS=LOG_DEBUG+2 /**< CipherInfo */
} coap_log_t;
#else /* RIOT_VERSION */
/* RIOT defines a subset of the syslog levels in log.h with different
 * numeric values. The remaining levels are defined here. Note that
 * output granularity differs from what would be expected when
 * adhering to the syslog levels.
 */
#include <log.h>
typedef short coap_log_t;
#define LOG_EMERG  (0)
#define LOG_ALERT  (1)
#define LOG_CRIT   (2)
#define LOG_ERR    (3)
/*  LOG_WARNING    (4) */
#define LOG_NOTICE (5)
/*  LOG_INFO       (6) */
/*  LOG_DEBUG      (7) */
#define COAP_LOG_CIPHERS (9)
#endif /* RIOT_VERSION */

#endif /* !HAVE_SYSLOG_H */

/**
 * Get the current logging level.
 *
 * @return One of the LOG_* values.
 */
coap_log_t coap_get_log_level(void);

/**
 * Sets the log level to the specified value.
 *
 * @param level One of the LOG_* values.
 */
void coap_set_log_level(coap_log_t level);

/**
 * Logging callback handler definition.
 *
 * @param level One of the LOG_* values.
 * @param message Zero-terminated string message to log.
 */
typedef void (*coap_log_handler_t) (coap_log_t level, const char *message);

/**
 * Add a custom log callback handler.
 *
 * @param handler The logging handler to use or @p NULL to use default handler.
 */
void coap_set_log_handler(coap_log_handler_t handler);

/**
 * Get the library package name.
 *
 * @return Zero-terminated string with the name of this library.
 */
const char *coap_package_name(void);

/**
 * Get the library package version.
 *
 * @return Zero-terminated string with the library version.
 */
const char *coap_package_version(void);

/**
 * Writes the given text to @c COAP_ERR_FD (for @p level <= @c LOG_CRIT) or @c
 * COAP_DEBUG_FD (for @p level >= @c LOG_ERR). The text is output only when
 * @p level is below or equal to the log level that set by coap_set_log_level().
 *
 * Internal function.
 *
 * @param level One of the LOG_* values.
 & @param format The format string to use.
 */
#if (defined(__GNUC__))
void coap_log_impl(coap_log_t level,
              const char *format, ...) __attribute__ ((format(printf, 2, 3)));
#else
void coap_log_impl(coap_log_t level, const char *format, ...);
#endif

#ifndef coap_log
/**
 * Logging function.
 * Writes the given text to @c COAP_ERR_FD (for @p level <= @c LOG_CRIT) or @c
 * COAP_DEBUG_FD (for @p level >= @c LOG_ERR). The text is output only when
 * @p level is below or equal to the log level that set by coap_set_log_level().
 *
 * @param level One of the LOG_* values.
 */
#define coap_log(level, ...) do { \
  if ((int)((level))<=(int)coap_get_log_level()) \
     coap_log_impl((level), __VA_ARGS__); \
} while(0)
#endif

#include "pdu.h"

/**
 * Defines the output mode for the coap_show_pdu() function.
 *
 * @param use_fprintf @p 1 if the output is to use fprintf() (the default)
 *                    @p 0 if the output is to use coap_log().
 */
void coap_set_show_pdu_output(int use_fprintf);

/**
 * Display the contents of the specified @p pdu.
 * Note: The output method of coap_show_pdu() is dependent on the setting of
 * coap_set_show_pdu_output().
 *
 * @param level The required minimum logging level.
 * @param pdu The PDU to decode.
 */
void coap_show_pdu(coap_log_t level, const coap_pdu_t *pdu);

/**
 * Display the current (D)TLS library linked with and built for version.
 *
 * @param level The required minimum logging level.
 */
void coap_show_tls_version(coap_log_t level);

/**
 * Build a string containing the current (D)TLS library linked with and
 * built for version.
 *
 * @param buffer The buffer to put the string into.
 * @param bufsize The size of the buffer to put the string into.
 *
 * @return A pointer to the provided buffer.
 */
char *coap_string_tls_version(char *buffer, size_t bufsize);

/**
 * Build a string containing the current (D)TLS library support
 *
 * @param buffer The buffer to put the string into.
 * @param bufsize The size of the buffer to put the string into.
 *
 * @return A pointer to the provided buffer.
 */
char *coap_string_tls_support(char *buffer, size_t bufsize);

/**
 * Print the address into the defined buffer.
 *
 * Internal Function.
 *
 * @param address The address to print.
 * @param buffer The buffer to print into.
 * @param size The size of the buffer to print into.
 *
 * @return The amount written into the buffer.
 */
size_t coap_print_addr(const coap_address_t *address,
                       unsigned char *buffer, size_t size);

/** @} */

/**
 * Set the packet loss level for testing.  This can be in one of two forms.
 *
 * Percentage : 0% to 100%.  Use the specified probability.
 * 0% is send all packets, 100% is drop all packets.
 *
 * List: A comma separated list of numbers or number ranges that are the
 * packets to drop.
 *
 * @param loss_level The defined loss level (percentage or list).
 *
 * @return @c 1 If loss level set, @c 0 if there is an error.
 */
int coap_debug_set_packet_loss(const char *loss_level);

/**
 * Check to see whether a packet should be sent or not.
 *
 * Internal function
 *
 * @return @c 1 if packet is to be sent, @c 0 if packet is to be dropped.
 */
int coap_debug_send_packet(void);


#endif /* COAP_DEBUG_H_ */
