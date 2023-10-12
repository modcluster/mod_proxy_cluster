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

#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "http_main.h"
#include "http_request.h"
#include "http_protocol.h"
#include "http_core.h"
#include "scoreboard.h"
#include "ap_mpm.h"
#include "mpm_common.h"
#include "mod_proxy.h"
#include "mod_watchdog.h"

#include "ap_slotmem.h"

#include "node.h"
#include "host.h"
#include "context.h"
#include "balancer.h"
#include "sessionid.h"
#include "domain.h"

#include "common.h"

#include "mod_proxy_cluster.h"

#if APR_HAVE_UNISTD_H
/* for getpid() */
#include <unistd.h>
#endif

#if APR_HAS_THREADS
#include "apr_thread_pool.h"
#endif

/* define HAVE_CLUSTER_EX_DEBUG to have extented debug in mod_cluster */
#define HAVE_CLUSTER_EX_DEBUG 0

/* define OUR load balancer method names (lbpname), must start by MC */
/* default behaviour be sticky StickySession="yes" */
#define MC_STICKY             "MC"
/* don't be STICKY use the best factor worker StickySession="no" */
#define MC_NOT_STICKY         "MC_NS"
/* remove session information on fail-over StickySessionRemove="yes", implies StickySession="yes" */
#define MC_REMOVE_SESSION     "MC_R"
/* Don't failover if the corresponding worker is failing StickySessionForce="yes", implies StickySession="yes" */
#define MC_NO_FAILOVER        "MC_NF"

#if APR_HAS_THREADS
#ifndef MC_USE_THREADS
#define MC_USE_THREADS 1
#endif
#else
#define MC_USE_THREADS 0
#endif

#define MC_THREADPOOL_SIZE (16)

struct proxy_cluster_helper
{
    int count_active; /* currently active request using the worker */
    proxy_worker_shared *shared;
    int index;     /* like the worker->id */
    int isinnodes; /* the proxy_worker_shared is in our shared memory */
};
typedef struct proxy_cluster_helper proxy_cluster_helper;

typedef struct watchdog_thread_args
{
    proxy_worker *worker;
    apr_pool_t *pool;
    proxy_server_conf *conf;
    nodeinfo_t *ou;
    server_rec *server;
    apr_time_t now;
    int id;
} watchdog_thread_args_t;

static struct node_storage_method *node_storage = NULL;
static struct host_storage_method *host_storage = NULL;
static struct context_storage_method *context_storage = NULL;
static struct balancer_storage_method *balancer_storage = NULL;
static struct sessionid_storage_method *sessionid_storage = NULL;
static struct domain_storage_method *domain_storage = NULL;

#define LB_CLUSTER_WATHCHDOG_NAME ("_mod_cluster_")
static APR_OPTIONAL_FN_TYPE(ap_watchdog_set_callback_interval) *mc_watchdog_set_interval;
static ap_watchdog_t *watchdog;

#if MC_USE_THREADS
static apr_thread_pool_t *mc_thread_pool;
static int mc_thread_pool_size;
#endif

static server_rec *main_server = NULL;
#define CREAT_ALL  0 /* create balancers/workers in all VirtualHost */
#define CREAT_NONE 1 /* don't create balancers (but add workers) */
#define CREAT_ROOT 2 /* Only create balancers/workers in the main server */
static int creat_bal = CREAT_ROOT;

static int use_alias = 0; /* 1 : Compare Alias with server_name */
static int deterministic_failover = 0;
static int use_nocanon = 0;

static apr_time_t lbstatus_recalc_time =
    apr_time_from_sec(5); /* recalcul the lbstatus based on number of request in the time interval */

static apr_time_t wait_for_remove = apr_time_from_sec(10); /* wait until that before removing a removed node */

static int enable_options = 1; /* Use OPTIONS * for CPING/CPONG */

#define TIMESESSIONID 300 /* after 5 minutes the sessionid have probably timeout */
#define TIMEDOMAIN    300 /* after 5 minutes the sessionid have probably timeout */


/* Create static table references we can look back to instead of fetching each request */
static apr_time_t cache_share_for = apr_time_from_sec(0); /* cache the shared memory in the process for n seconds */
static apr_time_t last_cached = 0;
static apr_pool_t *cached_pool = NULL;
static proxy_vhost_table *cached_vhost_table = NULL;
static proxy_context_table *cached_context_table = NULL;
static proxy_balancer_table *cached_balancer_table = NULL;
static proxy_node_table *cached_node_table = NULL;

/* for the hctemplate stuff */
static char *proxyhctemplate = NULL;
static APR_OPTIONAL_FN_TYPE(set_worker_hc_param) *set_worker_hc_param_f = NULL;

/* To stop the watchdog loop */
static int child_stopping = 0;

static int proxy_worker_cmp(const void *a, const void *b)
{
    const char *route1 = (*(proxy_worker **)a)->s->route;
    const char *route2 = (*(proxy_worker **)b)->s->route;
    return strcmp(route1, route2);
}

/* Compare proxy host with node host */
static int compare_hostname(const char *proxyhostname, const char *nodehostname)
{
    if (nodehostname[0] == '[') {
        const char *ptr = nodehostname;
        ptr++;
        return strncasecmp(ptr, proxyhostname, strlen(ptr) - 1);
    }

    return strcasecmp(proxyhostname, nodehostname);
}

static char *normalize_hostname(apr_pool_t *p, const char *hostname)
{
    char *ret = apr_palloc(p, strlen(hostname) + 1);
    char *ptr = ret;
    strcpy(ptr, hostname);
    for (; *ptr; ++ptr) {
        *ptr = apr_tolower(*ptr);
    }
    return ret;
}

/* Normalize the worker name */
static char *normalize_workername(apr_pool_t *pool, const char *url)
{
    apr_uri_t uri;
    if (apr_uri_parse(pool, url, &uri) != APR_SUCCESS) {
        return NULL;
    }
    if (!uri.scheme) {
        return NULL;
    }
    if (uri.port && uri.port == ap_proxy_port_of_scheme(uri.scheme)) {
        uri.port = 0;
    }
    return apr_uri_unparse(pool, &uri, APR_URI_UNP_REVEALPASSWORD);
}


/* Add health to the worker */
static void add_hcheck(server_rec *s, const proxy_server_conf *conf, proxy_worker *worker)
{
    if (set_worker_hc_param_f) {
        const char *arg = apr_pstrdup(conf->pool, proxyhctemplate);
        while (*arg) {
            char *key, *val;
            const char *err;
            key = ap_getword_conf(conf->pool, &arg);
            val = strchr(key, '=');
            if (!val) {
                ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                             "Invalid ProxyHCTemplate parameter. Parameter must be "
                             "in the form 'key=value'");
                return;
            }

            *val++ = '\0';
            err = set_worker_hc_param_f(conf->pool, s, worker, key, val, NULL);
            if (err != NULL) {
                ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "error %s for key: %s=%s", err, key, val);
            } else {
                ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, "hcheck %s=%s add to worker %s", key, val,
                             worker->s->name_ex);
            }
        }
    }
}

static int (*ap_proxy_retry_worker_fn)(const char *proxy_function, proxy_worker *worker, server_rec *s) = NULL;

static void check_workers(const proxy_server_conf *conf, const server_rec *s)
{
    int i;
    proxy_balancer *balancer;
    balancer = (proxy_balancer *)conf->balancers->elts;
    for (i = 0; i < conf->balancers->nelts; i++, balancer++) {
        int j;
        proxy_worker **workers;
        workers = (proxy_worker **)balancer->workers->elts;
        for (j = 0; j < balancer->workers->nelts; j++, workers++) {
            volatile proxy_worker *worker = *workers;
            proxy_cluster_helper *helper;
            int stop_worker = 0;
            helper = (proxy_cluster_helper *)worker->context;
            ap_assert(helper); /* we are in trouble ... */
            if (worker->s->port == 0 && worker->s->scheme[0] == '\0' && worker->s->hostname[0] == '\0' &&
                worker->s->route[0] == '\0') {
                /* this happens when a new child process is created and it "cleaned" some old slotmem */
                /* it is like the remove_workers_node we try to restore the non shared memory allocated in
                 * create_worker() */
                ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "check_workers: DOING (empty port : %d id : %d",
                             helper->shared->port, helper->index);
                stop_worker = 1;
            }
            if (worker->s->port != helper->shared->port || strcmp(worker->s->scheme, helper->shared->scheme) ||
                strcmp(worker->s->hostname, helper->shared->hostname)) {
                /* here the shared memory has changed since we created the worker */
                ap_log_error(APLOG_MARK, APLOG_ERR, 0, s, "check_workers: DOING (changed %d : %d)",
                             helper->shared->port, worker->s->port);
                stop_worker = 1;
            }
            if (stop_worker) {
                /* if count_active is not zero we are probably in trouble */
                helper->count_active = 0;
                ap_assert(helper->shared);
                ap_assert(helper->shared->port != 0);
                worker->s = helper->shared;
                helper->isinnodes = 0;
                /* If we use hcheck, we need to stop it for the worker */
                if (proxyhctemplate != NULL) {
                    worker->s->method = NONE;
                    worker->s->updated = 0;
                    worker->s->status |= PROXY_WORKER_STOPPED;
                }
                continue;
            }
        }
    }
}

static apr_status_t create_worker_reuse(proxy_server_conf *conf, const char *ptr_node, proxy_worker *worker,
                                        proxy_cluster_helper **helper_ptr, server_rec *server,
                                        proxy_worker_shared **shared, const nodeinfo_t *node, const char *url)
{
    apr_status_t rv;
    proxy_cluster_helper *helper;
    const char *ptr;

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server, "create_worker: worker for %s already exists", url);
    if (!worker->context) {
        /* That is BalancerMember, we dropped support of it */
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, server, "create_worker: reusing BalancerMember worker for %s", url);
        return APR_EGENERAL;
    }
    *helper_ptr = (proxy_cluster_helper *)worker->context;
    helper = *helper_ptr;
    if (helper->index == -1) {
        /* We are going to reuse a removed one */
        ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, server, "create_worker_reuse: reusing removed worker for %s", url);
        return APR_SUCCESS;
    }

    /* Check if the shared memory goes to the right place */
    ptr = ptr_node + node->offset;
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server, "create_worker: reusing worker (id %d) for %s", node->mess.id,
                 url);
    if (helper->index == node->mess.id && worker->s == (proxy_worker_shared *)ptr) {
        /* the shared memory may have been removed and recreated */
        if (!worker->s->status) {
            worker->s->status = PROXY_WORKER_INITIALIZED;
            strncpy(worker->s->route, node->mess.JVMRoute, sizeof(worker->s->route));
            worker->s->route[sizeof(worker->s->route) - 1] = '\0';
            strncpy(worker->s->upgrade, node->mess.Upgrade, sizeof(worker->s->upgrade));
            worker->s->upgrade[sizeof(worker->s->upgrade) - 1] = '\0';
            strncpy(worker->s->secret, node->mess.AJPSecret, sizeof(worker->s->secret));
            worker->s->secret[sizeof(worker->s->secret) - 1] = '\0';
            if (node->mess.ResponseFieldSize > 0) {
                worker->s->response_field_size = node->mess.ResponseFieldSize;
            }
            worker->s->response_field_size_set = node->mess.ResponseFieldSize > 0 ? 1 : 0;
            /* XXX: We need that information from TC */
            worker->s->redirect[0] = '\0';
            worker->s->lbstatus = 0;
            worker->s->lbfactor = -1; /* prevent using the node using status message */

            /* add health check */
            worker->s->updated = apr_time_now();
            if (proxyhctemplate != NULL) {
                add_hcheck(server, conf, worker);
            }
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server,
                         "create_worker: REUSING %s (scheme: %s hostname %s port %d route %s name %s) cleaning...", url,
                         worker->s->scheme, worker->s->hostname_ex, worker->s->port, worker->s->route,
                         worker->s->name_ex);
        }
        return APR_SUCCESS; /* Done Already existing */
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server,
                 "create_worker: can't reuse worker as it is for %s (scheme: %s hostname %s port %d route %s "
                 "name %s) cleaning...",
                 url, worker->s->scheme, worker->s->hostname_ex, worker->s->port, worker->s->route, worker->s->name_ex);
    ptr = ptr_node + node->offset;
    *shared = worker->s;
    worker->s = (proxy_worker_shared *)ptr;
    worker->s->was_malloced = 0; /* Prevent mod_proxy to free it */
    helper->isinnodes = 1;
    helper->index = node->mess.id;

    if ((rv = ap_proxy_initialize_worker(worker, server, conf->pool)) != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, rv, server, "create_worker: ap_proxy_initialize_worker failed %d for %s",
                     rv, url);
        return rv;
    }

    worker->s->index = node->mess.id;
    /* add health check */
    worker->s->updated = apr_time_now();
    if (proxyhctemplate != NULL) {
        add_hcheck(server, conf, worker);
    }
    return APR_SUCCESS;
}

static char *create_worker_build_name(const nodeinfo_t *node, apr_uri_t *uri, server_rec *server, apr_pool_t *pool)
{
    char *url;
    url = apr_pstrcat(pool, node->mess.Type, "://", normalize_hostname(pool, node->mess.Host), ":", node->mess.Port,
                      NULL);
    if (apr_uri_parse(pool, url, uri) != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, server, "create_worker: worker for %s failed: Unable to parse URL", url);
        return NULL;
    }
    if (!uri->scheme) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, server, "create_worker: worker for %s failed: URL must be absolute!",
                     url);
        return NULL;
    }
    if (uri->port && uri->port == ap_proxy_port_of_scheme(uri->scheme)) {
        url = apr_pstrcat(pool, node->mess.Type, "://", normalize_hostname(pool, node->mess.Host), NULL);
    }
    return url;
}

static void create_worker_arrange_shared_mem(proxy_server_conf *conf, proxy_worker *worker, server_rec *server,
                                             proxy_worker_shared *shared, const nodeinfo_t *node, const char *url)
{
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server, "create_worker: worker for %s arranging shared memory %s:%s", url,
                 worker->s->name_ex, shared->name_ex);
    worker->s->was_malloced = 0; /* Prevent mod_proxy to free it */
    worker->s->index = node->mess.id;
    strncpy(worker->s->name_ex, shared->name_ex, sizeof(worker->s->name_ex));
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server,
                 "create_worker: worker for %s arranging shared memory hostname %s:%s", url, worker->s->hostname,
                 shared->hostname);
    strncpy(worker->s->hostname, shared->hostname, sizeof(worker->s->hostname));
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server,
                 "create_worker: worker for %s arranging shared memory hostname_ex %s:%s", url, worker->s->hostname_ex,
                 shared->hostname_ex);
    strncpy(worker->s->hostname_ex, shared->hostname_ex, sizeof(worker->s->hostname_ex));
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server,
                 "create_worker: worker for %s arranging shared memory scheme %s:%s", url, worker->s->scheme,
                 shared->scheme);
    strncpy(worker->s->scheme, shared->scheme, sizeof(worker->s->scheme));
    worker->s->port = shared->port;
    worker->s->hmax = shared->hmax;
    strncpy(worker->s->route, node->mess.JVMRoute, sizeof(worker->s->route));
    worker->s->route[sizeof(worker->s->route) - 1] = '\0';
    strncpy(worker->s->upgrade, node->mess.Upgrade, sizeof(worker->s->upgrade));
    worker->s->upgrade[sizeof(worker->s->upgrade) - 1] = '\0';
    if (node->mess.ResponseFieldSize > 0) {
        worker->s->response_field_size = node->mess.ResponseFieldSize;
        worker->s->response_field_size_set = 1;
    } else {
        worker->s->response_field_size_set = 0;
    }
    strncpy(worker->s->secret, node->mess.AJPSecret, sizeof(worker->s->secret));
    worker->s->secret[sizeof(worker->s->secret) - 1] = '\0';
    worker->s->redirect[0] = '\0';
    worker->s->smax = node->mess.smax;
    worker->s->ttl = node->mess.ttl;
    if (node->mess.timeout) {
        worker->s->timeout_set = 1;
        worker->s->timeout = node->mess.timeout;
    }
    worker->s->flush_packets = node->mess.flushpackets;
    worker->s->flush_wait = node->mess.flushwait;
    worker->s->ping_timeout = node->mess.ping;
    worker->s->ping_timeout_set = 1;
    worker->s->acquire_set = 1;
    worker->s->conn_timeout_set = 1;
    worker->s->conn_timeout = node->mess.ping;
    worker->s->keepalive = 1;
    worker->s->keepalive_set = 1;
    worker->s->is_address_reusable = 1;
    worker->s->acquire = apr_time_make(0, 2 * 1000); /* 2 ms */
    worker->s->retry = apr_time_from_sec(PROXY_WORKER_DEFAULT_RETRY);

    /* check add health check */
    worker->s->updated = apr_time_now();
    if (proxyhctemplate != NULL) {
        add_hcheck(server, conf, worker);
    }
}

/**
 * Add a node to the worker conf
 * XXX: Contains code of ap_proxy_initialize_worker (proxy_util.c)
 * XXX: If something goes wrong the worker can't be used and we leak memory... in a pool
 * XXX: the share memory must be locked before calling it.
 * NOTE: pool is the request pool or any temporary pool. Use conf->pool for any data that live longer.
 * @param node the pointer to the node structure
 * @param ptr_node the address of the node in shared memory.
 * @param conf a proxy_server_conf.
 * @param balancer the balancer to update.
 * @param pool a temporary pool.
 * @server the server rec for logging purposes.
 *
 */
