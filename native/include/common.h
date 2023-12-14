/*
 * Copyright The mod_cluster Project Authors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Common routines
 */

#include "mod_proxy.h"

#include "ap_slotmem.h"

#include "domain.h"
#include "node.h"
#include "host.h"
#include "node.h"
#include "context.h"
#include "mod_proxy_cluster.h"


struct counter
{
    unsigned count;
    int *values;
};


/**
 * Read the virtual host table from shared memory
 * @param pool pool to use for memory allocation
 * @param host_storage host_storage used for reading virtual hosts
 * @param for_cache flag whether to use cache (true) or not (false)
 * @return pointer to the read virtual host table
 */
proxy_vhost_table *read_vhost_table(apr_pool_t *pool, struct host_storage_method *host_storage, int for_cache);

/**
 * Update the virtual host table from shared memory to populate a cached table
 * @param vhost_table pointer to virtual host table that should be updated
 * @param host_storage host_storage used fo reading virtual hosts
 * @return pointer to the updated virtual host table
 */
proxy_vhost_table *update_vhost_table_cached(proxy_vhost_table *vhost_table,
                                             const struct host_storage_method *host_storage);

/**
 * Read the context table from shared memory
 * @param pool pool used for memory allocation
 * @param context_storage context_storage for context retrieval
 * @param for_cache flag whether to use cache (true) or not (false)
 * @return pointer to the read table
 */
proxy_context_table *read_context_table(apr_pool_t *pool, const struct context_storage_method *context_storage,
                                        int for_cache);

/**
 * Update the context table from shared memory to populate a cached table
 * @param context_table context table to update
 * @param context_storage context storage for context retrieval
 * @return pointer to the updated table
 */
proxy_context_table *update_context_table_cached(proxy_context_table *context_table,
                                                 const struct context_storage_method *context_storage);

/**
 * Read the balancer table from shared memory
 * @param pool pool used for for memory allocation
 * @param balancer_storage balancer_storage to for balancers retrieval
 * @param for_cache flag whether to use cache (true) or not (false)
 * @return pointer to the read table
 */
proxy_balancer_table *read_balancer_table(apr_pool_t *pool, const struct balancer_storage_method *balancer_storage,
                                          int for_cache);

/**
 * Update the balancer table from shared memory to populate a cached table
 * @param balancer_table pointer to the balancer table that should be updated
 * @param balancer_storage balancer_storage to for balancers retrieval
 * @return pointer to the updated table
 */
proxy_balancer_table *update_balancer_table_cached(proxy_balancer_table *balancer_table,
                                                   const struct balancer_storage_method *balancer_storage);

/**
 * Read the node table from shared memory
 * @param pool pool used for memory allocation
 * @param node_storage node_storage used for node retrieval
 * @param for_cache flag whether to use cache (true) or not (false)
 * @return pointer to the read table
 */
proxy_node_table *read_node_table(apr_pool_t *pool, const struct node_storage_method *node_storage, int for_cache);

/**
 * Update the node table from shared memory to populate a cached table
 * @param node_table pointer to the node table that should be updated
 * @param node_storage node_storage to for node retrieval
 * @return pointer to the updated table
 */
proxy_node_table *update_node_table_cached(proxy_node_table *node_table,
                                           const struct node_storage_method *node_storage);

/**
 * Read the cookie corresponding to name
 * @param r request.
 * @param name name of the cookie
 * @param in tells if cookie should read from the request or the response
 * @return the value of the cookie
 */
char *get_cookie_param(request_rec *r, const char *name, int in);

/**
 * Retrieve the parameter with the given name
 * Something like 'JSESSIONID=12345...N'
 *
 * @param pool pool to use for allocation of the returned string
 * @param url url to process
 * @param name parameter name which value is retrieved
 * @return string containing the parameter of a given name or NULL if the name is not present
 *
 * TODO: Should use the mod_proxy_balancer one
 */
char *get_path_param(apr_pool_t *pool, char *url, const char *name);

