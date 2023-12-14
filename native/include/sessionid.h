/*
 * Copyright The mod_cluster Project Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SESSIONID_H
#define SESSIONID_H

/**
 * @file  sessionid.h
 * @brief sessionid description Storage Module for Apache
 * @author Jean-Frederic Clere
 *
 * @defgroup MEM sessionids
 * @ingroup  APACHE_MODS
 * @{
 */

#define SESSIONIDEXE ".sessionid"

#ifndef MEM_T
typedef struct mem mem_t;
#define MEM_T
#endif

#include "mod_clustersize.h"

/**
 * Status of the sessionid as read/store in httpd
 */
struct sessionidinfo
{
    /* NOTE: Due to `loc_get_id`, struct MUST begin with id */
    int id;                          /* id in table */
    char sessionid[SESSIONIDSZ + 1]; /* Sessionid value */
    char JVMRoute[JVMROUTESZ + 1];   /* corresponding node */

    apr_time_t updatetime; /* time of last received message */
};
typedef struct sessionidinfo sessionidinfo_t;

/**
 * Use apache httpd structure
 */
typedef struct ap_slotmem_provider_t slotmem_storage_method;

/**
 * Insert(alloc) and update a sessionid record in the shared table
 * @param s pointer to the shared table
 * @param sessionid sessionid to store in the shared table
 * @return APR_SUCCESS if all went well
 */
apr_status_t insert_update_sessionid(mem_t *s, sessionidinfo_t *sessionid);

/**
 * Read a sessionid record from the shared table
 * @param s pointer to the shared table
 * @param sessionid sessionid to read from the shared table
 * @return address of the read sessionid or NULL if error
 */
sessionidinfo_t *read_sessionid(mem_t *s, sessionidinfo_t *sessionid);

/**
 * Get a sessionid record from the shared table
 * @param s pointer to the shared table
 * @param sessionid address of the sessionid read from the shared table
 * @param ids id of the sessionid to return
 * @return APR_SUCCESS if all went well
 */
apr_status_t get_sessionid(mem_t *s, sessionidinfo_t **sessionid, int ids);

/**
 * Remove(free) a sessionid record from the shared table
 * @param s pointer to the shared table
 * @param sessionid sessionid to remove from the shared table
 * @return APR_SUCCESS if all went well
 */
apr_status_t remove_sessionid(mem_t *s, sessionidinfo_t *sessionid);

/**
 * Get the ids for the used (not free) sessionids in the table
 * @param s pointer to the shared table
 * @param ids array of int to store the used id (must be big enough)
 * @return number of sessionid existing or -1 if error
 */
int get_ids_used_sessionid(mem_t *s, int *ids);

/**
 * Get the size of the table (max size)
 * @param s pointer to the shared table
 * @return size of the existing table or -1 if error
 */
int get_max_size_sessionid(mem_t *s);

/**
 * Attach to the shared sessionid table
 * @param string name of an existing shared table
 * @param num address to store the size of the shared table
 * @param p pool to use for allocations
 * @param storage storage provider
 * @return address of struct used to access the table
 */
mem_t *get_mem_sessionid(char *string, unsigned *num, apr_pool_t *p, slotmem_storage_method *storage);

/**
 * Create a shared sessionid table
 * @param string name to use to create the table
 * @param num size of the shared table
 * @param persist tell if the slotmem element are persistent
 * @param p pool to use for allocations
 * @param storage storage provider
 * @return address of struct used to access the table
 */
mem_t *create_mem_sessionid(char *string, unsigned *num, int persist, apr_pool_t *p, slotmem_storage_method *storage);

/**
 * Provider for the mod_proxy_cluster or mod_jk modules
 */
struct sessionid_storage_method
{
    /**
     * The sessionid corresponding to the ident
     * @param ids ident of the sessionid to read
     * @param sessionid address of pointer to return the sessionid
     * @return APR_SUCCESS if all went well
     */
    apr_status_t (*read_sessionid)(int ids, sessionidinfo_t **sessionid);
    /**
     * Read the list of ident of used sessionids
     * @param ids address to store the idents
     * @return APR_SUCCESS if all went well
     */
    int (*get_ids_used_sessionid)(int *ids);
    /**
     * Read the max number of sessionids in the shared table
     */
    int (*get_max_size_sessionid)(void);
    /**
     * Remove the sessionid from shared memory (free the slotmem)
     */
    apr_status_t (*remove_sessionid)(sessionidinfo_t *sessionid);
    /**
     * Insert a new sessionid or update existing one
     */
    apr_status_t (*insert_update_sessionid)(sessionidinfo_t *sessionid);
};
#endif /*SESSIONID_H*/