static apr_status_t create_worker(proxy_server_conf *conf, proxy_balancer *balancer, server_rec *server,
                                  const nodeinfo_t *node, const char *ptr_node, apr_pool_t *pool)
{
    char *url;
    const char *ptr;
    const char *err;
    apr_status_t rv = APR_SUCCESS;
    proxy_worker *worker;
    proxy_worker_shared *shared;
    proxy_cluster_helper *helper;
    apr_uri_t uri;

    /* build the name (scheme and port) when needed */
    if ((url = create_worker_build_name(node, &uri, server, pool)) == NULL) {
        return APR_EGENERAL;
    }

    /* Check if the corresponding woker already exist. */
    worker = ap_proxy_get_worker(pool, balancer, conf, url);
    if (worker != NULL) {
        /* Yes, it exists. We will reuse already existing worker */
        return create_worker_reuse(conf, ptr_node, worker, &helper, server, &shared, node, url);
    }

    /* No, it does not exist, so we will create a new one.
     * Note that the ap_proxy_get_worker and ap_proxy_define_worker aren't symetrical, and
     * this leaks via the conf->pool
     */
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server, "create_worker: worker for %s Will create %d!!!", url,
                 node->mess.id);
    err = ap_proxy_define_worker(conf->pool, &worker, balancer, conf, url, 0);
    if (err) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, server, "create_worker: worker for %s failed: %s", url, err);
        return APR_EGENERAL;
    }

    worker->context = (proxy_cluster_helper *)apr_pcalloc(conf->pool, sizeof(proxy_cluster_helper));
    if (!worker->context) {
        return APR_EGENERAL;
    }

    helper = (proxy_cluster_helper *)worker->context;
    helper->count_active = 0;
    helper->shared = worker->s;
    helper->isinnodes = 0;
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server, "create_worker: worker for %s", url);

    /* Get the shared memory for this worker. We are here for 2 reasons:
     * 1 - the worker was created.
     * 2 - we are reusing a removed worker.
     */
    ptr = ptr_node + node->offset;
    shared = worker->s;
    worker->s = (proxy_worker_shared *)ptr;
    helper->isinnodes = 1;
    helper->index = node->mess.id;

    /* Changing the shared memory requires locking it... */
    if (strncmp(worker->s->name_ex, shared->name_ex, sizeof(worker->s->name_ex))) {
        /* We will modify it only if the name has changed to minimize access */
        create_worker_arrange_shared_mem(conf, worker, server, shared, node, url);
    } else {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server, "create_worker: worker for %s shared memory  OK %s:%s", url,
                     worker->s->name_ex, shared->name_ex);
        worker->s->was_malloced = 0; /* Prevent mod_proxy to free it */
    }

    if ((rv = ap_proxy_initialize_worker(worker, server, conf->pool)) != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, rv, server, "create_worker: ap_proxy_initialize_worker failed %d for %s:%s",
                     rv, url, ptr);
        return rv;
    }
    worker->s->index = node->mess.id;

    /* The Shared datastatus may already contain a valid information */
    if (!worker->s->status) {
        worker->s->status = PROXY_WORKER_INITIALIZED;
        /* XXX: We need that information from TC */
        worker->s->redirect[0] = '\0';
        worker->s->lbstatus = 0;
        worker->s->lbfactor = -1; /* prevent using the node using status message */
    }

    return APR_SUCCESS;
}

static balancerinfo_t *read_balancer_name(const char *name, apr_pool_t *pool)
{
    int sizebal, i;
    int *bal;
    sizebal = balancer_storage->get_max_size_balancer();
    if (sizebal == 0) {
        return NULL; /* Done broken. */
    }
    bal = apr_pcalloc(pool, sizeof(int) * sizebal);
    sizebal = balancer_storage->get_ids_used_balancer(bal);
    for (i = 0; i < sizebal; i++) {
        balancerinfo_t *balan;
        balancer_storage->read_balancer(bal[i], &balan);
        /* Something like balancer://cluster1 and cluster1 */
        if (strcmp(balan->balancer, name) == 0) {
            return balan;
        }
    }
    return NULL;
}

/**
 * Add balancer to the proxy_server_conf.
 * NOTE: pool is the request pool or any temporary pool. Use conf->pool for any data that live longer.
 * @param node the pointer to the node structure (contains the balancer information).
 * @param conf a proxy_server_conf.
 * @param balancer the balancer to update or NULL to create it.
 * @param name the name of the balancer.
 * @param pool a temporary pool.
 * @server the server rec for logging purposes.
 *
 */
static proxy_balancer *add_balancer_node(const nodeinfo_t *node, proxy_server_conf *conf, apr_pool_t *pool,
                                         const server_rec *server)
{
    proxy_balancer *balancer = NULL;
    char *name = apr_pstrcat(pool, "balancer://", node->mess.balancer, NULL);

    balancer = ap_proxy_get_balancer(pool, conf, name, 0);
    if (!balancer) {
        int sizeb = conf->balancers->elt_size;
        proxy_balancer_shared *bshared;
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server, "add_balancer_node: Create balancer %s", name);

        balancer = apr_array_push(conf->balancers);
        memset(balancer, 0, sizeb);

        balancer->gmutex = NULL;
        bshared = apr_palloc(conf->pool, sizeof(proxy_balancer_shared));
        memset(bshared, 0, sizeof(proxy_balancer_shared));
        if (PROXY_STRNCPY(bshared->sname, name) != APR_SUCCESS) {
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, server, "add_balancer_node: balancer safe-name (%s) too long", name);
            return NULL;
        }
        bshared->hash.def = ap_proxy_hashfunc(name, PROXY_HASHFUNC_DEFAULT);
        bshared->hash.fnv = ap_proxy_hashfunc(name, PROXY_HASHFUNC_FNV);
        balancer->s = bshared;
        balancer->hash = bshared->hash;
        balancer->sconf = conf;
        if (apr_thread_mutex_create(&(balancer->tmutex), APR_THREAD_MUTEX_DEFAULT, conf->pool) != APR_SUCCESS) {
            /* XXX: Do we need to log something here? */
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, server, "add_balancer_node: Can't create lock for balancer");
        }
        balancer->workers = apr_array_make(conf->pool, 5, sizeof(proxy_worker *));
        strncpy(balancer->s->name, name, PROXY_BALANCER_MAX_NAME_SIZE - 1);
        /* XXX: TODO we should have our own lbmethod(s), this one is the mod_proxy_balancer default one! */
        balancer->lbmethod = ap_lookup_provider(PROXY_LBMETHOD, "byrequests", "0");
    } else {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server, "add_balancer_node: Using balancer %s", name);
    }

    if (balancer && balancer->workers->nelts == 0) {
        /* Logic to copy the shared memory information to the balancer */
        balancerinfo_t *balan = read_balancer_name(&balancer->s->name[11], pool);
        if (balan == NULL) {
            return balancer; /* Done broken */
        }
        /* StickySession, StickySessionRemove we hack it via the lbpname (16 bytes) */
        strcpy(balancer->s->lbpname, balan->StickySession ? MC_STICKY : MC_NOT_STICKY);

        if (balan->StickySessionRemove) {
            strcpy(balancer->s->lbpname, MC_REMOVE_SESSION);
        }

        strncpy(balancer->s->sticky, balan->StickySessionCookie, PROXY_BALANCER_MAX_STICKY_SIZE - 1);
        balancer->s->sticky[PROXY_BALANCER_MAX_STICKY_SIZE - 1] = '\0';
        strncpy(balancer->s->sticky_path, balan->StickySessionPath, PROXY_BALANCER_MAX_STICKY_SIZE - 1);
        balancer->s->sticky_path[PROXY_BALANCER_MAX_STICKY_SIZE - 1] = '\0';
        if (balan->StickySessionForce) {
            strcpy(balancer->s->lbpname, MC_NO_FAILOVER);
            balancer->s->sticky_force = 1;
            balancer->s->sticky_force_set = 1;
        }
        balancer->s->timeout = balan->Timeout;
        balancer->s->max_attempts = balan->Maxattempts;
        balancer->s->max_attempts_set = 1;
    }
    return balancer;
}

/* We "reuse" the balancer */
static void reuse_balancer(proxy_balancer *balancer, const char *name, apr_pool_t *pool, const server_rec *s)
{
    balancerinfo_t *balan = read_balancer_name(name, pool);
    int changed = 0;
    if (balan == NULL) {
        return;
    }

    if (strncmp(balancer->s->lbpname, "MC", 2)) {
        /* replace the configured lbpname by our default one */
        strcpy(balancer->s->lbpname, MC_STICKY);
        changed = 1;
    }
    if (balan->StickySessionForce && !balancer->s->sticky_force) {
        balancer->s->sticky_force = 1;
        balancer->s->sticky_force_set = 1;
        strcpy(balancer->s->lbpname, MC_NO_FAILOVER);
        changed = 1;
    }
    if (!balan->StickySessionForce && balancer->s->sticky_force) {
        balancer->s->sticky_force = 0;
        strcpy(balancer->s->lbpname, MC_STICKY);
        changed = 1;
    }
    if (balan->StickySessionForce && strcmp(balancer->s->lbpname, MC_NO_FAILOVER)) {
        strcpy(balancer->s->lbpname, MC_NO_FAILOVER);
        changed = 1;
    }
    if (balan->StickySessionRemove && strcmp(balancer->s->lbpname, MC_REMOVE_SESSION)) {
        strcpy(balancer->s->lbpname, MC_REMOVE_SESSION);
        changed = 1;
    }
    if (!balan->StickySession && strcmp(balancer->s->lbpname, MC_NOT_STICKY)) {
        strcpy(balancer->s->lbpname, MC_NOT_STICKY);
        changed = 1;
    }
    if (strcmp(balan->StickySessionCookie, balancer->s->sticky) != 0) {
        strncpy(balancer->s->sticky, balan->StickySessionCookie, PROXY_BALANCER_MAX_STICKY_SIZE - 1);
        balancer->s->sticky[PROXY_BALANCER_MAX_STICKY_SIZE - 1] = '\0';
        changed = 1;
    }
    if (strcmp(balan->StickySessionPath, balancer->s->sticky_path) != 0) {
        strncpy(balancer->s->sticky_path, balan->StickySessionPath, PROXY_BALANCER_MAX_STICKY_SIZE - 1);
        balancer->s->sticky_path[PROXY_BALANCER_MAX_STICKY_SIZE - 1] = '\0';
        changed = 1;
    }
    balancer->s->timeout = balan->Timeout;
    balancer->s->max_attempts = balan->Maxattempts;
    balancer->s->max_attempts_set = 1;
    if (changed) {
        /* log a warning */
        ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, s, "Balancer %s changed", &balancer->s->name[11]);
    }
}

/*
 * Adds the balancers and the workers to the VirtualHosts corresponding to node using given server
 * NOTE: the calling routine should lock before calling us.
 * @param node the node information to add.
 * @param ptr_node the addr of the node in shared memory
 * @param pool  temporary pool to use for temporary buffer.
 * @param server the server to use
 */
static void add_balancers_workers_for_server(nodeinfo_t *node, const char *ptr_node, apr_pool_t *pool, server_rec *s)
{
    char *name = apr_pstrcat(pool, "balancer://", node->mess.balancer, NULL);

    void *sconf = s->module_config;
    proxy_server_conf *conf = (proxy_server_conf *)ap_get_module_config(sconf, &proxy_module);
    proxy_balancer *balancer = ap_proxy_get_balancer(pool, conf, name, 0);

    if (!balancer && (creat_bal == CREAT_NONE || (creat_bal == CREAT_ROOT && s != main_server))) {
        return;
    }
    if (!balancer) {
        balancer = add_balancer_node(node, conf, pool, s);
    } else {
        /* We "reuse" the balancer */
        reuse_balancer(balancer, &balancer->s->name[11], pool, s);
    }
    if (balancer) {
        create_worker(conf, balancer, s, node, ptr_node, pool);
    }
}

/*
 * Adds the balancers and the workers to the VirtualHosts corresponding to node
 * NOTE: the calling routine should lock before calling us.
 * @param node the node information to add.
 * @param ptr_node address of the node in shared memory.
 * @param pool  temporary pool to use for temporary buffer.
 */
static void add_balancers_workers(nodeinfo_t *node, const char *ptr_node, apr_pool_t *pool)
{
    server_rec *s = main_server;
    while (s) {
        add_balancers_workers_for_server(node, ptr_node, pool, s);
        s = s->next;
    }
}

/*
 * Get the worker corresponding to the given id.
 * NOTE: we need to compare the shared memory pointer too
 */
static proxy_worker *get_worker_from_id_stat(const proxy_server_conf *conf, int id, const proxy_worker_shared *stat,
                                             const nodeinfo_t *node)
{
    int i;
    char *ptr = conf->balancers->elts;
    int sizeb = conf->balancers->elt_size;
    int sizew = sizeof(proxy_worker *);

    for (i = 0; i < conf->balancers->nelts; i++, ptr = ptr + sizeb) {
        int j;
        char *ptrw;
        proxy_balancer *balancer = (proxy_balancer *)ptr;
        ptrw = balancer->workers->elts;
        for (j = 0; j < balancer->workers->nelts; j++, ptrw = ptrw + sizew) {
            proxy_worker **worker = (proxy_worker **)ptrw;
            proxy_cluster_helper *helper = (proxy_cluster_helper *)(*worker)->context;
            if ((*worker)->s == stat && helper->index == id) {
                return *worker;
            }
            if (helper->index == id) {
                (*worker)->s->index = -1;
            }
        }
    }
    return NULL;
}

/* Remove a node from the worker conf */
static int remove_workers_node(nodeinfo_t *node, proxy_server_conf *conf, apr_pool_t *pool, server_rec *server)
{
    int i;
    char *pptr = (char *)node;
    proxy_cluster_helper *helper;
    proxy_worker *worker;
    pptr = pptr + node->offset;

    worker = get_worker_from_id_stat(conf, node->mess.id, (proxy_worker_shared *)pptr, node);
    (void)pool;

    if (!worker) {
        /* XXX: Another process may use it, can't do: node_storage->remove_node(node); */
        return 0; /* Done */
    }

    /* prevent other threads using it */
    worker->s->status = worker->s->status | PROXY_WORKER_IN_ERROR;

    /* apr_reslist_acquired_count */
    i = 0;

    helper = (proxy_cluster_helper *)worker->context;
    if (helper) {
        i = helper->count_active;
    } else {
        ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, server, "remove_workers_node: helper is NULL");
    }
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server, "remove_workers_node: helper count_active: %d JVMRoute: %s", i,
                 node->mess.JVMRoute);

    if (i == 0) {
        /* The worker already comes from the apr_array of the balancer */
        proxy_worker_shared *stat = worker->s;

        /* Here that is tricky the worker needs shared but we don't and CONFIG will reset it */
        worker->s = helper->shared;
        helper->isinnodes = 0;
        ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, server,
                     "remove_workers_node: scheme %s hostname %s port %d route %s name (%s):(%s) id (%d:%d)",
                     stat->scheme, stat->hostname_ex, stat->port, stat->route, stat->name, helper->shared->name,
                     stat->index, helper->index);
        ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, server,
                     "remove_workers_node: restored: scheme %s hostname %s port %d route %s name %s id:%d",
                     worker->s->scheme, worker->s->hostname_ex, worker->s->port, worker->s->route, worker->s->name,
                     worker->s->index);
        /* XXX : look bad with new logic!!!! memcpy(worker->s, stat, sizeof(proxy_worker_shared)); */
        ap_assert(worker->s->port != 0);

        /* If we use hcheck, we need to stop it for the worker */
        if (proxyhctemplate != NULL) {
            worker->s->method = NONE;
            worker->s->updated = 0;
            worker->s->status |= PROXY_WORKER_STOPPED;
        }

        return 0;
    }

    node->mess.lastcleantry = apr_time_now();
    return 1; /* We should retry later */
}

/*
 * Create/Remove workers corresponding to updated nodes.
 * NOTE: It is called from proxy_cluster_watchdog_func and other locations
 *       It shouldn't call worker_nodes_are_updated() because there may be several VirtualHosts.
 */
static void update_workers_node(const proxy_server_conf *conf, apr_pool_t *pool, server_rec *server, int check,
                                proxy_node_table *node_table)
{
    int i;
    (void)conf;

    if (node_table == NULL) {
        return;
    }

    /* Check if we have to do something */
    if (check) {
        /* nodes_need_update will return 1 if last_updated is zero: first time we are called */
        if (node_storage->worker_nodes_need_update(main_server, pool) == 0) {
            return;
        }
    }

    /* XXX: How to skip the balancer that aren't controled by mod_manager */

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server, "update_workers_node: Starting");

    /* Only process the nodes that have been updated since our last update */
    for (i = 0; i < node_table->sizenode; i++) {
        nodeinfo_t *ou = &node_table->node_info[i];
        if (ou->mess.remove) {
            continue;
        }
        if (node_table->ptr_node[i] == NULL) {
            continue;
        }
        add_balancers_workers_for_server(ou, node_table->ptr_node[i], pool, server);
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server, "update_workers_node: Done");
}

