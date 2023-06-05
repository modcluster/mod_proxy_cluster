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
#include "mod_watchdog.h"
#include "scoreboard.h"
#include "ap_mpm.h"
#include "apr_version.h"
#include "ap_hooks.h"

#include "ap_slotmem.h"

#include "node.h"
#include "host.h"
#include "context.h"
#include "balancer.h"

#include "mod_proxy_cluster.h"

#define LB_CLUSTER_WATHCHDOG_NAME ("_lb_cluster_")
static ap_watchdog_t *watchdog;

static struct node_storage_method *node_storage = NULL;
static struct host_storage_method *host_storage = NULL;
static struct context_storage_method *context_storage = NULL;
static struct balancer_storage_method *balancer_storage = NULL;
static struct domain_storage_method *domain_storage = NULL;

static int use_alias = 0; /* 1 : Compare Alias with server_name */
static apr_time_t lbstatus_recalc_time =
    apr_time_from_sec(5); /* recalcul the lbstatus based on number of request in the time interval */
static apr_time_t wait_for_remove = apr_time_from_sec(10); /* wait until that before removing a removed node */

module AP_MODULE_DECLARE_DATA lbmethod_cluster_module;

static proxy_worker *internal_find_best_byrequests(request_rec *r, proxy_balancer *balancer,
                                                   proxy_vhost_table *vhost_table, proxy_context_table *context_table,
                                                   proxy_node_table *node_table)
{
    char *ptr = balancer->workers->elts;
    int sizew = balancer->workers->elt_size;
    proxy_worker *mycandidate = NULL;
    int i;

    for (i = 0; i < balancer->workers->nelts; i++, ptr = ptr + sizew) {
        nodeinfo_t *node;
        int id;
        proxy_worker **run = (proxy_worker **)ptr;
        proxy_worker *worker = *run;

        if (!PROXY_WORKER_IS_USABLE(worker)) {
            continue;
        }
        /* read the node and check context */
        node = table_get_node_route(node_table, worker->s->route, &id);
        if (!node) {
            continue;
        }
        if (!context_host_ok(r, balancer, id, use_alias, vhost_table, context_table, node_table)) {
            continue;
        }
        if (!mycandidate) {
            mycandidate = worker;
        }
        else {
            nodeinfo_t *node1;
            int id1;
            node1 = table_get_node_route(node_table, mycandidate->s->route, &id1);
            if (node1) {
                int lbstatus, lbstatus1;
                lbstatus1 = ((mycandidate->s->elected - node1->mess.oldelected) * 1000) / mycandidate->s->lbfactor;
                lbstatus = ((worker->s->elected - node->mess.oldelected) * 1000) / worker->s->lbfactor;
                if (lbstatus1 > lbstatus) {
                    mycandidate = worker;
                }
            }
        }
    }
    if (mycandidate) {
#if MODULE_MAGIC_NUMBER_MAJOR == 20120211 && MODULE_MAGIC_NUMBER_MINOR >= 124
        if (proxy_run_check_trans(r, mycandidate->s->name_ex) != OK) {
#else
        if (proxy_run_check_trans(r, mycandidate->s->name) != OK) {
#endif
            char *ptr = balancer->workers->elts;
            int sizew = balancer->workers->elt_size;
            for (i = 0; i < balancer->workers->nelts; i++, ptr = ptr + sizew) {
                proxy_worker **run = (proxy_worker **)ptr;
                proxy_worker *httpworker = *run;
                if (!strcmp(httpworker->s->hostname, mycandidate->s->hostname)) {
                    /* They don't the shared memory another test is needed... */
                    if (!memcmp(httpworker->s->scheme, "http", 4) && httpworker->s->port == mycandidate->s->port &&
                        !strcmp(httpworker->s->route, mycandidate->s->route)) {
                        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
#if MODULE_MAGIC_NUMBER_MAJOR == 20120211 && MODULE_MAGIC_NUMBER_MINOR >= 124
                                     "proxy: byrequests balancer Using %s instead %s", httpworker->s->name_ex,
                                     mycandidate->s->name_ex);
#else
                                     "proxy: byrequests balancer Using %s instead %s", httpworker->s->name,
                                     mycandidate->s->name);
#endif
                        return httpworker;
                    }
                }
            }
        }
    }
    return mycandidate;
}

