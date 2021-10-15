/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

/* Memory pool definitions for the libcoap when used with lwIP (which has its
 * own mechanism for quickly allocating chunks of data with known sizes). Has
 * to be findable by lwIP (ie. an #include <lwippools.h> must either directly
 * include this or include something more generic which includes this), and
 * MEMP_USE_CUSTOM_POOLS has to be set in lwipopts.h. */

#include "coap_internal.h"
#include "net.h"
#include "resource.h"
#include "subscribe.h"

#ifndef MEMP_NUM_COAPCONTEXT
#define MEMP_NUM_COAPCONTEXT 1
#endif

#ifndef MEMP_NUM_COAPENDPOINT
#define MEMP_NUM_COAPENDPOINT 1
#endif

/* 1 is sufficient as this is very short-lived */
#ifndef MEMP_NUM_COAPPACKET
#define MEMP_NUM_COAPPACKET 1
#endif

#ifndef MEMP_NUM_COAPNODE
#define MEMP_NUM_COAPNODE 4
#endif

#ifndef MEMP_NUM_COAPPDU
#define MEMP_NUM_COAPPDU MEMP_NUM_COAPNODE
#endif

#ifndef MEMP_NUM_COAPSESSION
#define MEMP_NUM_COAPSESSION 2
#endif

#ifndef MEMP_NUM_COAP_SUBSCRIPTION
#define MEMP_NUM_COAP_SUBSCRIPTION 4
#endif

#ifndef MEMP_NUM_COAPRESOURCE
#define MEMP_NUM_COAPRESOURCE 10
#endif

#ifndef MEMP_NUM_COAPRESOURCEATTR
#define MEMP_NUM_COAPRESOURCEATTR 20
#endif

#ifndef MEMP_NUM_COAPOPTLIST
#define MEMP_NUM_COAPOPTLIST 1
#endif

#ifndef MEMP_LEN_COAPOPTLIST
#define MEMP_LEN_COAPOPTLIST 12
#endif

#ifndef MEMP_NUM_COAPSTRING
#define MEMP_NUM_COAPSTRING 10
#endif

#ifndef MEMP_LEN_COAPSTRING
#define MEMP_LEN_COAPSTRING 32
#endif

#ifndef MEMP_NUM_COAPCACHE_KEYS
#define MEMP_NUM_COAPCACHE_KEYS        (2U)
#endif /* MEMP_NUM_COAPCACHE_KEYS */

#ifndef MEMP_NUM_COAPCACHE_ENTRIES
#define MEMP_NUM_COAPCACHE_ENTRIES        (2U)
#endif /* MEMP_NUM_COAPCACHE_ENTRIES */

#ifndef MEMP_NUM_COAPPDUBUF
#define MEMP_NUM_COAPPDUBUF 2
#endif

#ifndef MEMP_LEN_COAPPDUBUF
#define MEMP_LEN_COAPPDUBUF 32
#endif

#ifndef MEMP_NUM_COAPLGXMIT
#define MEMP_NUM_COAPLGXMIT 2
#endif

#ifndef MEMP_NUM_COAPLGCRCV
#define MEMP_NUM_COAPLGCRCV 2
#endif

#ifndef MEMP_NUM_COAPLGSRCV
#define MEMP_NUM_COAPLGSRCV 2
#endif

LWIP_MEMPOOL(COAP_CONTEXT, MEMP_NUM_COAPCONTEXT, sizeof(coap_context_t), "COAP_CONTEXT")
LWIP_MEMPOOL(COAP_ENDPOINT, MEMP_NUM_COAPENDPOINT, sizeof(coap_endpoint_t), "COAP_ENDPOINT")
LWIP_MEMPOOL(COAP_PACKET, MEMP_NUM_COAPPACKET, sizeof(coap_packet_t), "COAP_PACKET")
LWIP_MEMPOOL(COAP_NODE, MEMP_NUM_COAPNODE, sizeof(coap_queue_t), "COAP_NODE")
LWIP_MEMPOOL(COAP_PDU, MEMP_NUM_COAPPDU, sizeof(coap_pdu_t), "COAP_PDU")
LWIP_MEMPOOL(COAP_SESSION, MEMP_NUM_COAPSESSION, sizeof(coap_session_t), "COAP_SESSION")
LWIP_MEMPOOL(COAP_subscription, MEMP_NUM_COAP_SUBSCRIPTION, sizeof(coap_subscription_t), "COAP_subscription")
LWIP_MEMPOOL(COAP_RESOURCE, MEMP_NUM_COAPRESOURCE, sizeof(coap_resource_t), "COAP_RESOURCE")
LWIP_MEMPOOL(COAP_RESOURCEATTR, MEMP_NUM_COAPRESOURCEATTR, sizeof(coap_attr_t), "COAP_RESOURCEATTR")
LWIP_MEMPOOL(COAP_OPTLIST, MEMP_NUM_COAPOPTLIST, sizeof(coap_optlist_t)+MEMP_LEN_COAPOPTLIST, "COAP_OPTLIST")
LWIP_MEMPOOL(COAP_STRING, MEMP_NUM_COAPSTRING, sizeof(coap_string_t)+MEMP_LEN_COAPSTRING, "COAP_STRING")
LWIP_MEMPOOL(COAP_CACHE_KEY, MEMP_NUM_COAPCACHE_KEYS, sizeof(coap_cache_key_t), "COAP_CACHE_KEY")
LWIP_MEMPOOL(COAP_CACHE_ENTRY, MEMP_NUM_COAPCACHE_ENTRIES, sizeof(coap_cache_entry_t), "COAP_CACHE_ENTRY")
LWIP_MEMPOOL(COAP_PDU_BUF, MEMP_NUM_COAPPDUBUF, MEMP_LEN_COAPPDUBUF, "COAP_PDU_BUF")
LWIP_MEMPOOL(COAP_LG_XMIT, MEMP_NUM_COAPLGXMIT, sizeof(coap_lg_xmit_t), "COAP_LG_XMIT")
LWIP_MEMPOOL(COAP_LG_CRCV, MEMP_NUM_COAPLGCRCV, sizeof(coap_lg_crcv_t), "COAP_LG_CRCV")
LWIP_MEMPOOL(COAP_LG_SRCV, MEMP_NUM_COAPLGSRCV, sizeof(coap_lg_srcv_t), "COAP_LG_SRCV")