/*
 * Do a ping/pong to the node
 * XXX: ajp_handle_cping_cpong should come from a provider as
 * it is already in modules/proxy/ajp_utils.c
 */
static apr_status_t ajp_handle_cping_cpong(apr_socket_t *sock, const request_rec *r, apr_interval_time_t timeout)
{
    char buf[5];
    apr_size_t written = 5;
    apr_interval_time_t org;
    apr_status_t status;
    apr_status_t rv;

    /* built the cping message */
    buf[0] = 0x12;
    buf[1] = 0x34;
    buf[2] = (apr_byte_t)0;
    buf[3] = (apr_byte_t)1;
    buf[4] = (unsigned char)10;

    status = apr_socket_send(sock, buf, &written);
    if (status != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, status, r->server, "ajp_cping_cpong: send failed");
        return status;
    }
    status = apr_socket_timeout_get(sock, &org);
    if (status != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, status, r->server, "ajp_cping_cpong: apr_socket_timeout_get failed");
        return status;
    }
    status = apr_socket_timeout_set(sock, timeout);
    written = 5;
    status = apr_socket_recv(sock, buf, &written);
    if (status != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, status, r->server, "ajp_cping_cpong: apr_socket_recv failed");
        goto cleanup;
    }
    if (buf[0] != 0x41 || buf[1] != 0x42 || buf[2] != 0 || buf[3] != 1 || buf[4] != (unsigned char)9) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server,
                     "ajp_cping_cpong: awaited CPONG, received %02x %02x %02x %02x %02x", buf[0] & 0xFF, buf[1] & 0xFF,
                     buf[2] & 0xFF, buf[3] & 0xFF, buf[4] & 0xFF);
        status = APR_EGENERAL;
    }

cleanup:
    rv = apr_socket_timeout_set(sock, org);
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, "ajp_cping_cpong: apr_socket_timeout_set failed");
        return rv;
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "ajp_cping_cpong: Done");
    return status;
}

static apr_status_t ap_proxygetline(apr_bucket_brigade *bb, char *s, int n, request_rec *r, int fold, int *writen)
{
    char *tmp_s = s;
    apr_status_t rv;
    apr_size_t len;

    rv = ap_rgetline(&tmp_s, n, &len, r, fold, bb);
    apr_brigade_cleanup(bb);

    if (rv == APR_SUCCESS) {
        *writen = (int)len;
    } else if (rv == APR_ENOSPC) {
        *writen = n;
    } else {
        *writen = -1;
    }

    return rv;
}

/* In 2.4.x the routine is public any more */
static request_rec *ap_proxy_make_fake_req(conn_rec *c, request_rec *r)
{
    apr_pool_t *pool;
    request_rec *rp;

    apr_pool_create(&pool, c->pool);

    rp = apr_pcalloc(pool, sizeof(*r));

    rp->pool = pool;
    rp->status = HTTP_OK;

    rp->headers_in = apr_table_make(pool, 50);
    rp->subprocess_env = apr_table_make(pool, 50);
    rp->headers_out = apr_table_make(pool, 12);
    rp->err_headers_out = apr_table_make(pool, 5);
    rp->notes = apr_table_make(pool, 5);

    rp->server = r->server;
    rp->log = r->log;
    rp->proxyreq = r->proxyreq;
    rp->request_time = r->request_time;
    rp->connection = c;
    rp->output_filters = c->output_filters;
    rp->input_filters = c->input_filters;
    rp->proto_output_filters = c->output_filters;
    rp->proto_input_filters = c->input_filters;
    rp->useragent_ip = c->client_ip;
    rp->useragent_addr = c->client_addr;

    rp->request_config = ap_create_request_config(pool);
    proxy_run_create_req(r, rp);

    return rp;
}

/* Do a ping/pong to the node */
static apr_status_t http_handle_cping_cpong(proxy_conn_rec *p_conn, request_rec *r, apr_interval_time_t timeout)
{
    char *srequest;
    char buffer[HUGE_STRING_LEN];
    int len;
    apr_status_t status, rv;
    apr_interval_time_t org;
    apr_bucket_brigade *header_brigade, *tmp_bb;
    apr_bucket *e;
    request_rec *rp;

    srequest = apr_pstrcat(r->pool, "OPTIONS * HTTP/1.0\r\nUser-Agent: ", ap_get_server_banner(),
                           " (internal mod_cluster connection)\r\n\r\n", NULL);
    header_brigade = apr_brigade_create(r->pool, p_conn->connection->bucket_alloc);
    e = apr_bucket_pool_create(srequest, strlen(srequest), r->pool, p_conn->connection->bucket_alloc);
    APR_BRIGADE_INSERT_TAIL(header_brigade, e);
    e = apr_bucket_flush_create(p_conn->connection->bucket_alloc);
    APR_BRIGADE_INSERT_TAIL(header_brigade, e);

    status = ap_pass_brigade(p_conn->connection->output_filters, header_brigade);
    apr_brigade_cleanup(header_brigade);
    if (status != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, status, r->server, "http_cping_cpong: send failed");
        p_conn->close = 1;
        return status;
    }

    status = apr_socket_timeout_get(p_conn->sock, &org);
    if (status != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, status, r->server, "http_cping_cpong: apr_socket_timeout_get failed");
        p_conn->close = 1;
        return status;
    }
    status = apr_socket_timeout_set(p_conn->sock, timeout);

    /* we need to read the answer */
    status = APR_EGENERAL;
    rp = ap_proxy_make_fake_req(p_conn->connection, r);
    rp->proxyreq = PROXYREQ_RESPONSE;
    tmp_bb = apr_brigade_create(r->pool, p_conn->connection->bucket_alloc);
    while (1) {
        ap_proxygetline(tmp_bb, buffer, sizeof(buffer), rp, 0, &len);
        if (len <= 0) {
            break;
        }
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "http_cping_cpong: received %s", buffer);
        status = APR_SUCCESS;
    }
    if (status != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, status, r->server, "http_cping_cpong: ap_getline failed");
    }
    rv = apr_socket_timeout_set(p_conn->sock, org);
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, "http_cping_cpong: apr_socket_timeout_set failed");
        p_conn->close = 1;
        return rv;
    }

    p_conn->close = 1;
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "http_cping_cpong: Done");
    return status;
}

static apr_status_t proxy_cluster_try_pingpong(request_rec *r, proxy_worker *worker, char *url, proxy_server_conf *conf,
                                               apr_interval_time_t ping, apr_interval_time_t workertimeout)
{
    apr_status_t status;
    apr_interval_time_t timeout;
    proxy_conn_rec *backend = NULL;
    char server_portstr[32];
    char *locurl = url;
    apr_uri_t *uri;
    char *scheme = worker->s->scheme;
    int is_ssl = 0;
    (void)ping;
    (void)workertimeout;

    if ((strcasecmp(scheme, "HTTPS") == 0 || strcasecmp(scheme, "WSS") == 0 || strcasecmp(scheme, "WS") == 0 ||
         strcasecmp(scheme, "HTTP") == 0) &&
        !enable_options) {
        /* we cant' do CPING/CPONG so we just return OK */
        return APR_SUCCESS;
    }
    if (strcasecmp(scheme, "HTTPS") == 0 || strcasecmp(scheme, "WSS") == 0) {
        if (!ap_proxy_ssl_enable(NULL)) {
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server,
                         "proxy_cluster_try_pingpong: cping_cpong failed (mod_ssl not configured?)");
            return APR_EGENERAL;
        }
        is_ssl = 1;
    }

    /* create space for state information */
    status = ap_proxy_acquire_connection(scheme, &backend, worker, r->server);
    if (status != OK) {
        if (backend) {
            backend->close = 1;
            ap_proxy_release_connection(scheme, backend, r->server);
        }
        return status;
    }

    backend->is_ssl = is_ssl;
    if (is_ssl) {
        ap_proxy_ssl_connection_cleanup(backend, r);
    }

    /* Step One: Determine Who To Connect To */
    uri = apr_palloc(r->pool, sizeof(*uri)); /* We don't use it anyway */
    server_portstr[0] = '\0';
    status = ap_proxy_determine_connection(r->pool, r, conf, worker, backend, uri, &locurl, NULL, 0, server_portstr,
                                           sizeof(server_portstr));
    if (status != OK) {
        ap_proxy_release_connection(scheme, backend, r->server);
        return status;
    }

    /* Set the timeout
     * Note that the default timeout logic in the proxy_util.c is:
     *     1 - worker->timeout (if timeout_set timeout=n in the worker)
     *     2 - conf->timeout (if timeout_set ProxyTimeout 300)
     *     3 - s->timeout (TimeOut 300).
     * We hack it... Via 1
     * Since 20051115.16 (2.2.9) there is a conn_timeout and conn_timeout_set.
     * Changing the worker->timeout is a bad idea (we have to restore the value from the shared memory).
     */
    /* Do nothing: 2.4.x has the ping_timeout and conn_timeout */
    timeout = worker->s->ping_timeout;
    if (timeout <= 0) {
        timeout = apr_time_from_sec(10); /* 10 seconds */
    }

    /* Step Two: Make the Connection */
    status = ap_proxy_connect_backend(scheme, backend, worker, r->server);
    /* Do nothing: 2.4.x has the ping_timeout and conn_timeout */
    if (status != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "proxy_cluster_try_pingpong: can't connect to backend");
        ap_proxy_release_connection(scheme, backend, r->server);
        return status;
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "proxy_cluster_try_pingpong: connected to backend");

    if (strcasecmp(scheme, "AJP") == 0) {
        status = ajp_handle_cping_cpong(backend->sock, r, timeout);
    } else {
        /* non-AJP connections */
        if (!backend->connection) {
            if ((status = ap_proxy_connection_create(scheme, backend, (conn_rec *)NULL, r->server)) == OK) {
                if (is_ssl) {
                    apr_table_set(backend->connection->notes, "proxy-request-hostname", uri->hostname);
                }
            } else {
                ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                             "proxy_cluster_try_pingpong: create connection failed");
                ap_proxy_release_connection(scheme, backend, r->server);
                return status;
            }
        }
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "proxy_cluster_try_pingpong: trying %s",
                     backend->connection->client_ip);
        status = http_handle_cping_cpong(backend, r, timeout);
    }

    if (status != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "proxy_cluster_try_pingpong: cping_cpong failed");
        backend->close = 1;
    }

    ap_proxy_release_connection(scheme, backend, r->server);
    return status;
}

/* read the node and check that it corresponds to the worker */
static apr_status_t read_node_worker(int id, nodeinfo_t **node, const proxy_worker *worker)
{
    char sport[7];
    apr_status_t status = node_storage->read_node(id, node);
    if (status != APR_SUCCESS) {
        return status;
    }
    apr_snprintf(sport, sizeof(sport), "%d", worker->s->port);
    if (strcmp(worker->s->scheme, (*node)->mess.Type) || compare_hostname(worker->s->hostname, (*node)->mess.Host) ||
        strcmp(sport, (*node)->mess.Port)) {
        /* for some reasons it is not the right node */
        return APR_NOTFOUND;
    }
    return APR_SUCCESS;
}

static void *APR_THREAD_FUNC check_proxy_worker(apr_thread_t *thread, void *data)
{
    apr_status_t rv;
    char sport[7];
    char *url;
    apr_pool_t *rrp;
    request_rec *rnew;

    watchdog_thread_args_t *targs = (watchdog_thread_args_t *)data;
    proxy_worker *worker = targs->worker;
    apr_pool_t *pool = targs->pool;
    proxy_server_conf *conf = targs->conf;
    nodeinfo_t *ou = targs->ou;
    server_rec *server = targs->server;
    apr_time_t now = targs->now;
    int id = targs->id;

    (void)thread;
    apr_snprintf(sport, sizeof(sport), "%d", worker->s->port);

    if (strchr(worker->s->hostname, ':') != NULL) {
        url = apr_pstrcat(pool, worker->s->scheme, "://[", worker->s->hostname, "]:", sport, "/", NULL);
    } else {
        url = apr_pstrcat(pool, worker->s->scheme, "://", worker->s->hostname, ":", sport, "/", NULL);
    }

    apr_pool_create(&rrp, pool);
    apr_pool_tag(rrp, "subrequest");
    rnew = apr_pcalloc(rrp, sizeof(request_rec));
    rnew->pool = rrp;
    /* we need only those ones */
    rnew->server = server;
    rnew->connection = apr_pcalloc(rrp, sizeof(conn_rec));
    rnew->connection->log_id = "-";
    rnew->connection->conn_config = ap_create_conn_config(rrp);
    rnew->log_id = "-";
    rnew->useragent_addr = apr_pcalloc(rrp, sizeof(apr_sockaddr_t));
    rnew->per_dir_config = server->lookup_defaults;
    rnew->notes = apr_table_make(rnew->pool, 1);
    rnew->method = "PING";
    rnew->uri = "/";
    rnew->headers_in = apr_table_make(rnew->pool, 1);
    rv = proxy_cluster_try_pingpong(rnew, worker, url, conf, ou->mess.ping, ou->mess.timeout);
    apr_pool_destroy(rrp);

    /* We have checked the worker... check if we were told to stop */
    if (child_stopping) {
        return APR_SUCCESS;
    }

    ap_assert(node_storage->lock_nodes() == APR_SUCCESS);
    if (read_node_worker(id, &ou, worker) != APR_SUCCESS) {
        /* the node is gone or something like that */
        node_storage->unlock_nodes();
        return APR_SUCCESS;
    }

    if (rv != APR_SUCCESS) {
        /* We can't reach the node: XXX changing ou->mess.updatetimelb here ??? */
        /* XXX if this is a timeout, we might have a outdated list of nodes!!! */
        worker->s->status |= PROXY_WORKER_IN_ERROR;
        ou->mess.num_failure_idle++;
        if (ou->mess.num_failure_idle > 60) {
            /* Failing for 5 minutes: time to mark it removed */
            ou->mess.remove = 1;
            ou->updatetime = now;
        }
    } else {
        ou->mess.num_failure_idle = 0;
    }

    node_storage->unlock_nodes();
    return APR_SUCCESS;
}

/* Returns 1 if the caller function should continue processing. */
static int internal_update_lbstatus(proxy_server_conf *conf, apr_pool_t *pool, server_rec *server, apr_time_t now,
                                    nodeinfo_t *ou, int id, const proxy_worker_shared *stat)
{
    char sport[7];
    watchdog_thread_args_t targs;
    proxy_worker *worker;
    worker = get_worker_from_id_stat(conf, id, stat, ou);

    if (worker == NULL) {
        node_storage->unlock_nodes();
        return 1; /* skip it */
    }
    apr_snprintf(sport, sizeof(sport), "%d", worker->s->port);

    if (strcmp(worker->s->scheme, ou->mess.Type) || compare_hostname(worker->s->hostname, ou->mess.Host) ||
        strcmp(sport, ou->mess.Port)) {
        node_storage->unlock_nodes();
        /* the worker doesn't correspond to the node something is very broken */
        ap_log_error(APLOG_MARK, APLOG_CRIT, 0, server,
                     "update_workers_lbstatus worker: (%s) does not correspond to the node (%s)", worker->s->hostname,
                     ou->mess.Host);
        return 1;
    }

    /* Here we should decide about using hcheck result or a request that pings the node */
    if (proxyhctemplate != NULL) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server, "update_workers_lbstatus: Using hcheck!");
        if (worker->s->status & PROXY_WORKER_NOT_USABLE_BITMAP) {
            /* marked errored by hcheck */
            ou->mess.num_failure_idle++;
            if (ou->mess.num_failure_idle > 60) {
                /* Failing for 5 minutes: time to mark it removed */
                ou->mess.remove = 1;
                ou->updatetime = now;
            }
        } else {
            ou->mess.num_failure_idle = 0;
        }
        node_storage->unlock_nodes();
        return 1; /* Done in this case */
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server, "update_workers_lbstatus: Using old logic!");
    node_storage->unlock_nodes();

    /* We are going to check the worker... check if we are told to stop */
    if (child_stopping) {
        return 0;
    }

    /* We need threads to process that "blocking" logic */
#if MC_USE_THREADS
    if (mc_thread_pool) {
        apr_status_t res;
        apr_pool_t *targs_pool;
        watchdog_thread_args_t *targs_ptr;
        apr_pool_create(&targs_pool, server->process->pool);
        apr_pool_tag(targs_pool, "mc_watchdog_targs");
        targs_ptr = apr_palloc(targs_pool, sizeof(watchdog_thread_args_t));
        if (targs_ptr != NULL) {
            targs_ptr->server = server;
            targs_ptr->pool = targs_pool;
            targs_ptr->conf = conf;
            targs_ptr->ou = ou;
            targs_ptr->worker = worker;
            targs_ptr->now = now;
            targs_ptr->id = id;
            res = apr_thread_pool_push(mc_thread_pool, check_proxy_worker, (void *)targs_ptr,
                                       APR_THREAD_TASK_PRIORITY_NORMAL, NULL);
            if (res == APR_SUCCESS) {
                /* Early return. Task was scheduled! */
                return 0;
            }
            /* Log about failed scheduling and execute without threads below. */
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server,
                         "update_workers_lbstatus: thread push was NOT successful: %d", res);
        } else {
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, server,
                         "update_workers_lbstatus: Memory allocation for thread args failed");
        }
        /* Thread alloc or push failed, so run it without threads!
         * That means the execution continues after the endif below!
         */
    }