static proxy_worker *find_best(proxy_balancer *balancer, request_rec *r)
{
    proxy_worker *mycandidate = NULL;

    proxy_vhost_table *vhost_table = (proxy_vhost_table *)apr_table_get(r->notes, "vhost-table");
    proxy_context_table *context_table = (proxy_context_table *)apr_table_get(r->notes, "context-table");
    proxy_node_table *node_table = (proxy_node_table *)apr_table_get(r->notes, "node-table");

    if (!vhost_table) {
        vhost_table = read_vhost_table(r->pool, host_storage, 0);
    }

    if (!context_table) {
        context_table = read_context_table(r->pool, context_storage, 0);
    }

    if (!node_table) {
        node_table = read_node_table(r->pool, node_storage, 0);
    }

    mycandidate = internal_find_best_byrequests(r, balancer, vhost_table, context_table, node_table);

    return mycandidate;
}

static apr_status_t reset(proxy_balancer *balancer, server_rec *s)
{
    (void)balancer;
    (void)s;
    return APR_SUCCESS;
}

static apr_status_t age(proxy_balancer *balancer, server_rec *s)
{
    (void)balancer;
    (void)s;
    return APR_SUCCESS;
}

static apr_status_t updatelbstatus(proxy_balancer *balancer, proxy_worker *elected, server_rec *s)
{
    (void)balancer;
    (void)elected;
    (void)s;
    return APR_SUCCESS;
}

static const proxy_balancer_method cluster = {"cluster", &find_best, NULL, &reset, &age, &updatelbstatus};

/*
 * See if we could map the request.
 * first check is we have a balancer corresponding to the route.
 * then search the balancer correspond to the context and host.
 */
static int lbmethod_cluster_trans(request_rec *r)
{
    const char *balancer;
    void *sconf = r->server->module_config;
    proxy_server_conf *conf = (proxy_server_conf *)ap_get_module_config(sconf, &proxy_module);


#if HAVE_CLUSTER_EX_DEBUG
    ap_log_error(APLOG_MARK, APLOG_NOERRNO | APLOG_DEBUG, 0, r->server,
                 "lbmethod_cluster_trans for %d %s %s uri: %s args: %s unparsed_uri: %s", r->proxyreq, r->filename,
                 r->handler, r->uri, r->args, r->unparsed_uri);
    ap_log_error(APLOG_MARK, APLOG_NOERRNO | APLOG_DEBUG, 0, r->server, "lbmethod_cluster_trans for %d",
                 conf->balancers->nelts);
#endif

    proxy_vhost_table *vhost_table = read_vhost_table(r->pool, host_storage, 0);
    proxy_context_table *context_table = read_context_table(r->pool, context_storage, 0);
    proxy_balancer_table *balancer_table = read_balancer_table(r->pool, balancer_storage, 0);
    proxy_node_table *node_table = read_node_table(r->pool, node_storage, 0);

    apr_table_setn(r->notes, "vhost-table", (char *)vhost_table);
    apr_table_setn(r->notes, "context-table", (char *)context_table);
    apr_table_setn(r->notes, "balancer-table", (char *)balancer_table);
    apr_table_setn(r->notes, "node-table", (char *)node_table);

    balancer = get_route_balancer(r, conf, vhost_table, context_table, balancer_table, node_table, use_alias);
    if (!balancer) {
        balancer = get_context_host_balancer(r, vhost_table, context_table, node_table, use_alias);
    }


    if (balancer) {

        /* It is safer to use r->uri */
        if (strncmp(r->uri, "balancer://", 11)) {
            r->filename = apr_pstrcat(r->pool, "proxy:balancer://", balancer, r->uri, NULL);
        }
        else {
            r->filename = apr_pstrcat(r->pool, "proxy:", r->uri, NULL);
        }
        r->handler = "proxy-server";
        r->proxyreq = PROXYREQ_REVERSE;
#if HAVE_CLUSTER_EX_DEBUG
        ap_log_error(APLOG_MARK, APLOG_NOERRNO | APLOG_DEBUG, 0, r->server, "proxy_cluster_trans using %s uri: %s",
                     balancer, r->filename);
#endif
        return OK; /* Mod_proxy will process it */
    }

#if HAVE_CLUSTER_EX_DEBUG
    ap_log_error(APLOG_MARK, APLOG_NOERRNO | APLOG_DEBUG, 0, r->server,
                 "proxy_cluster_trans DECLINED %s uri: %s unparsed_uri: %s", balancer, r->filename, r->unparsed_uri);
#endif
    return DECLINED;
}

