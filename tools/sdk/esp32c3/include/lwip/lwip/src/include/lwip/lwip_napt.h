/**
 * @file lwip_napt.h
 * public API of ip4_napt
 *
 * @see ip4_napt.c
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * original reassembly code by Adam Dunkels <adam@sics.se>
 *
 */

#ifndef __LWIP_NAPT_H__
#define __LWIP_NAPT_H__

#include "lwip/opt.h"

#ifdef __cplusplus
extern "C" {
#endif

#if ESP_LWIP
#if IP_FORWARD
#if IP_NAPT

/* Default size of the tables used for NAPT */
#ifndef IP_NAPT_MAX
#define IP_NAPT_MAX 512
#endif
#ifndef IP_PORTMAP_MAX
#define IP_PORTMAP_MAX 32
#endif

/* Timeouts in sec for the various protocol types */
#ifndef IP_NAPT_TIMEOUT_MS_TCP
#define IP_NAPT_TIMEOUT_MS_TCP (30*60*1000)
#endif
#ifndef IP_NAPT_TIMEOUT_MS_TCP_DISCON
#define IP_NAPT_TIMEOUT_MS_TCP_DISCON (TCP_MSL)
#endif
#ifndef IP_NAPT_TIMEOUT_MS_UDP
#define IP_NAPT_TIMEOUT_MS_UDP (2*1000)
#endif
#ifndef IP_NAPT_TIMEOUT_MS_ICMP
#define IP_NAPT_TIMEOUT_MS_ICMP (2*1000)
#endif
#ifndef IP_NAPT_PORT_RANGE_START
#define IP_NAPT_PORT_RANGE_START 49152
#endif
#ifndef IP_NAPT_PORT_RANGE_END
#define IP_NAPT_PORT_RANGE_END   61439
#endif

/**
 * Enable/Disable NAPT for a specified interface.
 *
 * @param addr ip address of the interface
 * @param enable non-zero to enable NAPT, or 0 to disable.
 */
void
ip_napt_enable(u32_t addr, int enable);


/**
 * Enable/Disable NAPT for a specified interface.
 *
 * @param number number of the interface
 * @param enable non-zero to enable NAPT, or 0 to disable.
 */
void
ip_napt_enable_no(u8_t number, int enable);

/**
 * Register port mapping on the external interface to internal interface.
 * When the same port mapping is registered again, the old mapping is overwritten.
 * In this implementation, only 1 unique port mapping can be defined for each target address/port.
 *
 * @param proto target protocol
 * @param maddr ip address of the external interface
 * @param mport mapped port on the external interface, in host byte order.
 * @param daddr destination ip address
 * @param dport destination port, in host byte order.
 */
u8_t
ip_portmap_add(u8_t proto, u32_t maddr, u16_t mport, u32_t daddr, u16_t dport);

u8_t
ip_portmap_get(u8_t proto, u16_t mport, u32_t *maddr, u32_t *daddr, u16_t *dport);


/**
 * Unregister port mapping on the external interface to internal interface.
 *
 * @param proto target protocol
 * @param mport mapped port on the external interface, in host byte order.
 */
u8_t
ip_portmap_remove(u8_t proto, u16_t mport);



#if LWIP_STATS
/**
 * Get statistics.
 *
 * @param stats struct to receive current stats
 */
void
ip_napt_get_stats(struct stats_ip_napt *stats);
#endif

#endif /* IP_NAPT */
#endif /* IP_FORWARD */
#endif /* ESP_LWIP */

#ifdef __cplusplus
}
#endif

#endif /* __LWIP_NAPT_H__ */