#endif
    targs.server = server;
    targs.pool = pool;
    targs.conf = conf;
    targs.ou = ou;
    targs.worker = worker;
    targs.now = now;
    targs.id = id;

    check_proxy_worker(NULL, (void *)&targs);

    /* We have checked the worker... check if we were told to stop */
    if (child_stopping) {
        return 0;
    }

    return 1;
}

/* Update the lbfactor of each node if needed. */
static void update_workers_lbstatus(proxy_server_conf *conf, apr_pool_t *pool, server_rec *server)
{
    int *id, size, i;
    apr_time_t now;

    now = apr_time_now();

    /* read the ident of the nodes */
    size = node_storage->get_max_size_node();
    if (size == 0) {
        return;
    }
    id = apr_pcalloc(pool, sizeof(int) * size);
    size = node_storage->get_ids_used_node(id);

    /* update lbstatus if needed */
    for (i = 0; i < size; i++) {
        nodeinfo_t *ou;
        ap_assert(node_storage->lock_nodes() == APR_SUCCESS);
        /* Check if we are told to stop */
        if (child_stopping) {
            node_storage->unlock_nodes();
            return;
        }

        if (node_storage->read_node(id[i], &ou) != APR_SUCCESS) {
            node_storage->unlock_nodes();
            continue;
        }
        if (ou->mess.remove) {
            node_storage->unlock_nodes();
            continue;
        }
        if (ou->mess.updatetimelb < (now - lbstatus_recalc_time)) {
            /* The lbstatus needs to be updated */
            int elected, oldelected;
            apr_off_t read, oldread;
            proxy_worker_shared *stat;
            char *ptr = (char *)ou;

            ptr = ptr + ou->offset;
            stat = (proxy_worker_shared *)ptr;
            elected = stat->elected;
            read = stat->read;
            oldelected = ou->mess.oldelected;
            oldread = ou->mess.oldread;
            ou->mess.updatetimelb = now;
            ou->mess.oldelected = elected;
            ou->mess.oldread = read;
            if (stat->lbfactor > 0) {
                stat->lbstatus = ((elected - oldelected) * 1000) / stat->lbfactor;
            }
            if (read == oldread) {
                /* lbstatus_recalc_time without changes: test for broken nodes   */
                /* first get the worker, create a dummy request and do a ping    */
                /* worker->s->retries is the number of retries that have occured */
                /* it is set to zero when the back-end is back to normal.        */
                /* worker->s->retries is also set to zero is a connection is     */
                /* establish so we use read to check for changes                 */
                if (!internal_update_lbstatus(conf, pool, server, now, ou, id[i], stat)) {
                    node_storage->unlock_nodes();
                    return;
                }
            } else {
                ou->mess.num_failure_idle = 0;
            }
        }
        node_storage->unlock_nodes();
    }
}

/* Remove the sessionids that have timeout */
static void remove_timeout_sessionid(const proxy_server_conf *conf, apr_pool_t *pool, const server_rec *server)
{
    int *id, size, i;
    apr_time_t now;
    (void)conf;
    (void)server;

    now = apr_time_sec(apr_time_now());

    /* read the ident of the sessionid */
    size = sessionid_storage->get_max_size_sessionid();
    if (size == 0) {
        return;
    }

    id = apr_pcalloc(pool, sizeof(int) * size);
    size = sessionid_storage->get_ids_used_sessionid(id);

    /* update lbstatus if needed */
    for (i = 0; i < size; i++) {
        sessionidinfo_t *ou;
        if (sessionid_storage->read_sessionid(id[i], &ou) != APR_SUCCESS) {
            continue;
        }
        if (ou->updatetime < (now - TIMESESSIONID)) {
            /* Remove it */
            sessionid_storage->remove_sessionid(ou);
        }
    }
}

/* Remove the domain that have timeout */
static void remove_timeout_domain(apr_pool_t *pool)
{
    int *id, size, i;
    apr_time_t now;

    now = apr_time_sec(apr_time_now());

    /* read the ident of the domain */
    size = domain_storage->get_max_size_domain();
    if (size == 0) {
        return;
    }
    id = apr_pcalloc(pool, sizeof(int) * size);
    size = domain_storage->get_ids_used_domain(id);

    for (i = 0; i < size; i++) {
        domaininfo_t *ou;
        if (domain_storage->read_domain(id[i], &ou) != APR_SUCCESS) {
            continue;
        }
        if (ou->updatetime < (now - TIMEDOMAIN)) {
            /* Remove it */
            domain_storage->remove_domain(ou);
        }
    }
}

/* Check that the worker corresponds to a node that belongs to the same domain according to the JVMRoute. */
static int isnode_domain_ok(const request_rec *r, const nodeinfo_t *node, const char *domain)
{
    (void)r;
#if HAVE_CLUSTER_EX_DEBUG
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "isnode_domain_ok: domain %s:%s", domain, node->mess.Domain);
#endif
    if (domain == NULL) {
        return 1; /* OK no domain in the corresponding to the SESSIONID */
    }
    if (strcmp(node->mess.Domain, domain) == 0) {
        return 1; /* OK */
    }
    return 0;
}

static proxy_worker *internal_process_worker(proxy_worker *worker, int checking_standby, int checked_domain,
                                             const char *domain, const node_context *best,
                                             const node_context **mynodecontext, const request_rec *r,
                                             proxy_worker **mycandidate, nodeinfo_t **node1, const char *balancer_name)
{
    nodeinfo_t *node;
    const node_context *best1;
    proxy_cluster_helper *helper = (proxy_cluster_helper *)worker->context;

    if (!worker->s || !worker->context) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server,
                     "find_session_route: byrequests balancer %s skipping BAD worker %s", balancer_name,
                     worker->s ? worker->s->name_ex : "NULL");
        return NULL;
    }
    if (helper->index == -1) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server,
                     "find_session_route: byrequests balancer skipping REMOVED worker");
        return NULL; /* marked removed */
    }
    if (helper->index != worker->s->index) {
        /* something is very bad */
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server,
                     "find_session_route: byrequests balancer skipping BAD worker");
        return NULL; /* probably used by different worker */
    }

    /* standby logic
     * lbfactor: -1 broken node.
     *            0 standby.
     *           >0 factor to use.
     */
    if (worker->s->lbfactor < 0 || (worker->s->lbfactor == 0 && !checking_standby)) {
        return NULL;
    }

    /* If the worker is in error state the STATUS logic will retry it */
    if (!PROXY_WORKER_IS_USABLE(worker)) {
        return NULL;
    }

    /* Take into calculation only the workers that are
     * not in error state or not disabled.
     * and that can map the context.
     */
    if (best == NULL) {
        apr_table_setn(r->subprocess_env, "BALANCER_CONTEXT_ID", "");
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, "find_session_route: byrequests balancer FAILED");
        return NULL;
    }
    best1 = best;

    while (best1->node != -1) {
        if (best1->node == worker->s->index) {
            break;
        }
        best1++;
    }
    if (best1->node == -1) {
        return NULL; /* not found */
    }

    /* Let's do the table read only now after we know the worker is usable and matches */
    if (read_node_worker(worker->s->index, &node, worker) != APR_SUCCESS) {
        return NULL; /* Can't read node */
    }
    if (worker->s != (proxy_worker_shared *)((char *)node + node->offset)) {
        return NULL; /* wrong shared memory address */
    }

    /* First try only nodes in the domain */
    if (!checked_domain && !isnode_domain_ok(r, node, domain)) {
        return NULL;
    }

    if (worker->s->lbfactor == 0 && checking_standby) {
        *mycandidate = worker;
        *mynodecontext = best1;
        return worker; /* Done */
    } else if (!(*mycandidate)) {
        *mycandidate = worker;
        *mynodecontext = best1;
        *node1 = node;
    } else {
        int lbstatus, lbstatus1;

        /* Let's avoid repeat reads of mycandidate through our loop iterations */
        if (!(*node1) && node_storage->read_node((*mycandidate)->s->index, node1) != APR_SUCCESS) {
            *mycandidate = NULL;
            return worker;
        }

        lbstatus1 = (((*mycandidate)->s->elected - (*node1)->mess.oldelected) * 1000) / (*mycandidate)->s->lbfactor +
                    (*mycandidate)->s->lbstatus;
        lbstatus = ((worker->s->elected - node->mess.oldelected) * 1000) / worker->s->lbfactor + worker->s->lbstatus;
        if (lbstatus1 > lbstatus) {
            *mycandidate = worker;
            *mynodecontext = best1;
        }
    }

    return worker;
}

/*
 * The ModClusterService from the cluster fills the lbfactor values.
 * Our logic is a bit different the mod_balancer one. We check the
 * context and host to prevent to route to application beeing redeploy or
 * stopped in one node but not in others.
 * We also try the domain.
 */
static proxy_worker *internal_find_best_byrequests(const proxy_balancer *balancer, const proxy_server_conf *conf,
                                                   request_rec *r, const char *domain, int failoverdomain,
                                                   const proxy_vhost_table *vhost_table,
                                                   const proxy_context_table *context_table,
                                                   proxy_node_table *node_table)
{
    int i, hash = 0;
    proxy_worker *mycandidate = NULL;
    const node_context *mynodecontext = NULL;
    node_context *best = NULL;
    int checking_standby = 0;
    int checked_standby = 0;
    int checked_domain = 1;
    /* Create a separate array of available workers, to be sorted later */
    proxy_worker **workers = NULL;
    int workers_length = 0;
    const char *session_id_with_route;
    const char *route;
    char *tokenizer;
    const char *session_id;

    workers = apr_pcalloc(r->pool, sizeof(proxy_worker *) * balancer->workers->nelts);
#if HAVE_CLUSTER_EX_DEBUG
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                 "internal_find_best_byrequests: Entering byrequests for CLUSTER (%s) failoverdomain:%d",
                 balancer->s->name, failoverdomain);
#endif

    /* create workers for new nodes */
    if (!cache_share_for) {
        ap_assert(node_storage->lock_nodes() == APR_SUCCESS);
        update_workers_node(conf, r->pool, r->server, 1, node_table);
        check_workers(conf, r->server);
        node_storage->unlock_nodes();
    }

    /* do this once now to avoid repeating find_node_context_host through loop iterations */
    route = apr_table_get(r->notes, "session-route");
    best = find_node_context_host(r, balancer, route, use_alias, vhost_table, context_table, node_table);
    if (best == NULL) {
        /* No context to serve the request we can't do much */
        apr_table_setn(r->notes, "no-context-error", "1");
        return NULL;
    }

    /* First try to see if we have available candidate */
    if (domain && strlen(domain) > 0) {
        checked_domain = 0;
    }
    while (!checked_standby) {
        char *ptr = balancer->workers->elts;
        int sizew = balancer->workers->elt_size;
        for (i = 0; i < balancer->workers->nelts; i++, ptr = ptr + sizew) {
            nodeinfo_t *node1 = NULL;
            proxy_worker *worker =
                internal_process_worker(*(proxy_worker **)ptr, checking_standby, checked_domain, domain, best,
                                        &mynodecontext, r, &mycandidate, &node1, balancer->s->name);
            if (worker == NULL && best == NULL) {
                return NULL;
            }
            if (worker != NULL) {
                workers[workers_length++] = worker;
                if (worker->s->lbfactor == 0 && checking_standby) {
                    break;
                }
            }
        }
        session_id_with_route = apr_table_get(r->notes, "session-id");
        session_id = session_id_with_route ? apr_strtok(strdup(session_id_with_route), ".", &tokenizer) : NULL;
        /* Determine deterministic route, if session is associated with a route, but that route wasn't used */
        if (deterministic_failover && session_id && strchr((char *)session_id_with_route, '.') && workers_length > 0) {
            /* Deterministic selection of target route */
            if (workers_length > 1) {
                qsort(workers, workers_length, sizeof(*workers), &proxy_worker_cmp);
            }
            /* Compute consistent int from session id */
            for (i = 0; session_id[i] != 0; i++) {
                hash += session_id[i];
            }
            mycandidate = workers[hash % workers_length];
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                         "find_best_worker: Using deterministic failover target: %s", mycandidate->s->route);
        }
        if (mycandidate) {
            break;
        }
        if (failoverdomain) {
            break; /* We only failover in the domain */
        }
        if (checked_domain) {
            checked_standby = checking_standby++;
        }
        checked_domain++;
    }

    if (mycandidate) {
        /* Failover in domain */
        if (!checked_domain) {
            apr_table_setn(r->notes, "session-domain-ok", "1");
        }
        mycandidate->s->elected++;
        apr_table_setn(r->subprocess_env, "BALANCER_CONTEXT_ID", apr_psprintf(r->pool, "%d", mynodecontext->context));
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "find_best_worker: byrequests balancer DONE (%s)",
                     mycandidate->s->name_ex);
    } else {
        apr_table_setn(r->subprocess_env, "BALANCER_CONTEXT_ID", "");
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "find_best_worker: byrequests balancer FAILED");
    }
    return mycandidate;
}

/*
 * Check that we could connect to the node and create corresponding balancers and workers.
 * id   : worker id
 * load : load factor from the cluster manager.
 * load > 0  : a load factor.
 * load = 0  : standby worker.
 * load = -1 : errored worker.
 * load = -2 : just do a cping/cpong.
 */
static int proxy_node_isup(request_rec *r, int id, int load)
{
    proxy_worker *worker = NULL;
    server_rec *s = main_server;
    proxy_server_conf *conf = NULL;
    nodeinfo_t *node;
    proxy_worker_shared *stat;
    char *ptr;

    if (node_storage->read_node(id, &node) != APR_SUCCESS) {
        return 500;
    }
    if (node->mess.remove) {
        return 500;
    }

    /* Calculate the address of our shared memory that corresponds to the stat info of the worker */
    ptr = (char *)node;
    stat = (proxy_worker_shared *)(ptr + node->offset);

    /* create the balancers and workers (that could be the first time) */
    ap_assert(node_storage->lock_nodes() == APR_SUCCESS);
    add_balancers_workers(node, ptr, r->pool);
    node_storage->unlock_nodes();

    /* search for the worker in the VirtualHosts */
    while (s) {
        void *sconf = s->module_config;
        conf = (proxy_server_conf *)ap_get_module_config(sconf, &proxy_module);

        worker = get_worker_from_id_stat(conf, id, stat, node);
        if (worker != NULL) {
            break;
        }
        s = s->next;
    }
    if (worker == NULL) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                     "proxy_cluster_isup: Can't find worker for %d. Check balancer names.", id);
        return 500;
    }

    /* Try a  ping/pong to check the node */
    if (load >= 0 || load == -2) {
        /* Only try usuable nodes */
        if (proxyhctemplate != NULL) {
            /* Don't ping the hcheck is doing it for us */
            /* TODO : worker->s->error_time = 0; Force retry now do we need something? */
            if (worker->s->status & PROXY_WORKER_NOT_USABLE_BITMAP) {
                ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                             "proxy_cluster_isup: health check says PROXY_WORKER_IN_ERROR");
                return 500;
            }

            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "proxy_cluster_isup: health check says OK");
        } else {
            char sport[7];
            char *url;
            apr_snprintf(sport, sizeof(sport), "%d", worker->s->port);
            if (strchr(worker->s->hostname, ':') != NULL) {
                url = apr_pstrcat(r->pool, worker->s->scheme, "://[", worker->s->hostname, "]:", sport, "/", NULL);
            } else {
                url = apr_pstrcat(r->pool, worker->s->scheme, "://", worker->s->hostname, ":", sport, "/", NULL);
            }
            worker->s->error_time = 0; /* Force retry now */
            if (proxy_cluster_try_pingpong(r, worker, url, conf, node->mess.ping, node->mess.timeout) != APR_SUCCESS) {
                worker->s->status |= PROXY_WORKER_IN_ERROR;
                ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "proxy_cluster_isup: pingpong %s failed", url);
                return 500;
            }
        }
    }
    if (load == -2) {
        return 0;
    } else if (load == -1) {
        worker->s->status |= PROXY_WORKER_IN_ERROR;
        worker->s->lbfactor = -1;
    } else if (load == 0) {
        worker->s->status |= PROXY_WORKER_HOT_STANDBY;
        worker->s->lbfactor = 0;
    } else {
        worker->s->status &= ~PROXY_WORKER_IN_ERROR;
        worker->s->status &= ~PROXY_WORKER_STOPPED;
        worker->s->status &= ~PROXY_WORKER_DISABLED;
        worker->s->status &= ~PROXY_WORKER_HOT_STANDBY;
        worker->s->lbfactor = load;
    }
    return 0;
}