/*
 * Remove node that have beeen marked removed for more than 10 seconds.
 */
static void remove_removed_node(server_rec *s, apr_pool_t *pool, apr_time_t now, proxy_node_table *node_table)
{
    int i;
    (void)s;

    for (i = 0; i < node_table->sizenode; i++) {
        nodeinfo_t *ou;
        if (node_storage->read_node(node_table->nodes[i], &ou) != APR_SUCCESS) {
            continue;
        }
        if (ou->mess.remove && (now - ou->updatetime) >= wait_for_remove &&
            (now - ou->mess.lastcleantry) >= wait_for_remove) {
            /* remove the node from the shared memory */
            node_storage->remove_host_context(ou->mess.id, pool);
            node_storage->remove_node(ou->mess.id);
        }
    }
}

static apr_status_t mc_watchdog_callback(int state, void *data, apr_pool_t *pool)
{
    apr_status_t rv = APR_SUCCESS;
    server_rec *s = (server_rec *)data;
    proxy_node_table *node_table;
    apr_time_t now;
    switch (state) {
    case AP_WATCHDOG_STATE_STARTING:
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, "lbmethod_cluster_watchdog_callback STARTING");
        break;

    case AP_WATCHDOG_STATE_RUNNING:
        /* loop thru all workers */
        node_table = read_node_table(pool, node_storage, 0);
        now = apr_time_now();
        if (s) {
            int i;
            void *sconf = s->module_config;
            proxy_server_conf *conf = (proxy_server_conf *)ap_get_module_config(sconf, &proxy_module);
            proxy_balancer *balancer = (proxy_balancer *)conf->balancers->elts;

            for (i = 0; i < conf->balancers->nelts; i++, balancer++) {
                int n;
                proxy_worker **workers;
                proxy_worker *worker;
                /* Have any new balancers or workers been added dynamically? */
                ap_proxy_sync_balancer(balancer, s, conf);
                workers = (proxy_worker **)balancer->workers->elts;
                for (n = 0; n < balancer->workers->nelts; n++) {
                    nodeinfo_t *node;
                    int id;
                    worker = *workers;
                    node = table_get_node_route(node_table, worker->s->route, &id);
                    if (node != NULL) {
                        if (node->mess.remove) {
                            /* Already marked for removal */
                            workers++;
                            continue;
                        }
                        if (node->mess.updatetimelb < (now - lbstatus_recalc_time)) {
                            /* The lbstatus needs to be updated */
                            nodeinfo_t *ou;
                            int elected, oldelected;
                            elected = worker->s->elected;
                            oldelected = node->mess.oldelected;
                            node_storage->lock_nodes();
                            if (node_storage->read_node(id, &ou) != APR_SUCCESS) {
                                node_storage->unlock_nodes();
                                workers++;
                                continue;
                            }
                            if (ou->mess.remove) {
                                /* the stored node is already marked for removal */
                                node_storage->unlock_nodes();
                                workers++;
                                continue;
                            }
                            ou->mess.updatetimelb = now;
                            node->mess.updatetimelb = now;
                            node->mess.oldelected = elected;
                            ou->mess.oldelected = elected;
                            if (worker->s->lbfactor > 0) {
                                worker->s->lbstatus = ((elected - oldelected) * 1000) / worker->s->lbfactor;
                            }
                            if (elected == oldelected) {
                                /* lbstatus_recalc_time without changes: test for broken nodes */
                                if (PROXY_WORKER_IS(worker, PROXY_WORKER_HC_FAIL)) {
                                    ou->mess.num_failure_idle++;
                                    if (ou->mess.num_failure_idle > 60) {
                                        /* Failing for 5 minutes: time to mark it removed */
                                        ou->mess.remove = 1;
                                        ou->updatetime = now;
                                    }
                                }
                                else {
                                    ou->mess.num_failure_idle = 0;
                                }
                            }
                            else {
                                ou->mess.num_failure_idle = 0;
                            }
                            node_storage->unlock_nodes();
                        }
                    }
                    workers++;
                }
            }

            /* cleanup removed node in shared memory */
            remove_removed_node(s, pool, now, node_table);
        }
        break;

    case AP_WATCHDOG_STATE_STOPPING:
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, "lbmethod_cluster_watchdog_callback STOPPING");
        break;
    }
    return rv;
}

