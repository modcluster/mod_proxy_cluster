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

#define MOD_CLUSTER_EXPOSED_VERSION "mod_cluster/2.0.0.Alpha1-SNAPSHOT"

struct balancer_method {
/**
 * Check that the node is responding
 * @param r request_rec structure.
 * @param id ident of the worker.
 * @param load load factor to set if test is ok.
 * @return 0: All OK 500 : Error
 */ 
int (* proxy_node_isup)(request_rec *r, int id, int load);
/**
 * Check that the node is responding
 * @param r request_rec structure.
 * @param scheme something like ajp, http or https.
 * @param host the hostname.
 * @param port the port on which the node connector is running
 * @return 0: All OK 500 : Error
 */ 
int (* proxy_host_isup)(request_rec *r, char *scheme, char *host, char *port);
};
typedef struct balancer_method balancer_method;

/* Context table copy for local use */
struct proxy_context_table
{
	int sizecontext;
	int* contexts;
	contextinfo_t* context_info;
};
typedef struct proxy_context_table proxy_context_table;


/* VHost table copy for local use */
struct proxy_vhost_table
{
	int sizevhost;
	int* vhosts;
	hostinfo_t* vhost_info;
};
typedef struct proxy_vhost_table proxy_vhost_table;

/* Balancer table copy for local use */
struct proxy_balancer_table
{
	int sizebalancer;
	int* balancers;
	balancerinfo_t* balancer_info;
};
typedef struct proxy_balancer_table proxy_balancer_table;

/* Node table copy for local use */
struct proxy_node_table
{
	int sizenode;
	int* nodes;
	nodeinfo_t*  node_info;
};
typedef struct proxy_node_table proxy_node_table;

/* table of node and context selected by find_node_context_host() */
struct node_context
{
        int node;
        int context;
};
typedef struct node_context node_context;

/* common routines */
proxy_vhost_table *read_vhost_table(apr_pool_t *pool, struct host_storage_method *host_storage);
proxy_context_table *read_context_table(apr_pool_t *pool, struct context_storage_method *context_storage);
proxy_balancer_table *read_balancer_table(apr_pool_t *pool, struct balancer_storage_method *balancer_storage);
proxy_node_table *read_node_table(apr_pool_t *pool, struct node_storage_method *node_storage);

const char *get_route_balancer(request_rec *r, proxy_server_conf *conf,
                                      proxy_vhost_table *vhost_table,
                                      proxy_context_table *context_table,
                                      proxy_balancer_table *balancer_table,
                                      proxy_node_table *node_table,
                                      int use_alias);

const char *get_context_host_balancer(request_rec *r,
                proxy_vhost_table *vhost_table,
                proxy_context_table *context_table,
                proxy_node_table *node_table,
                int use_alias);

nodeinfo_t* table_get_node(proxy_node_table *node_table, int id);
char *cluster_get_sessionid(request_rec *r, const char *stickyval, char *uri, char **sticky_used);
char *get_cookie_param(request_rec *r, const char *name, int in);
char *get_path_param(apr_pool_t *pool, char *url,
                            const char *name);
node_context *find_node_context_host(request_rec *r, proxy_balancer *balancer, const char *route, int use_alias,
                                     proxy_vhost_table* vhost_table, proxy_context_table* context_table, proxy_node_table *node_table);
int hassession_byname(request_rec *r, int nodeid, const char *route, proxy_node_table *node_table);

nodeinfo_t* table_get_node_route(proxy_node_table *node_table, char *route, int *id);

node_context *context_host_ok(request_rec *r, proxy_balancer *balancer, int node, int use_alias,
                             proxy_vhost_table *vhost_table, proxy_context_table *context_table, proxy_node_table *node_table);


#endif /*MOD_PROXY_CLUSTER_H*/
