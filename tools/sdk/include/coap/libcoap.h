/*
 * libcoap.h -- platform specific header file for CoAP stack
 *
 * Copyright (C) 2015 Carsten Schoenert <c.schoenert@t-online.de>
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef _LIBCOAP_H_
#define _LIBCOAP_H_

/* The non posix embedded platforms like Contiki, TinyOS, RIOT, ... doesn't have
 * a POSIX compatible header structure so we have to slightly do some platform
 * related things. Currently there is only Contiki available so we check for a
 * CONTIKI environment and do *not* include the POSIX related network stuff. If
 * there are other platforms in future there need to be analogous environments.
 *
 * The CONTIKI variable is within the Contiki build environment! */

#if !defined (CONTIKI) 
#include <netinet/in.h>
#include <sys/socket.h>
#endif /* CONTIKI */

#endif /* _LIBCOAP_H_ */
