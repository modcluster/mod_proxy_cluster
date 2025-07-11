/*
 * Copyright The mod_cluster Project Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HOST_H
#define HOST_H

/**
 * @file  host.h
 * @brief host description Storage Module for Apache
 * @author Jean-Frederic Clere
 *
 * @defgroup MEM hosts
 * @ingroup  APACHE_MODS
 * @{
 */

#define HOSTEXE ".hosts"

#ifndef MEM_T
typedef struct mem mem_t;
#define MEM_T
#endif

#include "mod_clustersize.h"

/**
 * Status of the host as read/store in httpd
 */
struct hostinfo
{
    /* NOTE: Due to `loc_get_id`, struct MUST begin with id */
    int id;                    /* id in table */
    char host[HOSTALIASZ + 1]; /* Alias element of the virtual host */
    int vhost;                 /* id of the correspond virtual host */
    int node;                  /* id of the node containing the virtual host */

    apr_time_t updatetime; /* time of last received message */
};
typedef struct hostinfo hostinfo_t;

/**
 * Insert(alloc) and update a host record in the shared table
 * @param s pointer to the shared table
 * @param host host to store in the shared table
 * @return APR_SUCCESS if all went well
 */
apr_status_t insert_update_host(mem_t *s, hostinfo_t *host);

/**
 * Read a host record from the shared table
 * @param s pointer to the shared table
 * @param host host to read from the shared table
 * @return address of the read host or NULL if error
 */
hostinfo_t *read_host(mem_t *s, hostinfo_t *host);

/**
 * Get a host record from the shared table
 * @param s pointer to the shared table
 * @param host address of the host read from the shared table
 * @param ids id of the host to return
 * @return APR_SUCCESS if all went well
 */
apr_status_t get_host(mem_t *s, hostinfo_t **host, int ids);

/**
 * Remove(free) a host record from the shared table
 * @param s pointer to the shared table
 * @param id id id of host to remove from the shared table
 * @return APR_SUCCESS if all went well
 */
apr_status_t remove_host(mem_t *s, int id);

/**
 * Get the ids for the used (not free) hosts in the table
 * @param s pointer to the shared table
 * @param ids array of int to store the used id (must be big enough)
 * @return number of host existing or -1 if error
 */
int get_ids_used_host(mem_t *s, int *ids);

/**
 * Get the size of the table (max size)
 * @param s pointer to the shared table
 * @return size of the existing table or -1 if error
 */
int get_max_size_host(mem_t *s);

/**
 * Attach to the shared host table
 * @param string name of an existing shared table
 * @param num address to store the size of the shared table
 * @param p pool to use for allocations
 * @param storage slotmem logic provider
 * @return address of struct used to access the table
 */
mem_t *get_mem_host(char *string, unsigned *num, apr_pool_t *p, slotmem_storage_method *storage);

/**
 * Create a shared host table
 * @param string name to use to create the table
 * @param num size of the shared table
 * @param persist tell if the slotmem element are persistent
 * @param p pool to use for allocations
 * @param storage slotmem logic provider
 * @return address of struct used to access the table
 */
mem_t *create_mem_host(char *string, unsigned *num, int persist, apr_pool_t *p, slotmem_storage_method *storage);

/**
 * Provider for the mod_proxy_cluster or mod_jk modules
 */
struct host_storage_method
{
    /**
     * The host corresponding to the ident
     * @param ids ident of the host to read
     * @param host address of pointer to return the host
     * @return APR_SUCCESS if all went well
     */
    apr_status_t (*read_host)(int ids, hostinfo_t **host);
    /**
     * Read the list of ident of used hosts
     * @param ids address to store the idents
     * @return APR_SUCCESS if all went well
     */
    int (*get_ids_used_host)(int *ids);
    /**
     * Read the max number of hosts in the shared table
     * @return the maximum size of the table
     */
    int (*get_max_size_host)(void);
};
#endif /*HOST_H*/