static int proxy_host_isup(request_rec *r, const char *scheme, const char *host, const char *port)
{
    apr_socket_t *sock;
    apr_sockaddr_t *to;
    apr_status_t rv;
    int nport = atoi(port);

    rv = apr_socket_create(&sock, APR_INET, SOCK_STREAM, 0, r->pool);
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_WARNING, 0, r->server, "proxy_host_isup: pingpong (apr_socket_create) failed");
        return 500;
    }
    rv = apr_sockaddr_info_get(&to, host, APR_INET, nport, 0, r->pool);
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_WARNING, 0, r->server,
                     "proxy_host_isup: pingpong (apr_sockaddr_info_get(%s, %d)) failed", host, nport);
        return 500;
    }

    rv = apr_socket_connect(sock, to);
    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "proxy_host_isup: pingpong (apr_socket_connect) failed");
        return 500;
    }

    /* XXX: For the moment we support only AJP */
    if (strcasecmp(scheme, "AJP") == 0) {
        rv = ajp_handle_cping_cpong(sock, r, apr_time_from_sec(10));
        if (rv != APR_SUCCESS) {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "proxy_host_isup: cping_cpong failed");
            return 500;
        }
    } else {
        ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, r->server, "proxy_host_isup: %s no yet supported", scheme);
    }

    apr_socket_close(sock);
    return 0;
}

static proxy_worker *searchworker(request_rec *r, const char *bal, const char *ptr, int *id,
                                  const proxy_server_conf **the_conf)
{
    /* search for the worker in the VirtualHosts */
    proxy_worker *worker = NULL;
    server_rec *s = main_server;
    while (s) {
        void *sconf = s->module_config;
        proxy_balancer *balancer;
        proxy_server_conf *conf = (proxy_server_conf *)ap_get_module_config(sconf, &proxy_module);

        /* XXX: bad location just testing */
        check_workers(conf, s);
        check_workers(conf, s);
        check_workers(conf, s);

        balancer = ap_proxy_get_balancer(r->pool, conf, bal, 0);
        if (balancer != NULL) {
            worker = ap_proxy_get_worker(r->pool, balancer, conf, ptr);
            if (worker != NULL) {
                proxy_cluster_helper *helper;
                if (worker->s->index != -1) {
                    *id = worker->s->index;
                    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                                 "searchworker: %s worker->s->index: %d the_conf %ld", ptr, *id, (uintptr_t)conf);
                    *the_conf = conf;
                    return worker; /* Done current index */
                }
                helper = (proxy_cluster_helper *)worker->context;
                if (helper && helper->index != -1) {
                    *id = helper->index;
                    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                                 "searchworker: %s helper->index %d the_conf %ld", ptr, *id, (uintptr_t)conf);
                    *the_conf = conf;
                    return worker; /* Done previous index */
                }
                if (helper && helper->shared) {
                    *id = helper->shared->index;
                    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                                 "searchworker: %s helper->shared->index %d the_conf %ld", ptr, *id, (uintptr_t)conf);
                    *the_conf = conf;
                    return worker; /* our index was saved when we remove... */
                }
                ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "searchworker: %s FAILED", ptr);
                return NULL;
            }
        }

        s = s->next;
    }
    return NULL;
}

static proxy_worker *proxy_node_getid(request_rec *r, const char *balancername, const char *scheme, const char *host,
                                      const char *port, int *id, const proxy_server_conf **the_conf)
{
    proxy_worker *worker = NULL;
    char *url, *bal;
    /* apr_thread_mutex_lock(lock); */
    url = apr_pstrcat(r->pool, scheme, "://", host, ":", port, NULL);
    if (normalize_workername(r->pool, url) == NULL) {
        *id = -1;
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, "proxy_node_getid: normalize_workername returns NULL");
        return NULL; /* Should not happend */
    }

    bal = apr_pstrcat(r->pool, "balancer://", balancername, NULL);
    /* search for the worker in the VirtualHosts */
    worker = searchworker(r, bal, url, id, the_conf);
    if (worker == NULL) {
        *id = -1;
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "proxy_node_getid: searchworker returns NULL");
        return NULL;
    }
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "proxy_node_getid: the_conf %ld", (uintptr_t)*the_conf);
    return worker;
}

static int proxy_node_get_free_id(request_rec *r, int node_table_size)
{
    int i, used_count;
    int *ids = apr_pcalloc(r->pool, sizeof(int) * node_table_size);
    int *used_ids = apr_pcalloc(r->pool, sizeof(int) * node_table_size);
    server_rec *s = main_server;

    used_count = node_storage->get_ids_used_node(used_ids);
    for (i = 0; i < used_count; i++) {
        ids[used_ids[i]] = 1;
    }

    /* build the list of id used by workers */
    while (s) {
        void *sconf = s->module_config;
        proxy_balancer *balancer;
        proxy_server_conf *conf = (proxy_server_conf *)ap_get_module_config(sconf, &proxy_module);
        balancer = (proxy_balancer *)conf->balancers->elts;
        for (i = 0; i < conf->balancers->nelts; i++, balancer++) {
            int j;
            proxy_worker **workers;
            workers = (proxy_worker **)balancer->workers->elts;
            for (j = 0; j < balancer->workers->nelts; j++, workers++) {
                volatile proxy_worker *worker = *workers;
                proxy_cluster_helper *helper;
                if (worker->s->index >= node_table_size) {
                    ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, r->server,
                                 "proxy_node_get_free_id: skipping worker index (%d) higher than node_table_size (%d)",
                                 worker->s->index, node_table_size);
                    continue;
                }
                if (worker->s->index != -1) {
                    ids[worker->s->index] = 1;
                }
                helper = (proxy_cluster_helper *)worker->context;
                if (helper && helper->index != -1) {
                    ids[worker->s->index] = 1;
                }
            }
        }
        s = s->next;
    }

    for (i = 0; i < node_table_size; i++) {
        if (ids[i] == 0) {
            return i;
        }
    }
    return -1; /* All workers are full */
}

static void init_proxy_worker(server_rec *server, nodeinfo_t *node, proxy_worker *worker,
                              const proxy_server_conf *the_conf)
{
    worker->s->status = PROXY_WORKER_INITIALIZED;
    strncpy(worker->s->route, node->mess.JVMRoute, sizeof(worker->s->route));
    worker->s->route[sizeof(worker->s->route) - 1] = '\0';
    strncpy(worker->s->upgrade, node->mess.Upgrade, sizeof(worker->s->upgrade));
    worker->s->upgrade[sizeof(worker->s->upgrade) - 1] = '\0';
    strncpy(worker->s->secret, node->mess.AJPSecret, sizeof(worker->s->secret));
    worker->s->secret[sizeof(worker->s->secret) - 1] = '\0';
    if (node->mess.ResponseFieldSize > 0) {
        worker->s->response_field_size = node->mess.ResponseFieldSize;
        worker->s->response_field_size_set = 1;
    } else {
        worker->s->response_field_size_set = 0;
    }
    /* XXX: We need that information from TC */
    worker->s->redirect[0] = '\0';
    worker->s->lbstatus = 0;
    worker->s->lbfactor = -1; /* prevent using the node using status message */
    worker->s->index = node->mess.id;

    /* add health check */
    worker->s->updated = apr_time_now();
    if (proxyhctemplate != NULL) {
        add_hcheck(server, the_conf, worker);
    }
}

static void reenable_proxy_worker(server_rec *server, nodeinfo_t *node, proxy_worker *worker, nodeinfo_t *nodeinfo,
                                  const proxy_server_conf *the_conf)
{
    char *ptr;
    proxy_cluster_helper *helper;
    helper = (proxy_cluster_helper *)worker->context;
    helper->count_active = 0;
    ap_log_error(APLOG_MARK, APLOG_ERR, 0, server, "reenable_proxy_worker = CRAP (%d %d)!!!", helper->index,
                 node->mess.id);
    /* XXX: BAD IDEA!!! helper->shared = worker->s; */
    helper->isinnodes = 1;
    /* XXX: BAD IDEA!! helper->index = node->mess.id; */
    ptr = (char *)node;
    worker->s = (proxy_worker_shared *)(ptr + node->offset);
    /* merge the "new" node information */
    init_proxy_worker(server, nodeinfo, worker, the_conf);
}

/*
 * For the provider
 */
/* clang-format off */
static const struct balancer_method balancerhandler = {
    proxy_node_isup,
    proxy_host_isup,
    proxy_node_getid,
    reenable_proxy_worker,
    proxy_node_get_free_id
};
/* clang-format on */

static int node_has_workers(const server_rec *server, const proxy_server_conf *conf, int id)
{
    int i, j;
    proxy_balancer *balancer;
    balancer = (proxy_balancer *)conf->balancers->elts;
    for (i = 0; i < conf->balancers->nelts; i++, balancer++) {
        proxy_worker **workers;
        workers = (proxy_worker **)balancer->workers->elts;
        for (j = 0; j < balancer->workers->nelts; j++, workers++) {
            proxy_worker *worker = *workers;
            proxy_cluster_helper *helper = (proxy_cluster_helper *)worker->context;
            ap_assert(helper); /* we are in trouble ... */
            if (helper->index == id) {
                ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server,
                             "remove_removed_node: %d for REMOVED node_has_workers %d (%s)", id, getpid(),
                             worker->s->hostname_ex);
                return 1;
            }
        }
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server,
                 "remove_removed_node: %d for REMOVED node_has_workers NO WORKERS %d", id, getpid());
    return 0;
}

/* Remove node that have beeen marked removed for more than 10 seconds. */
static void remove_removed_node(apr_pool_t *pool, const proxy_server_conf *conf, const server_rec *server)
{
    int *id, size, i;
    apr_time_t now = apr_time_now();
    /* read the ident of the nodes */
    size = node_storage->get_max_size_node();
    (void)server;

    if (size == 0) {
        return;
    }
    id = apr_pcalloc(pool, sizeof(int) * size);
    size = node_storage->get_ids_used_node(id);
    for (i = 0; i < size; i++) {
        nodeinfo_t *ou;
        if (node_storage->read_node(id[i], &ou) != APR_SUCCESS) {
            continue;
        }
        if (strcmp(ou->mess.JVMRoute, "REMOVED") == 0 && (now - ou->updatetime) >= wait_for_remove) {
            if (node_has_workers(server, conf, ou->mess.id)) {
                ou->updatetime = now;
                ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server, "remove_removed_node: %d for REMOVED wait %d",
                             ou->mess.id, getpid());
            } else {
                ou->mess.num_remove_check++;
                ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server, "remove_removed_node: %d %s for REMOVED done %d",
                             ou->mess.id, ou->mess.Port, getpid());
                if (ou->mess.num_remove_check > 10) {
                    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server,
                                 "remove_removed_node: %d %s for REMOVED done (DONE) %d", ou->mess.id, ou->mess.Port,
                                 getpid());
                    node_storage->remove_node(ou->mess.id);
                }
            }
            continue;
        }
        if (ou->mess.remove && (now - ou->updatetime) >= wait_for_remove &&
            (now - ou->mess.lastcleantry) >= wait_for_remove) {
            /* if it has a domain store it in the domain */
            if (ou->mess.Domain[0] != '\0') {
                domaininfo_t dom;
                strncpy(dom.JVMRoute, ou->mess.JVMRoute, sizeof(dom.JVMRoute));
                dom.JVMRoute[sizeof(dom.JVMRoute) - 1] = '\0';
                strncpy(dom.balancer, ou->mess.balancer, sizeof(dom.balancer));
                dom.balancer[sizeof(dom.balancer) - 1] = '\0';
                strncpy(dom.domain, ou->mess.Domain, sizeof(dom.domain));
                dom.domain[sizeof(dom.domain) - 1] = '\0';
                if (domain_storage->insert_update_domain(&dom) != APR_SUCCESS) {
                    remove_timeout_domain(pool);
                    domain_storage->insert_update_domain(&dom);
                }
            }
            /* remove the node from the shared memory */
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, server, "remove_removed_node: %d %s %s %d", ou->mess.id,
                         ou->mess.JVMRoute, ou->mess.Port, getpid());
            node_storage->remove_host_context(ou->mess.id, pool);

            strcpy(ou->mess.JVMRoute, "REMOVED");
            ou->mess.Domain[0] = '\0';

            /* prevent real remove until processes don't have the node in workers */
            ou->updatetime = now;
            ou->mess.num_remove_check = 0;
        }
    }
}

/* Remote workers corresponding to removed nodes
 * NOTE: the calling routine should lock before calling us.
 */
static void remove_workers_nodes(proxy_server_conf *conf, apr_pool_t *pool, server_rec *server)
{
    int *id, size, i;

    /* read the ident of the nodes */
    size = node_storage->get_max_size_node();
    if (size == 0) {
        return;
    }
    id = apr_pcalloc(pool, sizeof(int) * size);
    size = node_storage->get_ids_used_node(id);
    for (i = 0; i < size; i++) {
        nodeinfo_t *ou;
        if (node_storage->read_node(id[i], &ou) != APR_SUCCESS) {
            continue;
        }
        if (ou->mess.remove) {
            remove_workers_node(ou, conf, pool, server);
        }
    }
}

/* Called by mc_watchdog_callback every n seconds */
/* it is called for the main server, we need to loop on all servers */
static void proxy_cluster_watchdog_func(server_rec *s, apr_pool_t *pool)
{
    void *sconf = s->module_config;
    proxy_server_conf *conf = (proxy_server_conf *)ap_get_module_config(sconf, &proxy_module);
    unsigned last;
    server_rec *smain = s;
    proxy_node_table *node_table = NULL;

    if (!conf) {
        return;
    }

    /* check if we need to update. */
    last = node_storage->worker_nodes_need_update(s, pool);

    /* if we need to update, let's check the cache */
    if (last) {
        if (cache_share_for) {
            /* time to refresh or create */
            apr_time_t now = apr_time_now();
            if (now >= last_cached + cache_share_for) {
                ap_assert(node_storage->lock_nodes() == APR_SUCCESS);
                last_cached = now;
                if (cached_pool) {
                    /* we need to update the cache */
                    update_vhost_table_cached(cached_vhost_table, host_storage);
                    update_context_table_cached(cached_context_table, context_storage);
                    update_balancer_table_cached(cached_balancer_table, balancer_storage);
                    update_node_table_cached(cached_node_table, node_storage);
                } else {
                    apr_pool_create(&cached_pool, conf->pool);
                    cached_vhost_table = read_vhost_table(cached_pool, host_storage, 1);
                    cached_context_table = read_context_table(cached_pool, context_storage, 1);
                    cached_balancer_table = read_balancer_table(cached_pool, balancer_storage, 1);
                    cached_node_table = read_node_table(cached_pool, node_storage, 1);
                    ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
                                 "cached_pool: should have been created in proxy_cluster_child_init!");
                }
                node_storage->unlock_nodes();
            }
            node_table = cached_node_table;
        } else {
            ap_assert(node_storage->lock_nodes() == APR_SUCCESS);
            if (child_stopping) {
                node_storage->unlock_nodes();
                return;
            }
            node_table = read_node_table(pool, node_storage, 0);
            node_storage->unlock_nodes();
        }
    }

    while (s) {
        sconf = s->module_config;
        conf = (proxy_server_conf *)ap_get_module_config(sconf, &proxy_module);
        /* Create new workers if the shared memory has changed */
        ap_assert(node_storage->lock_nodes() == APR_SUCCESS);
        if (child_stopping) {
            node_storage->unlock_nodes();
            return;
        }
        if (last) {
            /* Add the workers and balancers */
            update_workers_node(conf, pool, s, 0, node_table);
            /* Check for workers that point to "cleaned" shared memory */
            check_workers(conf, s);
        }
        /* removed nodes: check for workers */
        remove_workers_nodes(conf, pool, s);
        node_storage->unlock_nodes();
        /* Calculate the lbstatus for each node */
        update_workers_lbstatus(conf, pool, s);
        /* Free sessionid slots */
        if (sessionid_storage) {
            remove_timeout_sessionid(conf, pool, s);
        }
        /* cleanup removed node in shared memory */
        ap_assert(node_storage->lock_nodes() == APR_SUCCESS);
        if (child_stopping) {
            node_storage->unlock_nodes();
            return;
        }
        remove_removed_node(pool, conf, s);
        node_storage->unlock_nodes();
        s = s->next;
    }
    if (last) {
        node_storage->worker_nodes_are_updated(smain, last);
    }
}


static apr_status_t mc_watchdog_callback(int state, void *data, apr_pool_t *pool)
{
    apr_status_t res = APR_SUCCESS;
    server_rec *s = (server_rec *)data;
    switch (state) {
    case AP_WATCHDOG_STATE_STARTING:
#if MC_USE_THREADS
        if (mc_thread_pool_size && mc_thread_pool == NULL) {
            res = apr_thread_pool_create(&mc_thread_pool, mc_thread_pool_size, mc_thread_pool_size, s->process->pool);
            if (res != APR_SUCCESS) {
                ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, s,
                             "mc_watchdog_callback: apr_thread_pool_create failed, threads will not be used");
                mc_thread_pool = NULL;
            }
        } else {
            mc_thread_pool = NULL;
            ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, s, "mc_watchdog_callback: Threads are not used");
        }
#endif
        break;

    case AP_WATCHDOG_STATE_RUNNING:
        if (s) {
            apr_time_t wait = cache_share_for;
            /* It is called for every server defined in httpd */
            proxy_cluster_watchdog_func(s, pool);
            /* set the next call back to cache_share_for or 1 if zero */
            if (!wait) {
                wait = apr_time_from_sec(1);
            }
            mc_watchdog_set_interval(watchdog, wait, s, mc_watchdog_callback);
        }
        break;

    case AP_WATCHDOG_STATE_STOPPING:
