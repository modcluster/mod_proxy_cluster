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
    int i;
    void *sconf = r->server->module_config;
    proxy_server_conf *conf = (proxy_server_conf *)
        ap_get_module_config(sconf, &proxy_module);
    char *ptr = conf->balancers->elts;
    int sizeb = conf->balancers->elt_size;
    char *balancername = NULL;


    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r->server,
                "lbmethod_cluster_trans for %d %s %s uri: %s args: %s unparsed_uri: %s",
                 r->proxyreq, r->filename, r->handler, r->uri, r->args, r->unparsed_uri);
    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r->server,
                "lbmethod_cluster_trans for %d", conf->balancers->nelts);
    for (i = 0; i < conf->balancers->nelts; i++, ptr=ptr+sizeb) {
        proxy_balancer *balancer = (proxy_balancer *) ptr;
        ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r->server,
                    "lbmethod_cluster_trans for %s", balancer->s->name);
        balancername = balancer->s->name;
    }
    if (!balancername)
       return DECLINED;

    if (strncmp(r->uri, "/examples", 9) == 0) {
        r->filename =  apr_pstrcat(r->pool, "proxy:", balancername, r->uri, NULL);
        r->handler = "proxy-server";
        r->proxyreq = PROXYREQ_REVERSE;
    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, 0, r->server,
                "lbmethod_cluster_trans (OK) for %d %s %s uri: %s args: %s unparsed_uri: %s",
                 r->proxyreq, r->filename, r->handler, r->uri, r->args, r->unparsed_uri);
        return OK;
    }
    return DECLINED;
}

static void register_hooks(apr_pool_t *p)
{
    static const char * const aszPre[]={ "mod_manager.c", "mod_rewrite.c", NULL };
    static const char * const aszSucc[]={ "mod_proxy.c", NULL };

    ap_register_provider(p, PROXY_LBMETHOD, "cluster", "0", &cluster);

    ap_hook_translate_name(lbmethod_cluster_trans, aszPre, aszSucc, APR_HOOK_FIRST);
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
