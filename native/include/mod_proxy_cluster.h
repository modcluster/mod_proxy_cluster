/*
 *  mod_cluster
 *
 *  Copyright(c) 2008 Red Hat Middleware, LLC,
 *  and individual contributors as indicated by the @authors tag.
 *  See the copyright.txt in the distribution for a
 *  full listing of individual contributors.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library in the file COPYING.LIB;
 *  if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 *
 * @author Jean-Frederic Clere
 * @version $Revision$
 */

#ifndef MOD_PROXY_CLUSTER_H
#define MOD_PROXY_CLUSTER_H

#include "balancer.h"

#define MOD_CLUSTER_EXPOSED_VERSION "mod_cluster/2.0.0.Alpha1-SNAPSHOT"

/* We don't care about versions older then 2.4.x, i.e., MODULE_MAGIC_NUMBER_MAJOR < 20120211 */
#if MODULE_MAGIC_NUMBER_MAJOR == 20120211 && MODULE_MAGIC_NUMBER_MINOR < 124
#error Please update your HTTPD, mod_proxy_cluster requires version 2.4.53 or newer.
#endif

struct balancer_method
{
    /**
     * Check that the node is responding
     * @param r request_rec structure.
     * @param id ident of the worker.
     * @param load load factor to set if test is ok.
     * @return 0: All OK 500 : Error
     */
    int (*proxy_node_isup)(request_rec *r, int id, int load);
    /**
     * Check that the node is responding
     * @param r request_rec structure.
     * @param scheme something like ajp, http or https.
     * @param host the hostname.
     * @param port the port on which the node connector is running
     * @return 0: All OK 500 : Error
     */
    int (*proxy_host_isup)(request_rec *r, const char *scheme, const char *host, const char *port);
    /**
     * Check if a worker already exists and return the corresponding id
     * @param r request_rec structure.
     * @param balancername, the balancer name.
     * @param scheme something like ajp, http or https.
     * @param host the hostname.
     * @param port the port on which the node connector is running
     * @param id the address to store the index that was previously used.
     * @param the_conf adress to store the proxy_server_conf the worker is using.
     * @return the worker or NULL if not existing.
     */
    proxy_worker *(*proxy_node_getid)(request_rec *r, const char *balancername, const char *scheme, const char *host,
                                      const char *port, int *id, const proxy_server_conf **the_conf);

    /**
     * Re enable the proxy_worker
     * @param r request_rec structure.
     * @param node pointer to node structure we have created.
     * @param worker the proxy_worker to re enable
     * @param nodeinfo pointer to node structure we are creating
     * @param the_conf the proxy_server_conf from proxy_node_getid()
     */
    void (*reenable_proxy_worker)(server_rec *s, nodeinfo_t *node, proxy_worker *worker, nodeinfo_t *nodeinfo,
                                  const proxy_server_conf *the_conf);

    /**
     * return the first free id to insert in node table
     */
    int (*proxy_node_get_free_id)(request_rec *r, int node_table_size);
};

typedef struct balancer_method balancer_method;

/* Context table copy for local use */
struct proxy_context_table
{
    int sizecontext;
    int *contexts;
    contextinfo_t *context_info;
};
typedef struct proxy_context_table proxy_context_table;


/* VHost table copy for local use */
struct proxy_vhost_table
{
    int sizevhost;
    int *vhosts;
    hostinfo_t *vhost_info;
};
typedef struct proxy_vhost_table proxy_vhost_table;

/* Balancer table copy for local use */
struct proxy_balancer_table
{
    int sizebalancer;
    int *balancers;
    balancerinfo_t *balancer_info;
};
typedef struct proxy_balancer_table proxy_balancer_table;

/* Node table copy for local use, the ptr_node is the shared memory address (slotmem address) */
struct proxy_node_table
{
    int sizenode;
    int *nodes;
    nodeinfo_t *node_info;
    char **ptr_node;
};
typedef struct proxy_node_table proxy_node_table;

/* table of node and context selected by find_node_context_host() */
struct node_context
{
    int node;
    int context;
};
typedef struct node_context node_context;

#endif /*MOD_PROXY_CLUSTER_H*/