#if MC_USE_THREADS
        res = apr_thread_pool_destroy(mc_thread_pool);
        if (res != APR_SUCCESS) {
            ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, s, "mc_watchdog_callback: apr_thread_pool_destroy failed");
        }
        mc_thread_pool = NULL;
#endif
        break;
    }
    return res;
}

static void proxy_cluster_child_stopping(apr_pool_t *pool, int graceful)
{
    (void)pool;
    (void)graceful;
    child_stopping = 1;
}


/*
 * Create a thread per process to make maintenance task,
 * and the mutex of the node creation.
 */
static void proxy_cluster_child_init(apr_pool_t *p, server_rec *s)
{
    void *sconf = s->module_config;
    proxy_server_conf *conf = (proxy_server_conf *)ap_get_module_config(sconf, &proxy_module);
    main_server = s;

    (void)p; /* unused argument */
    ap_assert(node_storage->lock_nodes() == APR_SUCCESS);
    if (conf && node_storage && node_storage->get_max_size_node()) {
        /* fill the cache and create pool */
        apr_pool_t *pool;
        proxy_node_table *node_table = NULL;
        apr_pool_create(&pool, conf->pool);
        if (cache_share_for) {
            apr_pool_create(&cached_pool, conf->pool);
            cached_vhost_table = read_vhost_table(cached_pool, host_storage, 1);
            cached_context_table = read_context_table(cached_pool, context_storage, 1);
            cached_balancer_table = read_balancer_table(cached_pool, balancer_storage, 1);
            cached_node_table = read_node_table(cached_pool, node_storage, 1);
            node_table = cached_node_table;
        } else {
            node_table = read_node_table(pool, node_storage, 0);
        }

        while (s) {
            sconf = s->module_config;
            conf = (proxy_server_conf *)ap_get_module_config(sconf, &proxy_module);

            update_workers_node(conf, pool, s, 0, node_table);
            check_workers(conf, s);

            s = s->next;
        }
        apr_pool_destroy(pool);
    }
    node_storage->unlock_nodes();
}

static int proxy_cluster_post_config(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *main_s)
{
    APR_OPTIONAL_FN_TYPE(ap_watchdog_get_instance) *mc_watchdog_get_instance;
    APR_OPTIONAL_FN_TYPE(ap_watchdog_register_callback) *mc_watchdog_register_callback;
    server_rec *s = main_s;
    void *sconf = s->module_config;
    proxy_server_conf *conf = (proxy_server_conf *)ap_get_module_config(sconf, &proxy_module);
    int sizew = conf->workers->elt_size;
    int sizeb = conf->balancers->elt_size;
    int idx;
    int has_static_workers = 0;
    (void)plog;
    (void)ptemp;

    if (sizew != sizeof(proxy_worker) || sizeb != sizeof(proxy_balancer)) {
        ap_version_t version;
        ap_get_server_revision(&version);

        ap_log_error(APLOG_MARK, APLOG_WARNING, 0, s,
                     "httpd version %d.%d.%d doesn't match version %d.%d.%d used by mod_proxy_cluster.c", version.major,
                     version.minor, version.patch, AP_SERVER_MAJORVERSION_NUMBER, AP_SERVER_MINORVERSION_NUMBER,
                     AP_SERVER_PATCHLEVEL_NUMBER);

        if (version.major < 2 || (version.major == 2 && version.minor < 4) ||
            (version.major == 2 && version.minor == 4 && version.patch < 53)) {
            ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s,
                         "Unsupported version (%d.%d.%d) of httpd detected, 2.4.53 or newer is required", version.major,
                         version.minor, version.patch);
            return !OK;
        }
    }
    if (SIZEOFSCORE <= sizeof(proxy_worker_shared)) {
        ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s,
                     "SIZEOFSCORE too small for mod_proxy shared stat structure %d <= %ld", SIZEOFSCORE,
                     sizeof(proxy_worker_shared));
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    /* Check that the mod_proxy_balancer.c is not loaded */
    if (ap_find_linked_module("mod_proxy_balancer.c") != NULL) {
        ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s,
                     "Module mod_proxy_balancer is loaded"
                     " it must be removed  in order for mod_proxy_cluster to function properly");
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    /* if we have a proxyhctemplate check for the template or failed */
    if (proxyhctemplate != NULL) {
        if (ap_find_linked_module("mod_proxy_hcheck.c") == NULL) {
            ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, "UseProxyHCTemplate requires mod_proxy_hcheck");
            return HTTP_INTERNAL_SERVER_ERROR;
        }
        if (set_worker_hc_param_f == NULL) {
            ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, "UseProxyHCTemplate can't be validated");
            return HTTP_INTERNAL_SERVER_ERROR;
        }
        /* get mod_proxy_hcheck.c validating the string... */
    }

    /* check for static workers and warn if that is the case */
    for (idx = 0; s; ++idx) {
        int i;
        proxy_balancer *balancer;
        void *sconf = s->module_config;
        conf = (proxy_server_conf *)ap_get_module_config(sconf, &proxy_module);
        balancer = (proxy_balancer *)conf->balancers->elts;
        for (i = 0; i < conf->balancers->nelts; i++, balancer++) {
            int j;
            proxy_worker **workers;
            workers = (proxy_worker **)balancer->workers->elts;
            for (j = 0; j < balancer->workers->nelts; j++, workers++) {
                proxy_worker *worker = *workers;
                ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, "%s BalancerMember are NOT supported %s", balancer->s->name,
                             worker->s->name);
                has_static_workers = 1;
            }
        }
        s = s->next;
    }
    s = main_s;
    if (has_static_workers) {
        ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, "Worker defined as BalancerMember are NOT supported");
        return !OK;
    }

    node_storage = ap_lookup_provider("manager", "shared", "0");
    if (node_storage == NULL) {
        ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, "proxy_cluster_post_config: Can't find mod_manager for nodes");
        return !OK;
    }
    host_storage = ap_lookup_provider("manager", "shared", "1");
    if (host_storage == NULL) {
        ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, "proxy_cluster_post_config: Can't find mod_manager for hosts");
        return !OK;
    }
    context_storage = ap_lookup_provider("manager", "shared", "2");
    if (context_storage == NULL) {
        ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, "proxy_cluster_post_config: Can't find mod_manager for contexts");
        return !OK;
    }
    balancer_storage = ap_lookup_provider("manager", "shared", "3");
    if (balancer_storage == NULL) {
        ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, "proxy_cluster_post_config: Can't find mod_manager for balancers");
        return !OK;
    }
    sessionid_storage = ap_lookup_provider("manager", "shared", "4");
    if (sessionid_storage == NULL) {
        ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, "proxy_cluster_post_config: Can't find mod_manager for sessionids");
        return !OK;
    }
    /* if Maxsessionid = 0 switch of the sessionid storing logic */
    if (!sessionid_storage->get_max_size_sessionid()) {
        sessionid_storage = NULL; /* don't use it */
    }

    domain_storage = ap_lookup_provider("manager", "shared", "5");
    if (domain_storage == NULL) {
        ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, "proxy_cluster_post_config: Can't find mod_manager for domains");
        return !OK;
    }
    if (!ap_proxy_retry_worker_fn) {
        ap_proxy_retry_worker_fn = APR_RETRIEVE_OPTIONAL_FN(ap_proxy_retry_worker);
        if (!ap_proxy_retry_worker_fn) {
            ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, "mod_proxy must be loaded for mod_proxy_cluster");
            return !OK;
        }
    }

    /* Add version information */
    ap_add_version_component(p, MOD_CLUSTER_EXPOSED_VERSION);

    if (ap_state_query(AP_SQ_MAIN_STATE) == AP_SQ_MS_CREATE_PRE_CONFIG) {
        ap_log_error(APLOG_MARK, APLOG_CRIT, 0, s, "No watchdog for %d", getpid());
        return OK;
    }
    ap_log_error(APLOG_MARK, APLOG_INFO, 0, s, "watchdog for %d", getpid());

    /* add our watchdog callback */
    mc_watchdog_get_instance = APR_RETRIEVE_OPTIONAL_FN(ap_watchdog_get_instance);
    mc_watchdog_register_callback = APR_RETRIEVE_OPTIONAL_FN(ap_watchdog_register_callback);
    mc_watchdog_set_interval = APR_RETRIEVE_OPTIONAL_FN(ap_watchdog_set_callback_interval);
    if (!mc_watchdog_get_instance || !mc_watchdog_register_callback || !mc_watchdog_set_interval) {
        ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(03262) "mod_watchdog is required");
        return !OK;
    }
    /* get watchdog to create a thread for each of our children */
    if (mc_watchdog_get_instance(&watchdog, LB_CLUSTER_WATHCHDOG_NAME, 0, 0, p)) {
        ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(03263) "Failed to create watchdog instance (%s)",
                     LB_CLUSTER_WATHCHDOG_NAME);
        return !OK;
    }

    if (mc_watchdog_register_callback(watchdog, AP_WD_TM_SLICE, s, mc_watchdog_callback)) {
        ap_log_error(APLOG_MARK, APLOG_EMERG, 0, s, APLOGNO(03264) "Failed to register watchdog callback (%s)",
                     LB_CLUSTER_WATHCHDOG_NAME);
        return !OK;
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, APLOGNO(03265) "watchdog callback registered (%s for %s)",
                 LB_CLUSTER_WATHCHDOG_NAME, s->server_hostname);
    return OK;
}

/* pre_config for the health check part */
static int proxy_cluster_pre_config(apr_pool_t *pconf, apr_pool_t *plog, apr_pool_t *ptemp)
{
    (void)pconf;
    (void)plog;
    (void)ptemp;

#if MC_USE_THREADS
    mc_thread_pool = NULL;
    mc_thread_pool_size = MC_THREADPOOL_SIZE;
#endif

    set_worker_hc_param_f = APR_RETRIEVE_OPTIONAL_FN(set_worker_hc_param);
    return OK;
}

/*
 * See if we could map the request.
 * first check is we have a balancer corresponding to the route.
 * then search the balancer correspond to the context and host.
 */
static int proxy_cluster_trans(request_rec *r)
{
    const char *balancer;
    void *sconf = r->server->module_config;
    proxy_server_conf *conf = (proxy_server_conf *)ap_get_module_config(sconf, &proxy_module);
    proxy_dir_conf *dconf = ap_get_module_config(r->per_dir_config, &proxy_module);

    proxy_vhost_table *vhost_table = NULL;
    proxy_context_table *context_table = NULL;
    proxy_balancer_table *balancer_table = NULL;
    proxy_node_table *node_table = NULL;

    if (cache_share_for) {
        vhost_table = cached_vhost_table;
        context_table = cached_context_table;
        balancer_table = cached_balancer_table;
        node_table = cached_node_table;
    } else {
        ap_assert(node_storage->lock_nodes() == APR_SUCCESS);
        vhost_table = read_vhost_table(r->pool, host_storage, 0);
        context_table = read_context_table(r->pool, context_storage, 0);
        balancer_table = read_balancer_table(r->pool, balancer_storage, 0);
        node_table = read_node_table(r->pool, node_storage, 0);
        /* make sure we have up to date workers and balancers in our process */
        update_workers_node(conf, r->pool, r->server, 1, node_table);
        check_workers(conf, r->server);
        node_storage->unlock_nodes();
    }

    apr_table_setn(r->notes, "vhost-table", (char *)vhost_table);
    apr_table_setn(r->notes, "context-table", (char *)context_table);
    apr_table_setn(r->notes, "balancer-table", (char *)balancer_table);
    apr_table_setn(r->notes, "node-table", (char *)node_table);

    ap_log_rerror(APLOG_MARK, APLOG_TRACE8, 0, r, "proxy_cluster_trans: for %d %s %s uri: %s args: %s unparsed_uri: %s",
                  r->proxyreq, r->filename, r->handler, r->uri, r->args, r->unparsed_uri);

    balancer = get_route_balancer(r, conf, vhost_table, context_table, balancer_table, node_table, use_alias);
    if (!balancer) {
        balancer = get_context_host_balancer(r, vhost_table, context_table, node_table, use_alias);
    }

    if (balancer) {
        int i;
        int rv = HTTP_CONTINUE;
        struct proxy_alias *ent;
        const char *use_uri;
        /* short way - this location is reverse proxied? */
        if (dconf->alias) {
            if ((dconf->alias->flags & PROXYPASS_MAP_ENCODED) == 0) {
                rv = ap_proxy_trans_match(r, dconf->alias, dconf);
                if (rv != HTTP_CONTINUE) {
                    ap_log_rerror(APLOG_MARK, APLOG_TRACE3, 0, r,
                                  "proxy_cluster_trans: ap_proxy_trans_match(dconf) matches or reject %s  to %s %d",
                                  r->uri, r->filename, rv);
                    return rv; /* Done */
                }
            }
        }

        /* long way - walk the list of aliases, find a match */
        for (i = 0; i < conf->aliases->nelts; i++) {
            ent = &((struct proxy_alias *)conf->aliases->elts)[i];
            if ((ent->flags & PROXYPASS_MAP_ENCODED) == 0) {
                rv = ap_proxy_trans_match(r, ent, dconf);
                if (rv != HTTP_CONTINUE) {
                    ap_log_rerror(APLOG_MARK, APLOG_TRACE3, 0, r,
                                  "proxy_cluster_trans: ap_proxy_trans_match(conf) matches or reject %s  to %s %d",
                                  r->uri, r->filename, rv);
                    return rv; /* Done */
                }
            }
        }

        /* Here the ProxyPass or ProxyPassMatch have been checked and have NOT returned ERRROR nor OK */
        ap_log_rerror(APLOG_MARK, APLOG_TRACE3, 0, r, "proxy_cluster_trans: no match for ap_proxy_trans_match on:%s",
                      r->uri);

        /* Use proxy-nocanon if needed */
        if (use_nocanon) {
            apr_table_setn(r->notes, "proxy-nocanon", "1");
            use_uri = r->unparsed_uri;
        } else {
            use_uri = r->uri;
        }
        if (strncmp(use_uri, "balancer://", 11)) {
            r->filename = apr_pstrcat(r->pool, "proxy:balancer://", balancer, use_uri, NULL);
        } else {
            r->filename = apr_pstrcat(r->pool, "proxy:", use_uri, NULL);
        }
        r->handler = "proxy-server";
        r->proxyreq = PROXYREQ_REVERSE;
        ap_log_rerror(APLOG_MARK, APLOG_TRACE3, 0, r, "proxy_cluster_trans: using %s uri: %s", balancer, r->filename);
        return OK; /* Mod_proxy will process it */
    }

    ap_log_rerror(APLOG_MARK, APLOG_TRACE8, 0, r, "proxy_cluster_trans: DECLINED %s uri: %s unparsed_uri: %s",
                  balancer ? balancer : "", r->filename, r->unparsed_uri);
    return DECLINED;
}

/*
 * canonise the url
 * XXX: needs more see the unparsed_uri in proxy_cluster_trans()
 */
static int proxy_cluster_canon(request_rec *r, char *url)
{
    char *host, *path;
    char *search = NULL;
    const char *err;
    apr_port_t port = 0;
    const char *route;

    if (strncasecmp(url, "balancer:", 9) != 0) {
        return DECLINED;
    }
    url += 9;

#if HAVE_CLUSTER_EX_DEBUG
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "proxy_cluster_canon url: %s", url);
#endif

    /* do syntatic check.
     * We break the URL into host, port, path, search
     */
    err = ap_proxy_canon_netloc(r->pool, &url, NULL, NULL, &host, &port);
    if (err) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "proxy_cluster_canon: error parsing URL %s: %s", url, err);
        return HTTP_BAD_REQUEST;
    }
    /*
     * now parse path/search args, according to rfc1738:
     * process the path. With proxy-noncanon set (by
     * mod_proxy) we use the raw, unparsed uri
     */
    if (apr_table_get(r->notes, "proxy-nocanon")) {
        path = url; /* this is the raw path */
    } else {
        path = ap_proxy_canonenc(r->pool, url, strlen(url), enc_path, 0, r->proxyreq);
        search = r->args;
    }
    if (path == NULL) {
        return HTTP_BAD_REQUEST;
    }

    r->filename =
        apr_pstrcat(r->pool, "proxy:balancer://", host, "/", path, (search) ? "?" : "", (search) ? search : "", NULL);

    r->path_info = apr_pstrcat(r->pool, "/", path, NULL);

    /* Check sticky sessions again in case of ProxyPass */
    route = apr_table_get(r->notes, "session-route");
    if (!route) {
        void *sconf = r->server->module_config;
        proxy_server_conf *conf = (proxy_server_conf *)ap_get_module_config(sconf, &proxy_module);

        proxy_vhost_table *vhost_table = (proxy_vhost_table *)apr_table_get(r->notes, "vhost-table");
        proxy_context_table *context_table = (proxy_context_table *)apr_table_get(r->notes, "context-table");
        proxy_balancer_table *balancer_table = (proxy_balancer_table *)apr_table_get(r->notes, "balancer-table");
        proxy_node_table *node_table = (proxy_node_table *)apr_table_get(r->notes, "node-table");

        if (!vhost_table) {
            vhost_table = read_vhost_table(r->pool, host_storage, 0);
        }

        if (!context_table) {
            context_table = read_context_table(r->pool, context_storage, 0);
        }

        if (!balancer_table) {
            balancer_table = read_balancer_table(r->pool, balancer_storage, 0);
        }

        if (!node_table) {
            node_table = read_node_table(r->pool, node_storage, 0);
        }

        get_route_balancer(r, conf, vhost_table, context_table, balancer_table, node_table, use_alias);
    }

    return OK;
}

