/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mod_proxy.h"
#include "scoreboard.h"
#include "ap_mpm.h"
#include "apr_version.h"
#include "ap_hooks.h"

#include "slotmem.h"

#include "node.h"
#include "host.h"
#include "context.h"
#include "balancer.h"

#include "mod_proxy_cluster.h"

static struct node_storage_method *node_storage = NULL;
static struct host_storage_method *host_storage = NULL;
static struct context_storage_method *context_storage = NULL;
static struct balancer_storage_method *balancer_storage = NULL;
static struct domain_storage_method *domain_storage = NULL;

static int use_alias = 0; /* 1 : Compare Alias with server_name */

module AP_MODULE_DECLARE_DATA lbmethod_cluster_module;

static proxy_worker *find_best(proxy_balancer *balancer,
                                  request_rec *r)
{
    proxy_worker **worker = NULL;
    proxy_worker *mycandidate = NULL;
    int i;
    for (i = 0; i < balancer->workers->nelts; i++) {
        worker = &APR_ARRAY_IDX(balancer->workers, i, proxy_worker *);
    }
    if (worker != NULL)
        mycandidate = *worker;
    return mycandidate;
}

static apr_status_t reset(proxy_balancer *balancer, server_rec *s)
{
    return APR_SUCCESS;
}

static apr_status_t age(proxy_balancer *balancer, server_rec *s)
{
    return APR_SUCCESS;
}

static const proxy_balancer_method cluster = 
{
    "cluster",
    &find_best,
    NULL,
    &reset,
    &age,
    NULL
};

/*
 * See if we could map the request.
 * first check is we have a balancer corresponding to the route.
 * then search the balancer correspond to the context and host.
 */
static int lbmethod_cluster_trans(request_rec *r)
{
    const char *balancer;
    void *sconf = r->server->module_config;
    proxy_server_conf *conf = (proxy_server_conf *)
        ap_get_module_config(sconf, &proxy_module);


#if HAVE_CLUSTER_EX_DEBUG
    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r->server,
                "lbmethod_cluster_trans for %d %s %s uri: %s args: %s unparsed_uri: %s",
                 r->proxyreq, r->filename, r->handler, r->uri, r->args, r->unparsed_uri);
    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r->server,
                "lbmethod_cluster_trans for %d", conf->balancers->nelts);
#endif

    proxy_vhost_table *vhost_table = read_vhost_table(r, host_storage);
    proxy_context_table *context_table = read_context_table(r, context_storage);
    proxy_balancer_table *balancer_table = read_balancer_table(r, balancer_storage);
    proxy_node_table *node_table = read_node_table(r, node_storage);

    apr_table_setn(r->notes, "vhost-table",  (char *) vhost_table);
    apr_table_setn(r->notes, "context-table",  (char *) context_table);
    apr_table_setn(r->notes, "balancer-table",  (char *) balancer_table);
    apr_table_setn(r->notes, "node-table",  (char *) node_table);

    balancer = get_route_balancer(r, conf, vhost_table, context_table, balancer_table, node_table, use_alias);
    if (!balancer) {
        balancer = get_context_host_balancer(r, vhost_table, context_table, node_table, use_alias);
    }
    

    if (balancer) {

        /* It is safer to use r->uri */
        if (strncmp(r->uri, "balancer://",11))
            r->filename =  apr_pstrcat(r->pool, "proxy:balancer://", balancer, r->uri, NULL);
        else
            r->filename =  apr_pstrcat(r->pool, "proxy:", r->uri, NULL);
        r->handler = "proxy-server";
        r->proxyreq = PROXYREQ_REVERSE;
#if HAVE_CLUSTER_EX_DEBUG
        ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r->server,
                    "proxy_cluster_trans using %s uri: %s",
                     balancer, r->filename);
#endif
        return OK; /* Mod_proxy will process it */
    }
 
#if HAVE_CLUSTER_EX_DEBUG
    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r->server,
                "proxy_cluster_trans DECLINED %s uri: %s unparsed_uri: %s",
                 balancer, r->filename, r->unparsed_uri);
#endif
    return DECLINED;
}

static int lbmethod_cluster_post_config(apr_pool_t *p, apr_pool_t *plog,
                                     apr_pool_t *ptemp, server_rec *s)
{
   node_storage = ap_lookup_provider("manager" , "shared", "0");
    if (node_storage == NULL) {
        ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, 0, s,
                    "proxy_cluster_post_config: Can't find mod_manager for nodes");
        return !OK;
    }
    host_storage = ap_lookup_provider("manager" , "shared", "1");
    if (host_storage == NULL) {
        ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, 0, s,
                    "proxy_cluster_post_config: Can't find mod_manager for hosts");
        return !OK;
    }
    context_storage = ap_lookup_provider("manager" , "shared", "2");
    if (context_storage == NULL) {
        ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, 0, s,
                    "proxy_cluster_post_config: Can't find mod_manager for contexts");
        return !OK;
    }
    balancer_storage = ap_lookup_provider("manager" , "shared", "3");
    if (balancer_storage == NULL) {
        ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, 0, s,
                    "proxy_cluster_post_config: Can't find mod_manager for balancers");
        return !OK;
    }
    domain_storage = ap_lookup_provider("manager" , "shared", "5");
    if (domain_storage == NULL) {
        ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, 0, s,
                    "proxy_cluster_post_config: Can't find mod_manager for domains");
        return !OK;
    }

    /* Add version information */
    ap_add_version_component(p, MOD_CLUSTER_EXPOSED_VERSION);
    return OK;
}

static void register_hooks(apr_pool_t *p)
{
    static const char * const aszPre[]={ "mod_manager.c", "mod_rewrite.c", NULL };
    static const char * const aszSucc[]={ "mod_proxy.c", NULL };

    ap_register_provider(p, PROXY_LBMETHOD, "cluster", "0", &cluster);

    ap_hook_translate_name(lbmethod_cluster_trans, aszPre, aszSucc, APR_HOOK_FIRST);
    ap_hook_post_config(lbmethod_cluster_post_config, NULL, NULL, APR_HOOK_MIDDLE);
}


AP_DECLARE_MODULE(lbmethod_cluster) = {
    STANDARD20_MODULE_STUFF,
    NULL,                       /* create per-directory config structure */
    NULL,                       /* merge per-directory config structures */
    NULL,        /* create per-server config structure */
    NULL,         /* merge per-server config structures */
    NULL,                       /* command apr_table_t */
    register_hooks              /* register hooks */
};
