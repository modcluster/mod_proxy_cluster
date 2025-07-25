/*
 * Copyright The mod_cluster Project Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CONTEXT_H
#define CONTEXT_H

/**
 * @file  context.h
 * @brief context description Storage Module for Apache
 * @author Jean-Frederic Clere
 *
 * @defgroup MEM contexts
 * @ingroup  APACHE_MODS
 * @{
 */

#define CONTEXTEXE ".contexts"

#ifndef MEM_T
typedef struct mem mem_t;
#define MEM_T
#endif

/**
 * Status of the application
 */
#define ENABLED  1
#define DISABLED 2
#define STOPPED  3
#define REMOVE   4 /* That status not stored but used by the logic to remove the entry */

#include "mod_clustersize.h"

/**
 * Status of the context as read/store in httpd
 */
struct contextinfo
{
    /* NOTE: Due to `loc_get_id`, struct MUST begin with id */
    int id;                      /* id in table */
    char context[CONTEXTSZ + 1]; /* Context where the application is mapped. */
    int vhost;                   /* id of the correspond virtual host in hosts table */
    int node;                    /* id of the correspond node in nodes table */
    int status;                  /* status: ENABLED/DISABLED/STOPPED */
    int nbrequests;              /* number of request been processed */

    apr_time_t updatetime; /* time of last received message */
};
typedef struct contextinfo contextinfo_t;

/**
 * Insert(alloc) and update a context record in the shared table
 * @param s pointer to the shared table
 * @param context context to store in the shared table
 * @return APR_SUCCESS if all went well
 */
apr_status_t insert_update_context(mem_t *s, contextinfo_t *context);

/**
 * Read a context record from the shared table
 * @param s pointer to the shared table
 * @param context context to read from the shared table
 * @return address of the read context or NULL if error
 */
contextinfo_t *read_context(mem_t *s, contextinfo_t *context);

/**
 * Get a context record from the shared table
 * @param s pointer to the shared table
 * @param context address of the context read from the shared table
 * @param ids id of the context to return
 * @return APR_SUCCESS if all went well
 */
apr_status_t get_context(mem_t *s, contextinfo_t **context, int ids);

/**
 * Remove(free) a context record from the shared table
 * @param s pointer to the shared table
 * @param int the id of context to remove from the shared table
 * @return APR_SUCCESS if all went well
 */
apr_status_t remove_context(mem_t *s, int id);

/**
 * Get the ids for the used (not free) contexts in the table
 * @param s pointer to the shared table
 * @param ids array of int to store the used id (must be big enough)
 * @return number of the context existing or -1 if error
 */
int get_ids_used_context(mem_t *s, int *ids);

/**
 * Get the size of the table (max size)
 * @param s pointer to the shared table
 * @return size of the existing table or -1 if error
 */
int get_max_size_context(mem_t *s);

/**
 * Attach to the shared context table
 * @param string name of an existing shared table
 * @param num address to store the size of the shared table
 * @param p pool to use for allocations
 * @param storage slotmem logic provider
 * @return address of the struct used to access the table
 */
mem_t *get_mem_context(char *string, unsigned *num, apr_pool_t *p, slotmem_storage_method *storage);

/**
 * Create a shared context table
 * @param string name to use to create the table
 * @param num size of the shared table
 * @param persist tell if the slotmem element are persistent
 * @param p pool to use for allocations
 * @param storage slotmem logic provider
 * @return address of struct used to access the table
 */
mem_t *create_mem_context(char *string, unsigned *num, int persist, apr_pool_t *p, slotmem_storage_method *storage);

/**
 * Provider for the mod_proxy_cluster or mod_jk modules
 */
struct context_storage_method
{
    /**
     * The context corresponding to the ident
     * @param ids ident of the context to read
     * @param context address of pointer to return the context
     * @return APR_SUCCESS if all went well
     */
    apr_status_t (*read_context)(int ids, contextinfo_t **context);
    /**
     * Read the list of ident of used contexts
     * @param ids address to store the idents
     * @return APR_SUCCESS if all went well
     */
    int (*get_ids_used_context)(int *ids);
    /**
     * Read the max number of contexts in the shared table
     */
    int (*get_max_size_context)(void);
    /**
     * Lock the context table
     */
    apr_status_t (*lock_contexts)(void);
    /**
     * Unlock the context table
     */
    apr_status_t (*unlock_contexts)(void);
};
#endif /*CONTEXT_H*/