/*
 * Find the worker that has the 'route' defined
 * (Should we also find the domain corresponding to it).
 */
static proxy_worker *find_route_worker(request_rec *r, const proxy_balancer *balancer, const char *route,
                                       const proxy_vhost_table *vhost_table, const proxy_context_table *context_table,
                                       const proxy_node_table *node_table)
{
    int i;
    int checking_standby;
    int checked_standby;
    int sizew = balancer->workers->elt_size;

    proxy_worker *worker;
    const node_context *nodecontext;

    checking_standby = checked_standby = 0;
    while (!checked_standby) {
        char *ptr = balancer->workers->elts;
        for (i = 0; i < balancer->workers->nelts; i++, ptr = ptr + sizew) {
            proxy_worker **run = (proxy_worker **)ptr;
            int index = (*run)->s->index;
            proxy_cluster_helper *helper = (*run)->context;
            worker = *run;
            if (index != helper->index) {
                ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                             "find_route_worker: find_route_worker skipping BAD worker");
                continue; /* skip it */
            }
            if (index == -1) {
                continue; /* marked removed */
            }

            if (checking_standby ? !PROXY_WORKER_IS_STANDBY(worker) : PROXY_WORKER_IS_STANDBY(worker)) {
                continue;
            }
            if (*(worker->s->route) && strcmp(worker->s->route, route) == 0) {
                /* that is the worker corresponding to the route */
                if (worker && PROXY_WORKER_IS_USABLE(worker)) {
                    /* The context may not be available */
                    nodeinfo_t *node;
                    if (read_node_worker(index, &node, worker) != APR_SUCCESS) {
                        return NULL; /* can't read node */
                    }
                    if ((nodecontext = context_host_ok(r, balancer, index, use_alias, vhost_table, context_table,
                                                       node_table)) != NULL) {
                        apr_table_setn(r->subprocess_env, "BALANCER_CONTEXT_ID",
                                       apr_psprintf(r->pool, "%d", nodecontext->context));
                        return worker;
                    }

                    return NULL; /* application has been removed from the node */
                }
                /*
                 * If the worker is in error state run
                 * retry on that worker. It will be marked as
                 * operational if the retry timeout is elapsed.
                 * The worker might still be unusable, but we try
                 * anyway.
                 */
                ap_proxy_retry_worker_fn("BALANCER", worker, r->server);
                if (PROXY_WORKER_IS_USABLE(worker)) {
                    /* The context may not be available */
                    nodeinfo_t *node;
                    if (node_storage->read_node(index, &node) != APR_SUCCESS) {
                        return NULL; /* can't read node */
                    }
                    if ((nodecontext = context_host_ok(r, balancer, index, use_alias, vhost_table, context_table,
                                                       node_table)) != NULL) {
                        apr_table_setn(r->subprocess_env, "BALANCER_CONTEXT_ID",
                                       apr_psprintf(r->pool, "%d", nodecontext->context));
                        return worker;
                    }

                    return NULL; /* application has been removed from the node */
                }
                /*
                 * We have a worker that is unusable.
                 * It can be in error or disabled, but in case
                 * it has a redirection set use that redirection worker.
                 * This enables to safely remove the member from the
                 * balancer. Of course you will need some kind of
                 * session replication between those two remote.
                 */
                if (*worker->s->redirect) {
                    proxy_worker *rworker = NULL;
                    rworker =
                        find_route_worker(r, balancer, worker->s->redirect, vhost_table, context_table, node_table);
                    /* Check if the redirect worker is usable */
                    if (rworker && !PROXY_WORKER_IS_USABLE(rworker)) {
                        /*
                         * If the worker is in error state run
                         * retry on that worker. It will be marked as
                         * operational if the retry timeout is elapsed.
                         * The worker might still be unusable, but we try
                         * anyway.
                         */
                        ap_proxy_retry_worker_fn("BALANCER", worker, r->server);
                    }
                    if (rworker && PROXY_WORKER_IS_USABLE(rworker)) {
                        /* The context may not be available */
                        nodeinfo_t *node;
                        if (node_storage->read_node(index, &node) != APR_SUCCESS) {
                            return NULL; /* can't read node */
                        }
                        if ((nodecontext = context_host_ok(r, balancer, index, use_alias, vhost_table, context_table,
                                                           node_table)) != NULL) {
                            apr_table_setn(r->subprocess_env, "BALANCER_CONTEXT_ID",
                                           apr_psprintf(r->pool, "%d", nodecontext->context));
                            return rworker;
                        }

                        return NULL; /* application has been removed from the node */
                    }
                }
            }
        }
        checked_standby = checking_standby++;
    }
    return NULL;
}

/*
 * Find the worker corresponding to the JVMRoute.
 */
static proxy_worker *find_session_route(const proxy_balancer *balancer, request_rec *r, const char **route,
                                        const char **sticky_used, char **url, const char **domain,
                                        const proxy_vhost_table *vhost_table, const proxy_context_table *context_table,
                                        const proxy_node_table *node_table)
{
    proxy_worker *worker = NULL;
    (void)url;

#if HAVE_CLUSTER_EX_DEBUG
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                 "find_session_route: sticky %s sticky_path: %s sticky_force: %d", balancer->s->sticky,
                 balancer->s->sticky_path, balancer->s->sticky_force);
#endif
    if (balancer->s->sticky[0] == '\0' || balancer->s->sticky_path[0] == '\0') {
        return NULL;
    }
    /* Check our MC subname: MC_NOT_STICKY: we don't need the route */
    if (strcmp(balancer->s->lbpname, MC_NOT_STICKY) == 0) {
        return NULL;
    }

    /* We already should have the route in the notes for the trans() */
    *route = apr_table_get(r->notes, "session-route");
    if (*route && (**route)) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "find_session_route: Using route %s", *route);
    } else {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "find_session_route: No route found");
        return NULL;
    }

    *sticky_used = apr_table_get(r->notes, "session-sticky");

    if (domain) {
        *domain = apr_table_get(r->notes, "CLUSTER_DOMAIN");
    }

    /* We have a route in path or in cookie
     * Find the worker that has this route defined.
     */
    worker = find_route_worker(r, balancer, *route, vhost_table, context_table, node_table);
    if (worker && strcmp(*route, worker->s->route)) {
        /*
         * Notice that the route of the worker chosen is different from
         * the route supplied by the client. (mod_proxy compatibility).
         */
        apr_table_setn(r->subprocess_env, "BALANCER_ROUTE_CHANGED", "1");
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "find_session_route: CLUSTER: Route changed from %s to %s",
                     *route, worker->s->route);
    }
    return worker;
}

static proxy_worker *find_best_worker(const proxy_balancer *balancer, const proxy_server_conf *conf, request_rec *r,
                                      const char *domain, int failoverdomain, const proxy_vhost_table *vhost_table,
                                      const proxy_context_table *context_table, proxy_node_table *node_table,
                                      int recurse)
{
    proxy_worker *candidate = NULL;
    apr_status_t rv;

    if ((rv = PROXY_THREAD_LOCK(balancer)) != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, rv, r->server,
                     "find_best_worker: CLUSTER: (%s). Lock failed for find_best_worker()", balancer->s->name);
        return NULL;
    }

    candidate = internal_find_best_byrequests(balancer, conf, r, domain, failoverdomain, vhost_table, context_table,
                                              node_table);

    if ((rv = PROXY_THREAD_UNLOCK(balancer)) != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, rv, r->server,
                     "find_best_worker: CLUSTER: (%s). Unlock failed for find_best_worker()", balancer->s->name);
    }

    if (candidate == NULL) {
        /* All the workers are in error state or disabled.
         * If the balancer has a timeout sleep for a while
         * and try again to find the worker. The chances are
         * that some other thread will release a connection.
         * By default the timeout is not set, and the server
         * returns SERVER_BUSY.
         */
#if APR_HAS_THREADS
        if (balancer->s->timeout && recurse) {
            /* XXX: This can perhaps be build using some
             * smarter mechanism, like tread_cond.
             * But since the statuses can came from
             * different childs, use the provided algo.
             */
            apr_interval_time_t timeout = balancer->s->timeout;
            apr_interval_time_t step, tval = 0;

            step = timeout / 100;
            while (tval < timeout) {
                apr_sleep(step);
                /* Try again */
                if ((candidate = find_best_worker(balancer, conf, r, domain, failoverdomain, vhost_table, context_table,
                                                  node_table, 0))) {
                    break;
                }
                tval += step;
            }
        }
#endif
    }
    return candidate;
}

static int rewrite_url(request_rec *r, const proxy_worker *worker, char **url)
{
    const char *scheme = strstr(*url, "://");
    const char *path = NULL;

    if (scheme) {
        path = ap_strchr_c(scheme + 3, '/');
    }

    /* we break the URL into host, port, uri */
    if (!worker) {
        return ap_proxyerror(r, HTTP_BAD_REQUEST,
                             apr_pstrcat(r->pool, "rewrite_url: missing worker. URI cannot be parsed: ", *url, NULL));
    }

    *url = apr_pstrcat(r->pool, worker->s->name_ex, path, NULL);

    return OK;
}

/* Remove the session information */
static void remove_session_route(request_rec *r, const char *name)
{
    char *path = NULL;
    char *url = r->filename;
    char *start = NULL;
    char *cookies;
    const char *readcookies;
    char *start_cookie;

    /* First try to manipulate the url. */
    for (path = strstr(url, name); path; path = strstr(path + 1, name)) {
        start = path;
        if (*(start - 1) == '&') {
            start--;
        }
        path += strlen(name);
        if (*path == '=') {
            ++path;
            if (strlen(path)) {
                char *filename = r->filename;
                while (*path != '&' && *path != '\0') {
                    path++;
                }
                /* We have it */
                *start = '\0';
                r->filename = apr_pstrcat(r->pool, filename, path, NULL);
                return;
            }
        }
    }
    /* Second try to manipulate the cookie header... */

    if ((readcookies = apr_table_get(r->headers_in, "Cookie"))) {
        cookies = apr_pstrdup(r->pool, readcookies);
        for (start_cookie = ap_strstr(cookies, name); start_cookie; start_cookie = ap_strstr(start_cookie + 1, name)) {
            if (start_cookie == cookies || start_cookie[-1] == ';' || start_cookie[-1] == ',' ||
                isspace(start_cookie[-1])) {

                start = start_cookie;
                if (start_cookie != cookies &&
                    (start_cookie[-1] == ';' || start_cookie[-1] == ',' || isspace(start_cookie[-1]))) {
                    start--;
                }
                start_cookie += strlen(name);
                while (*start_cookie && isspace(*start_cookie)) {
                    ++start_cookie;
                }
                if (*start_cookie == '=' && start_cookie[1]) {
                    /*
                     * Session cookie was found, get it's value
                     */
                    char *end_cookie;
                    char *cookie;
                    ++start_cookie;
                    if ((end_cookie = ap_strchr(start_cookie, ';')) == NULL) {
                        end_cookie = ap_strchr(start_cookie, ',');
                    }

                    cookie = cookies;
                    *start = '\0';
                    cookies = apr_pstrcat(r->pool, cookie, end_cookie, NULL);
                    apr_table_setn(r->headers_in, "Cookie", cookies);
                }
            }
        }
    }
}

/* XXX: check lock logic...
 * Update the context active request counter
 * NOTE: it needs to lock the whole context table
 */
static void upd_context_count(const char *id, int val, const server_rec *s)
{
    int ident = atoi(id);
    contextinfo_t *context;
    (void)s;

    if (context_storage->read_context(ident, &context) == APR_SUCCESS) {
        context->nbrequests = context->nbrequests + val;
    }
}

static apr_status_t decrement_busy_count(void *w)
{
    proxy_worker *worker = (proxy_worker *)w;
    if (worker->s->busy > 0) {
        worker->s->busy--;
    }

    return APR_SUCCESS;
}

/*
 * Find a worker for mod_proxy logic
 */
static int proxy_cluster_pre_request(proxy_worker **worker, proxy_balancer **balancer, request_rec *r,
                                     proxy_server_conf *conf, char **url)
{
    int access_status;
    proxy_worker *runtime;
    const char *route = NULL;
    const char *sticky = NULL;
    const char *domain = NULL;
    int failoverdomain = 0;
    apr_status_t rv;
    proxy_cluster_helper *helper;
    const char *context_id;

    /* the node should be filled in trans(). */
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
        ap_assert(node_storage->lock_nodes() == APR_SUCCESS);
        node_table = read_node_table(r->pool, node_storage, 0);
        node_storage->unlock_nodes();
    }

    *worker = NULL;
    if (*balancer) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "proxy_cluster_pre_request: url %s balancer %s", *url,
                     (*balancer)->s->name);
    } else {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "proxy_cluster_pre_request: url %s", *url);
    }
    /* Step 1: check if the url is for us
     * The url we can handle starts with 'balancer://'
     * If balancer is already provided skip the search
     * for balancer, because this is failover attempt.
     */
    if (*balancer) {
        /* Adjust the helper->count corresponding to the previous try */
        const char *worker_name = apr_table_get(r->subprocess_env, "BALANCER_WORKER_NAME");
        if (worker_name && *worker_name) {
            int i;
            int sizew = (*balancer)->workers->elt_size;
            char *ptr = (*balancer)->workers->elts;
            unsigned def = ap_proxy_hashfunc(worker_name, PROXY_HASHFUNC_DEFAULT);
            unsigned fnv = ap_proxy_hashfunc(worker_name, PROXY_HASHFUNC_FNV);
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "proxy_cluster_pre_request: worker %s", worker_name);
            /* Ajust the context counter here */
            context_id = apr_table_get(r->subprocess_env, "BALANCER_CONTEXT_ID");
            ap_assert(node_storage->lock_nodes() == APR_SUCCESS);
            if (context_id && *context_id) {
                upd_context_count(context_id, -1, r->server);
            }
            for (i = 0; i < (*balancer)->workers->nelts; i++, ptr = ptr + sizew) {
                proxy_worker **run = (proxy_worker **)ptr;
                if ((*run)->hash.def == def && (*run)->hash.fnv == fnv) {
                    helper = (proxy_cluster_helper *)(*run)->context;
                    if (helper->count_active > 0) {
                        helper->count_active--;
                    }
                    break;
                }
            }
            node_storage->unlock_nodes();
        } else {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "proxy_cluster_pre_request: NO worker");
        }
    }

    /* TODO if we don't have a balancer but a route we should use it directly */
    if (!*balancer && !(*balancer = ap_proxy_get_balancer(r->pool, conf, *url, 0))) {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "proxy_cluster_pre_request: NOT CREATED!!!");
        /* May be the node has not been created yet */
        ap_assert(node_storage->lock_nodes() == APR_SUCCESS);
        update_workers_node(conf, r->pool, r->server, 1, node_table);
        check_workers(conf, r->server);
        node_storage->unlock_nodes();
        if (!(*balancer = ap_proxy_get_balancer(r->pool, conf, *url, 0))) {
            /* node_storage->unlock_nodes(); */
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, "proxy_cluster_pre_request: CLUSTER no balancer for %s",
                         *url);
            return DECLINED;
        }
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                     "proxy_cluster_pre_request: CLUSTER balancer CREATED for %s", *url);
    }

    /* Step 2: find the session route */

    runtime = find_session_route(*balancer, r, &route, &sticky, url, &domain, vhost_table, context_table, node_table);

    /* Lock the LoadBalancer
     * XXX: perhaps we need the process lock here
     */
    if ((rv = PROXY_THREAD_LOCK(*balancer)) != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, rv, r->server,
                     "proxy_cluster_pre_request: CLUSTER: (%s). Lock failed for pre_request", (*balancer)->s->name);
        return DECLINED;
    }
    if (runtime) {
        runtime->s->elected++;
        *worker = runtime;
    } else if (route && ((*balancer)->s->sticky_force)) {
        if (domain == NULL) {
            /*
             * We have a route provided that doesn't match the
             * balancer name. See if the provider route is the
             * member of the same balancer in which case return 503
             */
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server,
                         "proxy_cluster_pre_request: CLUSTER: (%s). All workers are in error state for route (%s)",
                         (*balancer)->s->name, route);
            if ((rv = PROXY_THREAD_UNLOCK(*balancer)) != APR_SUCCESS) {
                ap_log_error(APLOG_MARK, APLOG_ERR, rv, r->server,
                             "proxy_cluster_pre_request: CLUSTER: (%s). Unlock failed for pre_request",
                             (*balancer)->s->name);
            }
            return HTTP_SERVICE_UNAVAILABLE;
        }

        /* We try to to failover using another node in the domain */
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "proxy_cluster_pre_request: failover in domain");
        failoverdomain = 1;
    }

    if ((rv = PROXY_THREAD_UNLOCK(*balancer)) != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, rv, r->server,
                     "proxy_cluster_pre_request: CLUSTER: (%s). Unlock failed for pre_request", (*balancer)->s->name);
    }
    if (!*worker) {
        /* We have to failover (in domain only may be) or we don't use sticky sessions */
        runtime =
            find_best_worker(*balancer, conf, r, domain, failoverdomain, vhost_table, context_table, node_table, 1);
        if (!runtime) {
            const char *no_context_error = apr_table_get(r->notes, "no-context-error");
            if (no_context_error == NULL) {
                ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server,
                             "proxy_cluster_pre_request: CLUSTER: (%s). All workers are in error state",
                             (*balancer)->s->name);

                return HTTP_SERVICE_UNAVAILABLE;
            }

            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                         "proxy_cluster_pre_request: CLUSTER: (%s). No context for the URL", (*balancer)->s->name);
            return HTTP_NOT_FOUND;
        }
        if ((*balancer)->s->sticky[0] != '\0' && runtime) {
            /*
             * This balancer has sticky sessions and the client either has not
             * supplied any routing information or all workers for this route
             * including possible redirect and hotstandby workers are in error
             * state, but we have found another working worker for this
             * balancer where we can send the request. Thus notice that we have
             * changed the route to the backend.
             */
            apr_table_setn(r->subprocess_env, "BALANCER_ROUTE_CHANGED", "1");
        }
        /* Use MC_R in lbpname to know if we have to remove the session information */
        if (route && strcmp((*balancer)->s->lbpname, MC_REMOVE_SESSION) == 0) {
            /* Failover to another domain. Remove sessionid information. */
            const char *domain_ok = apr_table_get(r->notes, "session-domain-ok");
            if (!domain_ok) {
                remove_session_route(r, sticky);
            }
        }
        *worker = runtime;
    }

    (*worker)->s->busy++;
    apr_pool_cleanup_register(r->pool, *worker, decrement_busy_count, apr_pool_cleanup_null);

    /* Also mark the context here note that find_best_worker set BALANCER_CONTEXT_ID */
    context_id = apr_table_get(r->subprocess_env, "BALANCER_CONTEXT_ID");
    ap_assert(node_storage->lock_nodes() == APR_SUCCESS);
    if (context_id && *context_id) {
        upd_context_count(context_id, 1, r->server);
    }

    /* Mark the worker used for the cleanup logic */
    /* XXX: Do we need the lock here??? */
    helper = (proxy_cluster_helper *)(*worker)->context;
    helper->count_active++;
    node_storage->unlock_nodes();

    /*
     * get_route_balancer already fills all of the notes and some subprocess_env
     * but not all.
     * Note that BALANCER_WORKER_NAME would have changed in case of failover.
     */
    /* Add balancer/worker info to env. */
    apr_table_setn(r->subprocess_env, "BALANCER_NAME", (*balancer)->s->name);
    apr_table_setn(r->subprocess_env, "BALANCER_WORKER_NAME", (*worker)->s->name_ex);
    apr_table_setn(r->subprocess_env, "BALANCER_WORKER_ROUTE", (*worker)->s->route);

    /* Rewrite the url from 'balancer://url'
     * to the 'worker_scheme://worker_hostname[:worker_port]/url'
     * This replaces the balancers fictional name with the
     * real hostname of the elected worker.
     */
    access_status = rewrite_url(r, *worker, url);

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                 "proxy_cluster_pre_request: balancer (%s) worker (%s) rewritten to %s", (*balancer)->s->name,
                 (*worker)->s->name_ex, *url);
    return access_status;
}

