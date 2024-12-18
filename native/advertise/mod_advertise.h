/*
 * Copyright The mod_cluster Project Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MOD_ADVERTISE_H
#define MOD_ADVERTISE_H

/**
 * @file  mod_advertise.h
 * @brief Advertise Module for Apache Httpd
 * @author Mladen Turk
 *
 * @defgroup MOD_ADVERTISE mod_advertise
 * @ingroup  APACHE_MODS
 * @{
 */

#define CORE_PRIVATE

#include "apr_md5.h"
#include "apr_strings.h"
#include "apr_uuid.h"

#define APR_WANT_STRFUNC
#include "apr_want.h"

#include "http_protocol.h"
#include "http_log.h"
#include "util_time.h"
#include "ap_provider.h"

#if APR_HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if APR_HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#if !APR_HAS_THREADS
#error This module does not compile unless you have a thread capable APR!
#endif

#ifdef __cplusplus
extern "C" {
#endif


#define MA_BSIZE            4096
#define MA_SSIZE            1024
#define MA_DEFAULT_ADVPORT  23364
#define MA_DEFAULT_GROUP    "224.0.1.105"
#define MA_TM_RESOLUTION    APR_TIME_C(100000)
#define MA_DEFAULT_ADV_FREQ apr_time_from_sec(10)
#define MA_TM_MAINTAIN_STEP 10

/**
 * Multicast Time to Live (ttl) for a advertise transmission.
 */
#define MA_ADVERTISE_HOPS   10

/**
 * Advertise protocol types
 */
#define MA_ADVERTISE_SERVER 0
#define MA_ADVERTISE_STATUS 1

/**
 * Advertise mode enumeration.
 */
typedef enum { ma_advertise_off, ma_advertise_on } ma_advertise_e;

/**
 * Advertise header data structure
 */
typedef struct ma_advertise_hdr_t ma_advertise_hdr_t;
struct ma_advertise_hdr_t
{
    int type;
    apr_status_t status;
    char suuid[APR_UUID_FORMATTED_LENGTH + 1];
};

/**
 * Advertise server data structure
 */
typedef struct ma_advertise_srv_t ma_advertise_srv_t;
struct ma_advertise_srv_t
{
    const char *handle;
    const char *address;
    const char *protocol;
    server_rec *server;
    apr_port_t port;
};


#ifdef __cplusplus
}
#endif

/** @} */
#endif /* MOD_ADVERTISE_H */