/**
 * Check that the request has a sessionid with a route
 * @param r the request_rec
 * @param stickyval the cookie or/and parameter name
 * @param uri part of the URL to for the session parameter
 * @param sticky_used the string that was used to find the route
 * @return route
 */
char *cluster_get_sessionid(request_rec *r, const char *stickyval, char *uri, char **sticky_used);

/**
 * Check that the request has a sessionid (even invalid)
 * @param r the request_rec.
 * @param nodeid the node id.
 * @param route (if received)
 * @param node_table node table
 * @return 1 is it finds a sessionid 0 otherwise.
 */
int hassession_byname(request_rec *r, int nodeid, const char *route, const proxy_node_table *node_table);

/**
 * Find the best nodes for a request (check host, context, and balancer)
 * @param r the request_rec
 * @param balancer the balancer (balancer to use in that case we check it)
 * @param route from the sessionid if we have one
 * @param use_alias compare alias with server_name
 * @param vhost_table virtual host table
 * @param context_table context table
 * @param node_table node table
 * @return a pointer to a list of nodes
 */
node_context *find_node_context_host(request_rec *r, const proxy_balancer *balancer, const char *route, int use_alias,
                                     const proxy_vhost_table *vhost_table, const proxy_context_table *context_table,
                                     const proxy_node_table *node_table);

/**
 * Find the balancer corresponding to the node information
 * @param r the request
 * @param conf server configuration
 * @param vhost_table virtual host table
 * @param context_table context table
 * @param balancer_table balancer table
 * @param node_table node table
 * @param use_alias whether to compare hostname with virtual hosts (true) or not (false)
 * @return balancer name corresponding to the given node or NULL if it does not exist
 */
const char *get_route_balancer(request_rec *r, const proxy_server_conf *conf, const proxy_vhost_table *vhost_table,
                               const proxy_context_table *context_table, const proxy_balancer_table *balancer_table,
                               const proxy_node_table *node_table, int use_alias);

/**
 * Read a node from the table using its it
 * @param node_table node table to retreive a searched node from
 * @param id an id of the searched node
 * @return node_info struct corresponding to the given @id or NULL if it doesn't exist
 */
const nodeinfo_t *table_get_node(const proxy_node_table *node_table, int id);

/**
 * Get a node from the table using the route
 * @param node_table node table to retreive a searched node from
 * @param route route corresponding to the searched node
 * @param id id that is set to the searched node's id if found
 * @return node_info struct corresponding to the given @route or NULL if it doesn't exist
 */
nodeinfo_t *table_get_node_route(proxy_node_table *node_table, char *route, int *id);

/**
 * Search the balancer that corresponds to the pair context/host
 * @param r the request_rec
 * @param vhost_table table of host virtual hosts
 * @param context_table table of contexts
 * @return the balancer name or NULL if not found
 */
const char *get_context_host_balancer(request_rec *r, proxy_vhost_table *vhost_table,
                                      proxy_context_table *context_table, proxy_node_table *node_table, int use_alias);


/**
 * Extracts an id from the given memory (expects structs scuh as nodemess and others)
 *
 * @param mem one of the structs such as nodemess and others (first member must be int id!)
 * @param data struct counter that gets updated (count++ and values pointer++)
 * @param pool unused argument
 * @return APR_SUCCESS
 */
apr_status_t loc_get_id(void *mem, void *data, apr_pool_t *pool);

/**
 * Check that the worker will handle the host/context.
 *
 * @param r the request_rec
 * @param balancer the balancer (balancer to use in that case we check it)
 * @param node node id
 * @param use_alias compare alias with server_name
 * @param vhost_table virtual host table
 * @param context_table context table
 * @param node_table node table
 * @return node_context in case a node with the given id handles the host/context
 *
 * The id of the worker is used to find the (slot) node in the shared memory
 * (See get_context_host_balancer too)
 */
const node_context *context_host_ok(request_rec *r, const proxy_balancer *balancer, int node, int use_alias,
                                    const proxy_vhost_table *vhost_table, const proxy_context_table *context_table,
                                    const proxy_node_table *node_table);