static int proxy_cluster_post_request(proxy_worker *worker, proxy_balancer *balancer, request_rec *r,
                                      proxy_server_conf *conf)
{
    proxy_cluster_helper *helper;
    const char *sessionid;
    const char *route;
    char *cookie = NULL;
    const char *sticky;
    char *oroute;
    const char *context_id = apr_table_get(r->subprocess_env, "BALANCER_CONTEXT_ID");
    (void)conf; /* unused argument */

    /* Ajust the context counter here too */
    ap_assert(node_storage->lock_nodes() == APR_SUCCESS);
    if (context_id && *context_id) {
        upd_context_count(context_id, -1, r->server);
    }

    /* mark the worker as not in use */
    helper = (proxy_cluster_helper *)worker->context;
    if (helper->count_active > 0) {
        helper->count_active--;
    }

    node_storage->unlock_nodes();

#if HAVE_CLUSTER_EX_DEBUG
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "proxy_cluster_post_request: for (%s) %s", balancer->s->name,
                 balancer->s->sticky);
#endif

    if (sessionid_storage) {

        /* Add information about sessions corresponding to a node */
        sticky = apr_table_get(r->notes, "session-sticky");
        if (sticky == NULL && balancer->s->sticky[0] != '\0') {
            sticky = apr_pstrdup(r->pool, balancer->s->sticky);
        }
        if (sticky != NULL) {
            cookie = get_cookie_param(r, sticky, 0);
            sessionid = apr_table_get(r->notes, "session-id");
            route = apr_table_get(r->notes, "session-route");
            if (cookie) {
                if (sessionid && strcmp(cookie, sessionid)) {
                    /* The cookie has changed, remove the old one and store the next one */
                    sessionidinfo_t ou;
#if HAVE_CLUSTER_EX_DEBUG
                    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                                 "proxy_cluster_post_request: sessionid changed (%s to %s)", sessionid, cookie);
#endif
                    strncpy(ou.sessionid, sessionid, SESSIONIDSZ);
                    ou.id = -1;
                    sessionid_storage->remove_sessionid(&ou);
                }
                if ((oroute = strchr(cookie, '.')) != NULL) {
                    oroute++;
                }
                route = oroute;
                sessionid = cookie;
            }

            if (sessionid && route) {
                sessionidinfo_t ou;
                strncpy(ou.sessionid, sessionid, SESSIONIDSZ);
                strncpy(ou.JVMRoute, route, JVMROUTESZ);
                sessionid_storage->insert_update_sessionid(&ou);
            }
        }
    }

    /*  20051115.25 (2.2.17) Add errstatuses member to proxy_balancer */
    if (!apr_is_empty_array(balancer->errstatuses)) {
        int i;
        apr_status_t rv;
        if ((rv = PROXY_THREAD_LOCK(balancer)) != APR_SUCCESS) {
            ap_log_error(APLOG_MARK, APLOG_ERR, rv, r->server,
                         "proxy_cluster_post_request: BALANCER: (%s). Lock failed for post_request", balancer->s->name);
            return HTTP_INTERNAL_SERVER_ERROR;
        }
        for (i = 0; i < balancer->errstatuses->nelts; i++) {
            int val = ((int *)balancer->errstatuses->elts)[i];
            if (r->status == val) {
                ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
                              "proxy_cluster_post_request: %s Forcing worker (%s) into error state "
                              "due to status code %d matching 'failonstatus' "
                              "balancer parameter",
                              balancer->s->name, worker->s->name_ex, val);
                worker->s->status |= PROXY_WORKER_IN_ERROR;
                worker->s->error_time = apr_time_now();
                break;
            }
        }
        if ((rv = PROXY_THREAD_UNLOCK(balancer)) != APR_SUCCESS) {
            ap_log_error(APLOG_MARK, APLOG_ERR, rv, r->server,
                         "proxy_cluster_post_request: BALANCER: (%s). Unlock failed for post_request",
                         balancer->s->name);
        }
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "proxy_cluster_post_request: %d for (%s)", r->status,
                 balancer->s->name);

    return OK;
}

/*
 * Register the hooks on our module.
 */
static void proxy_cluster_hooks(apr_pool_t *p)
{
    static const char *const aszPre[] = {"mod_manager.c", "mod_rewrite.c", NULL};
    static const char *const aszSucc[] = {"mod_proxy.c", NULL};

    ap_hook_post_config(proxy_cluster_post_config, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_pre_config(proxy_cluster_pre_config, NULL, NULL, APR_HOOK_MIDDLE);

    /* create the "maintenance" thread */
    ap_hook_child_init(proxy_cluster_child_init, aszPre, NULL, APR_HOOK_LAST);

    /* stop it */
    ap_hook_child_stopping(proxy_cluster_child_stopping, NULL, NULL, APR_HOOK_MIDDLE);

    /* check the url and give the mapping to mod_proxy */
    ap_hook_translate_name(proxy_cluster_trans, aszPre, aszSucc, APR_HOOK_FIRST);

    proxy_hook_canon_handler(proxy_cluster_canon, NULL, NULL, APR_HOOK_FIRST);

    proxy_hook_pre_request(proxy_cluster_pre_request, NULL, NULL, APR_HOOK_FIRST);
    proxy_hook_post_request(proxy_cluster_post_request, NULL, NULL, APR_HOOK_FIRST);

    /* Register a provider for the "ping/pong" logic */
    ap_register_provider(p, "proxy_cluster", "balancer", "0", &balancerhandler);
    /* Register a provider for the loadbalancer (for things like ProxyPass /titi balancer://mycluster/myapp) */
    ap_register_provider(p, PROXY_LBMETHOD, "byrequests", "0", &balancerhandler);
}

static void *create_proxy_cluster_server_config(apr_pool_t *p, server_rec *s)
{
    (void)p;
    (void)s;
    return NULL;
}

static const char *cmd_proxy_cluster_creatbal(cmd_parms *cmd, void *dummy, const char *arg)
{
    int val = atoi(arg);
    (void)cmd;
    (void)dummy;

    if (val < 0 || val > 2) {
        return "CreateBalancers must be one of: 0, 1 or 2";
    }

    creat_bal = val;
    return NULL;
}

static const char *cmd_proxy_cluster_use_alias(cmd_parms *cmd, void *dummy, const char *arg)
{
    (void)cmd;
    (void)dummy;

    /* Cannot use AP_INIT_FLAG, to keep compatibility with versions <= 1.3.0. Final which accepted
       only values 1 and 0. (see MODCLUSTER-403) */
    if (strcasecmp(arg, "Off") == 0 || strcasecmp(arg, "0") == 0) {
        use_alias = 0;
    } else if (strcasecmp(arg, "On") == 0 || strcasecmp(arg, "1") == 0) {
        use_alias = 1;
    } else {
        return "UseAlias must be either On or Off";
    }

    return NULL;
}

static const char *cmd_proxy_cluster_lbstatus_recalc_time(cmd_parms *cmd, void *dummy, const char *arg)
{
    int val = atoi(arg);
    (void)cmd;
    (void)dummy;

    if (val < 0) {
        return "LBstatusRecalTime must be greater than 0";
    }

    lbstatus_recalc_time = apr_time_from_sec(val);
    return NULL;
}

static const char *cmd_proxy_cluster_wait_for_remove(cmd_parms *cmd, void *dummy, const char *arg)
{
    int val = atoi(arg);
    (void)cmd;
    (void)dummy;

    if (val < 10) {
        return "WaitForRemove must be greater than 10";
    }

    wait_for_remove = apr_time_from_sec(val);
    return NULL;
}

static const char *cmd_proxy_cluster_enable_options(cmd_parms *cmd, void *dummy, const char *args)
{
    char *val = ap_getword_conf(cmd->pool, &args);
    (void)dummy;

    if (strcasecmp(val, "Off") == 0 || strcasecmp(val, "0") == 0) {
        /* Disables OPTIONS, overrides the default */
        enable_options = 0;
    } else if (strcmp(val, "") == 0 || strcasecmp(val, "On") == 0 || strcasecmp(val, "1") == 0) {
        /* No param or explicitly set default */
        enable_options = 1;
    } else {
        return "EnableOptions must be either without value or On or Off";
    }

    return NULL;
}

static const char *cmd_proxy_cluster_deterministic_failover(cmd_parms *parms, void *mconfig, int on)
{
    deterministic_failover = on;
    (void)parms;
    (void)mconfig;

    return NULL;
}

static const char *cmd_proxy_cluster_cache_shared_for(cmd_parms *cmd, void *dummy, const char *arg)
{
    int val = atoi(arg);
    (void)cmd;
    (void)dummy;

    if (val < 0) {
        return "CacheShareFor must be greater than 0";
    }

    cache_share_for = apr_time_from_sec(val);
    return NULL;
}

static const char *cmd_proxy_cluster_proxyhctemplate(cmd_parms *cmd, void *dummy, const char *arg)
{
    proxy_worker worker;
    proxy_worker_shared shared;
    const char *err;
    apr_pool_t *pool;
    server_rec *s = cmd->server;
    (void)dummy;

    proxyhctemplate = apr_pstrdup(cmd->pool, arg);
    while (*arg) {
        char *key, *val;
        key = ap_getword_conf(cmd->pool, &arg);
        val = strchr(key, '=');
        if (!val) {
            return "Invalid ProxyHCTemplate parameter. Parameter must be in the form 'key=value'";
        }

        *val++ = '\0';
        /* are we able to check more stuff? err= test() */
        if (set_worker_hc_param_f == NULL) {
            return "Can't check ProxyHCTemplate parameter, is proxy_hcheck_module loaded?";
        }

        worker.s = &shared;
        apr_pool_create(&pool, cmd->pool);
        err = set_worker_hc_param_f(pool, s, &worker, key, val, NULL);
        apr_pool_destroy(pool);
        if (err != NULL) {
            return apr_psprintf(cmd->pool, "%s key: %s=%s", err, key, val);
        }
    }
    return NULL;
}

static const char *cmd_proxy_cluster_use_nocanon(cmd_parms *parms, void *mconfig, int on)
{
    (void)parms;
    (void)mconfig;
    use_nocanon = on;

    return NULL;
}


#if MC_USE_THREADS
static const char *cmd_mc_thread_count(cmd_parms *cmd, void *dummy, const char *arg)
{
    int size;
    (void)cmd;
    (void)dummy;
    size = atoi(arg);
    if (size < 0) {
        return "Invalid value for ModProxyClusterThreadCount. Parameter has to be >= 0";
    }

    mc_thread_pool_size = size;
    return NULL;
}
#endif

/* clang-format off */
static const command_rec proxy_cluster_cmds[] = {
    AP_INIT_TAKE1("CreateBalancers", cmd_proxy_cluster_creatbal, NULL, OR_ALL,
                  "CreateBalancers - Defined VirtualHosts where the balancers are created 0: All, 1: None, 2: Main "
                  "(Default: 2 Main)"),
    AP_INIT_TAKE1("UseAlias", cmd_proxy_cluster_use_alias, NULL, OR_ALL,
                  "UseAlias - Check that the Alias corresponds to the ServerName Off: Don't check (ignore Aliases), On: "
                  "Check aliases (Default: Off)"),
    AP_INIT_TAKE1("LBstatusRecalTime", cmd_proxy_cluster_lbstatus_recalc_time, NULL, OR_ALL,
                  "LBstatusRecalTime - Time interval in seconds for loadbalancing logic to recalculate the status of a "
                  "node: (Default: 5 seconds)"),
    AP_INIT_TAKE1("WaitBeforeRemove", cmd_proxy_cluster_wait_for_remove, NULL, OR_ALL,
                  "WaitBeforeRemove - Time in seconds before a node removed is forgotten by httpd: (Default: 10 seconds)"),
    /* This is not the ideal type, but it either takes no parameters (for backwards compatibility) or 1 flag argument. */
    AP_INIT_RAW_ARGS("EnableOptions", cmd_proxy_cluster_enable_options, NULL, OR_ALL,
                     "EnableOptions - Use OPTIONS with HTTP/HTTPS for CPING/CPONG. On: Use OPTIONS, Off: Do not use "
                     "OPTIONS (Default: On)"),
    AP_INIT_FLAG("DeterministicFailover", cmd_proxy_cluster_deterministic_failover, NULL, OR_ALL,
                 "DeterministicFailover - controls whether a node upon failover is chosen deterministically (Default: Off)"),
    AP_INIT_TAKE1("CacheShareFor", cmd_proxy_cluster_cache_shared_for, NULL, OR_ALL,
                  "CacheShareFor - Time in seconds for how long the shared information is cached by httpd: (Default: 0 "
                  "seconds, no-caching)"),
    AP_INIT_RAW_ARGS("ModProxyClusterHCTemplate", cmd_proxy_cluster_proxyhctemplate, NULL, OR_ALL,
                     "ModProxyClusterHCTemplate - Set of health check parameters to use with mod_proxy_cluster workers."),
    AP_INIT_FLAG("UseNocanon", cmd_proxy_cluster_use_nocanon, NULL, OR_ALL,
                 "UseNocanon - When no ProxyPass or ProxyMatch for the URL, passes the URL path \"raw\" to the backend "
                 "(Default: Off)"),
#if MC_USE_THREADS
    AP_INIT_TAKE1("ModProxyClusterThreadCount", cmd_mc_thread_count, NULL, OR_ALL,
                  "ModProxyClusterThreadCount - Set custom size for the watchdog thread pool (Default: 16)"),
#endif
    {NULL}
};
/* clang-format on */

module AP_MODULE_DECLARE_DATA proxy_cluster_module = {
    STANDARD20_MODULE_STUFF,
    NULL,                               /* per-directory config creator */
    NULL,                               /* dir config merger */
    create_proxy_cluster_server_config, /* server config creator */
    NULL,                               /* server config merger */
    proxy_cluster_cmds,                 /* command table */
    proxy_cluster_hooks,                /* register hooks */
    AP_MODULE_FLAG_NONE                 /* flags */
};