static int lbmethod_cluster_post_config(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s)
{
    APR_OPTIONAL_FN_TYPE(ap_watchdog_get_instance) *mc_watchdog_get_instance;
    APR_OPTIONAL_FN_TYPE(ap_watchdog_register_callback) *mc_watchdog_register_callback;
    (void)plog;
    (void)ptemp;

    node_storage = ap_lookup_provider("manager", "shared", "0");
    if (node_storage == NULL) {
        ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, 0, s,
                     "proxy_cluster_post_config: Can't find mod_manager for nodes");
        return !OK;
    }
    host_storage = ap_lookup_provider("manager", "shared", "1");
    if (host_storage == NULL) {
        ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, 0, s,
                     "proxy_cluster_post_config: Can't find mod_manager for hosts");
        return !OK;
    }
    context_storage = ap_lookup_provider("manager", "shared", "2");
    if (context_storage == NULL) {
        ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, 0, s,
                     "proxy_cluster_post_config: Can't find mod_manager for contexts");
        return !OK;
    }
    balancer_storage = ap_lookup_provider("manager", "shared", "3");
    if (balancer_storage == NULL) {
        ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, 0, s,
                     "proxy_cluster_post_config: Can't find mod_manager for balancers");
        return !OK;
    }
    domain_storage = ap_lookup_provider("manager", "shared", "5");
    if (domain_storage == NULL) {
        ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, 0, s,
                     "proxy_cluster_post_config: Can't find mod_manager for domains");
        return !OK;
    }

    /* Add version information */
    ap_add_version_component(p, MOD_CLUSTER_EXPOSED_VERSION);

    if (ap_state_query(AP_SQ_MAIN_STATE) == AP_SQ_MS_CREATE_PRE_CONFIG) {
        return OK;
    }

    /* add our watchdog callback */
    mc_watchdog_get_instance = APR_RETRIEVE_OPTIONAL_FN(ap_watchdog_get_instance);
    mc_watchdog_register_callback = APR_RETRIEVE_OPTIONAL_FN(ap_watchdog_register_callback);
    if (!mc_watchdog_get_instance || !mc_watchdog_register_callback) {
        ap_log_error(APLOG_MARK, APLOG_CRIT, 0, s, APLOGNO(03262) "mod_watchdog is required");
        return !OK;
    }
    if (mc_watchdog_get_instance(&watchdog, LB_CLUSTER_WATHCHDOG_NAME, 0, 1, p)) {
        ap_log_error(APLOG_MARK, APLOG_CRIT, 0, s, APLOGNO(03263) "Failed to create watchdog instance (%s)",
                     LB_CLUSTER_WATHCHDOG_NAME);
        return !OK;
    }
    while (s) {
        if (mc_watchdog_register_callback(watchdog, AP_WD_TM_SLICE, s, mc_watchdog_callback)) {
            ap_log_error(APLOG_MARK, APLOG_CRIT, 0, s, APLOGNO(03264) "Failed to register watchdog callback (%s)",
                         LB_CLUSTER_WATHCHDOG_NAME);
            return !OK;
        }
        s = s->next;
    }
    return OK;
}

static void register_hooks(apr_pool_t *p)
{
    static const char *const aszPre[] = {"mod_manager.c", "mod_rewrite.c", NULL};
    static const char *const aszSucc[] = {"mod_proxy.c", NULL};

    ap_register_provider(p, PROXY_LBMETHOD, "cluster", "0", &cluster);

    ap_hook_translate_name(lbmethod_cluster_trans, aszPre, aszSucc, APR_HOOK_FIRST);
    ap_hook_post_config(lbmethod_cluster_post_config, NULL, NULL, APR_HOOK_MIDDLE);
}


AP_DECLARE_MODULE(lbmethod_cluster) = {
    STANDARD20_MODULE_STUFF,
    NULL,               /* create per-directory config structure */
    NULL,               /* merge per-directory config structures */
    NULL,               /* create per-server config structure */
    NULL,               /* merge per-server config structures */
    NULL,               /* command apr_table_t */
    register_hooks,     /* register hooks */
    AP_MODULE_FLAG_NONE /* flags */
};
