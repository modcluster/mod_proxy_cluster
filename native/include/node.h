/*
 * Copyright The mod_cluster Project Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NODE_H
#define NODE_H

/**
 * @file  node.h
 * @brief node description Storage Module for Apache
 * @author Jean-Frederic Clere
 *
 * @defgroup MEM nodes
 * @ingroup  APACHE_MODS
 * @{
 */

#define NODEEXE ".nodes"

#ifndef MEM_T
typedef struct mem mem_t;
#define MEM_T
#endif

#include "mod_clustersize.h"

#include "ap_mmn.h"

/**
 * Configuration of the node received from jboss cluster
 */
struct nodemess
{
    /* NOTE: Due to `loc_get_id`, struct MUST begin with id */
    int id; /* id in table and worker id */

    /* balancer info */
    char balancer[BALANCERSZ]; /* name of the balancer */
    char JVMRoute[JVMROUTESZ];
    char Domain[DOMAINNDSZ];
    char Host[HOSTNODESZ];
    char Port[PORTNODESZ];
    char Type[SCHEMENDSZ];
    char Upgrade[SCHEMENDSZ];
    char AJPSecret[AJPSECRETSZ];
    int reversed; /* 1 : reversed... 0 : normal */
    int remove;   /* 1 : removed     0 : normal */
    long ResponseFieldSize;

    /* node conf part */
    int flushpackets;
    int flushwait;
    apr_time_t ping;
    int smax;
    apr_time_t ttl;
    apr_time_t timeout;

    /* part updated in httpd */
    apr_time_t updatetimelb; /* time of last update of the lbstatus value */
    int num_failure_idle;    /* number of time the cping/cpong failed while calculating the lbstatus value */
    apr_size_t oldelected;   /* value of s->elected when calculating the lbstatus */
    apr_off_t oldread;       /* Number of bytes read from remote when calculating the lbstatus */
    apr_time_t lastcleantry; /* time of last unsuccessful try to clean the worker in proxy part */
    int num_remove_check;    /* number of tries to remove a REMOVED node */
};
typedef struct nodemess nodemess_t;

#define SIZEOFSCORE 1700 /* at least size of the proxy_worker_stat structure */

/**
 * Status of the node as read/store in httpd
 */
struct nodeinfo
{
    /* config from jboss/tomcat */
    nodemess_t mess;
    /* filled by httpd */
    apr_time_t updatetime;  /* time of last received message */
    unsigned long offset;   /* offset to the proxy_worker_stat structure */
    char stat[SIZEOFSCORE]; /* to store the status */
};
typedef struct nodeinfo nodeinfo_t;

/**
 * Use apache httpd structure
 */
typedef struct ap_slotmem_provider_t slotmem_storage_method;

/**
 * Return the last stored in the mem structure
 * @param pointer to the shared table
 * @return APR_SUCCESS if all went well
 *
 */
apr_status_t get_last_mem_error(mem_t *mem);

/**
 * Insert(alloc) and update a node record in the shared table
 * @param s pointer to the shared table
 * @param node node to store in the shared table
 * @param id pointer to store the id where the node is inserted
 * @param clean tells to clean or not the worker_shared part
 * @return APR_SUCCESS if all went well
 *
 */
apr_status_t insert_update_node(mem_t *s, nodeinfo_t *node, int *id, int clean);

/**
 * Read a node record from the shared table
 * @param s pointer to the shared table
 * @param node node to read from the shared table
 * @return address of the read node or NULL if error
 */
nodeinfo_t *read_node(mem_t *s, nodeinfo_t *node);

/**
 * Get a node record from the shared table
 * @param s pointer to the shared table
 * @param node address of the node read from the shared table
 * @param ids id of the node to return
 * @return APR_SUCCESS if all went well
 */
apr_status_t get_node(mem_t *s, nodeinfo_t **node, int ids);

/**
 * Remove(free) a node record from the shared table
 * @param pointer to the shared table
 * @param ids id of node to remove from the shared table
 * @return APR_SUCCESS if all went well
 */
apr_status_t remove_node(mem_t *s, int ids);

/**
 * Find a node record from the shared table using JVMRoute
 * @param s pointer to the shared table
 * @param node address where the node is located in the shared table
 * @param route JVMRoute to search
 * @return APR_SUCCESS if all went well
 */
apr_status_t find_node(mem_t *s, nodeinfo_t **node, const char *route);

/**
 * Find a node record from the shared table using Host/Port
 * @param s pointer to the shared table
 * @param node address where the node is located in the shared table
 * @param host host to search
 * @param port port to search
 * @return APR_SUCCESS if all went well
 */
apr_status_t find_node_byhostport(mem_t *s, nodeinfo_t **node, const char *host, const char *port);

/**
 * Lock the nodes table
 */
apr_status_t lock_nodes(void);

/**
 * Unlock the nodes table
 */
apr_status_t unlock_nodes(void);

/**
 * Get the ids for the used (not free) nodes in the table
 * @param s pointer to the shared table
 * @param ids array of int to store the used id (must be big enough)
 * @return number of node existing or -1 if error
 */
int get_ids_used_node(mem_t *s, int *ids);

/**
 * Get the size of the table (max size)
 * @param s pointer to the shared table
 * @return size of the existing table or -1 if error
 */
int get_max_size_node(mem_t *s);

/**
 * Attach to the shared node table
 * @param string name of an existing shared table
 * @param num address to store the size of the shared table
 * @param p pool to use for allocations
 * @param storage storage provider
 * @return address of struct used to access the table
 */
mem_t *get_mem_node(char *string, unsigned *num, apr_pool_t *p, slotmem_storage_method *storage);

/**
 * Create a shared node table
 * @param name to use to create the table
 * @param size of the shared table
 * @param persist tell if the slotmem element are persistent
 * @param p pool to use for allocations
 * @param storage storage provider
 * @return address of struct used to access the table
 */
mem_t *create_mem_node(char *string, unsigned *num, int persist, apr_pool_t *p, slotmem_storage_method *storage);

/**
 * Provider for the mod_proxy_cluster or mod_jk modules
 */
struct node_storage_method
{
    /**
     * The node corresponding to the ident
     * @param ids ident of the node to read
     * @param node address of pointer to return the node
     * @return APR_SUCCESS if all went well
     */
    apr_status_t (*read_node)(int ids, nodeinfo_t **node);
    /**
     * Read the list of ident of used nodes
     * @param ids address to store the idents
     * @return APR_SUCCESS if all went well
     */
    int (*get_ids_used_node)(int *ids);
    /**
     * Read the max number of nodes in the shared table
     * @return max number of nodes in the shared table
     */
    int (*get_max_size_node)(void);
    /**
     * Check the nodes for modifications.
     * XXX: void *data is server_rec *s in fact
     */
    unsigned (*worker_nodes_need_update)(void *data, apr_pool_t *pool);
    /**
     * Mark that the worker node are now up to date
     */
    int (*worker_nodes_are_updated)(void *data, unsigned version);
    /**
     * Remove the node from shared memory (free the slotmem)
     */
    int (*remove_node)(int node);
    /**
     * Find the node using the JVMRoute information
     */
    apr_status_t (*find_node)(nodeinfo_t **node, const char *route);
    /**
     * Remove the virtual hosts and contexts corresponding the node
     */
    void (*remove_host_context)(int node, apr_pool_t *pool);

    /**
     * Lock the nodes table
     */
    apr_status_t (*lock_nodes)(void);

    /**
     * Unlock the nodes table
     */
    apr_status_t (*unlock_nodes)(void);
};
#endif /*NODE_H*/
