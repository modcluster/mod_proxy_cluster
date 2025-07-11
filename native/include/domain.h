/*
 * Copyright The mod_cluster Project Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DOMAIN_H
#define DOMAIN_H

/**
 * @file  domain.h
 * @brief domain description Storage Module for Apache
 * @author Jean-Frederic Clere
 *
 * @defgroup MEM domain
 * @ingroup  APACHE_MODS
 * @{
 */

#define DOMAINEXE ".domain"

#ifndef MEM_T
typedef struct mem mem_t;
#define MEM_T
#endif

#include "mod_clustersize.h"

/**
 * Status of the domain as read/store in httpd
 */
struct domaininfo
{
    /* NOTE: Due to `loc_get_id`, struct MUST begin with id */
    int id;                    /* id in table */
    char domain[DOMAINNDSZ];   /* domain value */
    char JVMRoute[JVMROUTESZ]; /* corresponding node */
    char balancer[BALANCERSZ]; /* name of the balancer */

    apr_time_t updatetime; /* time of last received message */
};
typedef struct domaininfo domaininfo_t;

/**
 * Insert(alloc) and update a domain record in the shared table
 * @param s pointer to the shared table
 * @param domain domain to store in the shared table
 * @return APR_SUCCESS if all went well
 */
apr_status_t insert_update_domain(mem_t *s, domaininfo_t *domain);

/**
 * Read a domain record from the shared table
 * @param s pointer to the shared table
 * @param domain domain to read from the shared table
 * @return address of the read domain or NULL if error
 */
domaininfo_t *read_domain(mem_t *s, domaininfo_t *domain);

/**
 * Get a domain record from the shared table
 * @param s pointer to the shared table
 * @param domain address of the domain read from the shared table
 * @param ids id of the domain to return
 * @return APR_SUCCESS if all went well
 */
apr_status_t get_domain(mem_t *s, domaininfo_t **domain, int ids);

/**
 * Remove(free) a domain record from the shared table
 * @param s pointer to the shared table
 * @param domain domain to remove from the shared table
 * @return APR_SUCCESS if all went well
 */
apr_status_t remove_domain(mem_t *s, domaininfo_t *domain);

/**
 * Find a domain record from the shared table using JVMRoute and balancer
 * @param s pointer to the shared table
 * @param domain address where the node is located in the shared table
 * @param route JVMRoute to search
 * @param balancer balancer to search
 * @return APR_SUCCESS if all went well
 */
apr_status_t find_domain(mem_t *s, domaininfo_t **domain, const char *route, const char *balancer);

/**
 * Get the ids for the used (not free) domains in the table
 * @param s pointer to the shared table
 * @param ids array of int to store the used id (must be big enough)
 * @return number of domain existing or -1 if error
 */
int get_ids_used_domain(mem_t *s, int *ids);

/**
 * Get the size of the table (max size)
 * @param s pointer to the shared table
 * @return size of the existing table or -1 if error
 */
int get_max_size_domain(mem_t *s);

/**
 * Attach to the shared domain table
 * @param string name of an existing shared table
 * @param num address to store the size of the shared table
 * @param p pool to use for allocations
 * @param storage slotmem logic provider
 * @return address of struct used to access the table
 */
mem_t *get_mem_domain(char *string, unsigned *num, apr_pool_t *p, slotmem_storage_method *storage);

/**
 * Create a shared domain table
 * @param string name to use to create the table
 * @param num size of the shared table
 * @param persist tell if the slotmem element are persistent
 * @param p pool to use for allocations
 * @param storage slotmem logic provider
 * @return address of struct used to access the table
 */
mem_t *create_mem_domain(char *string, unsigned *num, int persist, apr_pool_t *p, slotmem_storage_method *storage);

/**
 * Provider for the mod_proxy_cluster or mod_jk modules
 */
struct domain_storage_method
{
    /**
     * The domain corresponding to the ident
     * @param ids ident of the domain to read
     * @param domain address of pointer to return the domain
     * @return APR_SUCCESS if all went well
     */
    apr_status_t (*read_domain)(int ids, domaininfo_t **domain);
    /**
     * Read the list of ident of used domains
     * @param ids address to store the idents
     * @return APR_SUCCESS if all went well
     */
    int (*get_ids_used_domain)(int *ids);
    /**
     * Read the max number of domains in the shared table
     * @return the max number of domains
     */
    int (*get_max_size_domain)(void);
    /**
     * Remove the domain from shared memory (free the slotmem)
     * @return APR_SUCCESS if all went well
     */
    apr_status_t (*remove_domain)(domaininfo_t *domain);
    /**
     * Insert a new domain or update existing one
     * @return APR_SUCCESS if all went well
     */
    apr_status_t (*insert_update_domain)(domaininfo_t *domain);
    /**
     * Find the domain using the JVMRoute and balancer information
     * @return APR_SUCCESS if all went well
     */
    apr_status_t (*find_domain)(domaininfo_t **node, const char *route, const char *balancer);
};
#endif /*DOMAIN_H*/
