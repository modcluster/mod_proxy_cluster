/*
 * Copyright The mod_cluster Project Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BALANCER_H
#define BALANCER_H

/**
 * @file  balancer.h
 * @brief balancer description Storage Module for Apache
 * @author Jean-Frederic Clere
 *
 * @defgroup MEM balancers
 * @ingroup  APACHE_MODS
 * @{
 */

#define BALANCEREXE ".balancers"

#ifndef MEM_T
typedef struct mem mem_t;
#define MEM_T
#endif

#include "mod_clustersize.h"

/**
 * Status of the balancer as read/store in httpd
 */
struct balancerinfo
{
    /* NOTE: Due to `loc_get_id`, struct MUST begin with id */
    int id;                    /* id in table */
    char balancer[BALANCERSZ]; /* Name of the balancer */
    int StickySession;         /* 0 : Don't use, 1: Use it */
    char StickySessionCookie[COOKNAMESZ];
    char StickySessionPath[PATHNAMESZ];
    int StickySessionRemove; /* 0 : Don't remove, 1: Remove it */
    int StickySessionForce;  /* 0: Don't force, 1: return error */
    int Timeout;
    int Maxattempts;

    apr_time_t updatetime; /* time of last received message */
};
typedef struct balancerinfo balancerinfo_t;

/**
 * Insert(alloc) and update a balancer record in the shared table
 * @param s pointer to the shared table
 * @param balancer balancer to store in the shared table
 * @return APR_SUCCESS if all went well
 *
 */
apr_status_t insert_update_balancer(mem_t *s, balancerinfo_t *balancer);

/**
 * Read a balancer record from the shared table
 * @param s pointer to the shared table
 * @param balancer balancer to read from the shared table
 * @return address of the read balancer or NULL if error
 */
balancerinfo_t *read_balancer(mem_t *s, balancerinfo_t *balancer);

/**
 * Get a balancer record from the shared table
 * @param s pointer to the shared table
 * @param balancer address of the balancer read from the shared table
 * @param ids id of the balancer to return
 * @return APR_SUCCESS if all went well
 */
apr_status_t get_balancer(mem_t *s, balancerinfo_t **balancer, int ids);

/**
 * Remove(free) a balancer record from the shared table
 * @param s pointer to the shared table
 * @param balancer balancer to remove from the shared table
 * @return APR_SUCCESS if all went well
 */
apr_status_t remove_balancer(mem_t *s, balancerinfo_t *balancer);

/**
 * Get the ids for the used (not free) balancers in the table
 * @param s pointer to the shared table
 * @param ids array of int to store the used id (must be big enough)
 * @return number of balancer existing or -1 if error
 */
int get_ids_used_balancer(mem_t *s, int *ids);

/**
 * Get the size of the table (max size)
 * @param s pointer to the shared table
 * @return size of the existing table or -1 if error
 */
int get_max_size_balancer(mem_t *s);

/**
 * Attach to the shared balancer table
 * @param string name of an existing shared table
 * @param num address to store the size of the shared table
 * @param p pool to use for allocations
 * @param storage storage provider
 * @return address of struct used to access the table
 */
mem_t *get_mem_balancer(char *string, unsigned *num, apr_pool_t *p, slotmem_storage_method *storage);

/**
 * Create a shared balancer table
 * @param string name to use to create the table
 * @param num size of the shared table
 * @param persist tell if the slotmem element are persistent
 * @param p pool to use for allocations
 * @param storage storage provider
 * @return address of struct used to access the table
 */
mem_t *create_mem_balancer(char *string, unsigned *num, int persist, apr_pool_t *p, slotmem_storage_method *storage);

/**
 * Provider for the mod_proxy_cluster or mod_jk modules
 */
struct balancer_storage_method
{
    /**
     * The balancer corresponding to the ident
     * @param ids ident of the balancer to read
     * @param balancer address of pointer to return the balancer
     * @return APR_SUCCESS if all went well
     */
    apr_status_t (*read_balancer)(int ids, balancerinfo_t **balancer);
    /**
     * Read the list of ident of used balancers
     * @param ids address to store the idents
     * @return APR_SUCCESS if all went well
     */
    int (*get_ids_used_balancer)(int *ids);
    /**
     * read the max number of balancers in the shared table
     */
    int (*get_max_size_balancer)(void);
};

/**
 * Helper function for translating hcheck template parameters to corresponding balancer parameters
 */
const char *translate_balancer_params(const char *param);

#endif /*BALANCER_H*/
