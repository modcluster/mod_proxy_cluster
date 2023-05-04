/*
 * Common routines
 */

#include "httpd.h"
#include "http_config.h"
#include "http_log.h"
#include "http_main.h"
#include "http_request.h"
#include "mod_proxy.h"

#include "slotmem.h"

#include "domain.h"
#include "node.h"
#include "host.h"
#include "context.h"
#include "balancer.h"

#include "mod_proxy_cluster.h"

/* Read the virtual host table from shared memory */
proxy_vhost_table *read_vhost_table(apr_pool_t *pool, struct host_storage_method *host_storage, int for_cache)
{
    int i;
    int size;
    proxy_vhost_table *vhost_table = apr_palloc(pool, sizeof(proxy_vhost_table));
    size = host_storage->get_max_size_host();
    if (size == 0) {
        vhost_table->sizevhost = 0;
        vhost_table->vhosts = NULL;
        vhost_table->vhost_info = NULL;
        return vhost_table;
    }

    vhost_table->vhosts = apr_palloc(pool, sizeof(int) * host_storage->get_max_size_host());
    vhost_table->sizevhost = host_storage->get_ids_used_host(vhost_table->vhosts);
    if (for_cache)
        vhost_table->vhost_info = apr_palloc(pool, sizeof(hostinfo_t) * size);
    else
        vhost_table->vhost_info = apr_palloc(pool, sizeof(hostinfo_t) * vhost_table->sizevhost);
    for (i = 0; i < vhost_table->sizevhost; i++) {
        hostinfo_t *h;
        int host_index = vhost_table->vhosts[i];
        host_storage->read_host(host_index, &h);
        vhost_table->vhost_info[i] = *h;
    }
    return vhost_table;
}

/* Update the virtual host table from shared memory to populate a cached table */
proxy_vhost_table *update_vhost_table_cached(proxy_vhost_table *vhost_table, struct host_storage_method *host_storage)
{
    int i;
    int size;
    size = host_storage->get_max_size_host();
    if (size == 0) {
        vhost_table->sizevhost = 0;
        vhost_table->vhosts = NULL;
        vhost_table->vhost_info = NULL;
        return vhost_table;
    }

    vhost_table->sizevhost = host_storage->get_ids_used_host(vhost_table->vhosts);
    for (i = 0; i < vhost_table->sizevhost; i++) {
        hostinfo_t *h;
        int host_index = vhost_table->vhosts[i];
        host_storage->read_host(host_index, &h);
        vhost_table->vhost_info[i] = *h;
    }
    return vhost_table;
}

/* Read the context table from shared memory */
proxy_context_table *read_context_table(apr_pool_t *pool, struct context_storage_method *context_storage, int for_cache)
{
    int i;
    int size;
    proxy_context_table *context_table = apr_palloc(pool, sizeof(proxy_context_table));
    size = context_storage->get_max_size_context();
    if (size == 0) {
        context_table->sizecontext = 0;
        context_table->contexts = NULL;
        context_table->context_info = NULL;
        return context_table;
    }
    context_table->contexts = apr_palloc(pool, sizeof(int) * size);
    context_table->sizecontext = context_storage->get_ids_used_context(context_table->contexts);
    if (for_cache)
        context_table->context_info = apr_palloc(pool, sizeof(contextinfo_t) * size);
    else
        context_table->context_info = apr_palloc(pool, sizeof(contextinfo_t) * context_table->sizecontext);
    for (i = 0; i < context_table->sizecontext; i++) {
        contextinfo_t *h;
        int context_index = context_table->contexts[i];
        context_storage->read_context(context_index, &h);
        context_table->context_info[i] = *h;
    }
    return context_table;
}

/* Update the context table from shared memory to populate a cached table */
proxy_context_table *update_context_table_cached(proxy_context_table *context_table,
                                                 struct context_storage_method *context_storage)
{
    int i;
    int size;
    size = context_storage->get_max_size_context();
    if (size == 0) {
        context_table->sizecontext = 0;
        context_table->contexts = NULL;
        context_table->context_info = NULL;
        return context_table;
    }
    context_table->sizecontext = context_storage->get_ids_used_context(context_table->contexts);
    for (i = 0; i < context_table->sizecontext; i++) {
        contextinfo_t *h;
        int context_index = context_table->contexts[i];
        context_storage->read_context(context_index, &h);
        context_table->context_info[i] = *h;
    }
    return context_table;
}

/* Read the balancer table from shared memory */
proxy_balancer_table *read_balancer_table(apr_pool_t *pool, struct balancer_storage_method *balancer_storage,
                                          int for_cache)
{
    int i;
    int size;
    proxy_balancer_table *balancer_table = apr_palloc(pool, sizeof(proxy_balancer_table));
    size = balancer_storage->get_max_size_balancer();
    if (size == 0) {
        balancer_table->sizebalancer = 0;
        balancer_table->balancers = NULL;
        balancer_table->balancer_info = NULL;
        return balancer_table;
    }
    balancer_table->balancers = apr_palloc(pool, sizeof(int) * size);
    balancer_table->sizebalancer = balancer_storage->get_ids_used_balancer(balancer_table->balancers);
    if (for_cache)
        balancer_table->balancer_info = apr_palloc(pool, sizeof(balancerinfo_t) * size);
    else
        balancer_table->balancer_info = apr_palloc(pool, sizeof(balancerinfo_t) * balancer_table->sizebalancer);
    for (i = 0; i < balancer_table->sizebalancer; i++) {
        balancerinfo_t *h;
        int balancer_index = balancer_table->balancers[i];
        balancer_storage->read_balancer(balancer_index, &h);
        balancer_table->balancer_info[i] = *h;
    }
    return balancer_table;
}

/* Update the balancer table from shared memory to populate a cached table */
proxy_balancer_table *update_balancer_table_cached(proxy_balancer_table *balancer_table,
                                                   struct balancer_storage_method *balancer_storage)
{
    int i;
    int size;
    size = balancer_storage->get_max_size_balancer();
    if (size == 0) {
        balancer_table->sizebalancer = 0;
        balancer_table->balancers = NULL;
        balancer_table->balancer_info = NULL;
        return balancer_table;
    }
    balancer_table->sizebalancer = balancer_storage->get_ids_used_balancer(balancer_table->balancers);
    for (i = 0; i < balancer_table->sizebalancer; i++) {
        balancerinfo_t *h;
        int balancer_index = balancer_table->balancers[i];
        balancer_storage->read_balancer(balancer_index, &h);
        balancer_table->balancer_info[i] = *h;
    }
    return balancer_table;
}

/* Read the node table from shared memory */
proxy_node_table *read_node_table(apr_pool_t *pool, struct node_storage_method *node_storage, int for_cache)
{
    int i;
    int size;
    proxy_node_table *node_table = apr_palloc(pool, sizeof(proxy_node_table));
    size = node_storage->get_max_size_node();
    if (size == 0) {
        node_table->sizenode = 0;
        node_table->nodes = NULL;
        node_table->node_info = NULL;
        return node_table;
    }
    node_table->nodes = apr_palloc(pool, sizeof(int) * size);
    node_table->sizenode = node_storage->get_ids_used_node(node_table->nodes);
    if (for_cache) {
        node_table->node_info = apr_palloc(pool, sizeof(nodeinfo_t) * size);
        node_table->ptr_node = apr_palloc(pool, sizeof(char *) * size);
    }
    else {
        node_table->node_info = apr_palloc(pool, sizeof(nodeinfo_t) * node_table->sizenode);
        node_table->ptr_node = apr_palloc(pool, sizeof(char *) * node_table->sizenode);
    }
    for (i = 0; i < node_table->sizenode; i++) {
        nodeinfo_t *h;
        int node_index = node_table->nodes[i];
        apr_status_t rv = node_storage->read_node(node_index, &h);
        if (rv == APR_SUCCESS) {
            node_table->node_info[i] = *h;
            node_table->ptr_node[i] = (char *)h;
        }
        else {
            /* we can't read the node! */
            node_table->ptr_node[i] = NULL;
            memset(&node_table->node_info[i], 0, sizeof(nodeinfo_t));
        }
    }
    return node_table;
}

/* Update the node table from shared memory to populate a cached table*/
proxy_node_table *update_node_table_cached(proxy_node_table *node_table, struct node_storage_method *node_storage)
{
    int i;
    int size;
    size = node_storage->get_max_size_node();
    if (size == 0) {
        node_table->sizenode = 0;
        node_table->nodes = NULL;
        node_table->node_info = NULL;
        return node_table;
    }
    node_table->sizenode = node_storage->get_ids_used_node(node_table->nodes);
    for (i = 0; i < node_table->sizenode; i++) {
        nodeinfo_t *h;
        int node_index = node_table->nodes[i];
        apr_status_t rv = node_storage->read_node(node_index, &h);
        if (rv == APR_SUCCESS) {
            node_table->node_info[i] = *h;
            node_table->ptr_node[i] = (char *)h;
        }
        else {
            /* we can't read the node! */
            node_table->ptr_node[i] = NULL;
            memset(&node_table->node_info[i], 0, sizeof(nodeinfo_t));
        }
    }
    return node_table;
}

/*
 * Read the cookie corresponding to name
 * @param r request.
 * @param name name of the cookie
 * @param in tells if cookie should read from the request or the response
 * @return the value of the cookie
 */
char *get_cookie_param(request_rec *r, const char *name, int in)
{
    const char *cookies;
    const char *start_cookie;

    if (in)
        cookies = apr_table_get(r->headers_in, "Cookie");
    else
        cookies = apr_table_get(r->headers_out, "Set-Cookie");
    if (cookies) {
        for (start_cookie = ap_strstr_c(cookies, name); start_cookie;
             start_cookie = ap_strstr_c(start_cookie + 1, name)) {
            if (start_cookie == cookies || start_cookie[-1] == ';' || start_cookie[-1] == ',' ||
                isspace(start_cookie[-1])) {

                start_cookie += strlen(name);
                while (*start_cookie && isspace(*start_cookie))
                    ++start_cookie;
                if (*start_cookie == '=' && start_cookie[1]) {
                    /*
                     * Session cookie was found, get it's value
                     */
                    char *end_cookie, *cookie;
                    ++start_cookie;
                    cookie = apr_pstrdup(r->pool, start_cookie);
                    if ((end_cookie = strchr(cookie, ';')) != NULL)
                        *end_cookie = '\0';
                    if ((end_cookie = strchr(cookie, ',')) != NULL)
                        *end_cookie = '\0';
                    /* remove " from version1 cookies */
                    if (*cookie == '\"' && *(cookie + strlen(cookie) - 1) == '\"') {
                        ++cookie;
                        *(cookie + strlen(cookie) - 1) = '\0';
                        cookie = apr_pstrdup(r->pool, cookie);
                    }
                    return cookie;
                }
            }
        }
    }
    return NULL;
}

/**
 * Retrieve the parameter with the given name
 * Something like 'JSESSIONID=12345...N'
 *
 * TODO: Should use the mod_proxy_balancer one
 */
char *get_path_param(apr_pool_t *pool, char *url, const char *name)
{
    char *path = NULL;
    char *pathdelims = ";?&";

    for (path = strstr(url, name); path; path = strstr(path + 1, name)) {

        /* Must be following a ';' and followed by '=' to be the correct session id name */
        if (*(path - 1) == ';') {
            path += strlen(name);
            if (*path == '=') {
                /*
                 * Session path was found, get its value
                 */
                ++path;
                if (*path) {
                    char *q;
                    path = apr_strtok(apr_pstrdup(pool, path), pathdelims, &q);
                    return path;
                }
            }
        }
    }
    return NULL;
}

/**
 * Check that the request has a sessionid with a route
 * @param r the request_rec.
 * @param stickyval the cookie or/and parameter name.
 * @param uri part of the URL to for the session parameter.
 * @param sticky_used the string that was used to find the route
 */
char *cluster_get_sessionid(request_rec *r, const char *stickyval, char *uri, char **sticky_used)
{
    char *sticky, *sticky_path;
    char *path;
    char *route;

    /* for 2.2.x the sticky parameter may contain 2 values */
    sticky = sticky_path = apr_pstrdup(r->pool, stickyval);
    if ((path = strchr(sticky, '|'))) {
        *path++ = '\0';
        sticky_path = path;
    }
    *sticky_used = sticky_path;
    route = get_cookie_param(r, sticky, 1);
    if (!route) {
        route = get_path_param(r->pool, uri, sticky_path);
        *sticky_used = sticky;
    }
    return route;
}

/**
 * Check that the request has a sessionid (even invalid)
 * @param r the request_rec.
 * @param nodeid the node id.
 * @param route (if received)
 * @return 1 is it finds a sessionid 0 otherwise.
 */
int hassession_byname(request_rec *r, int nodeid, const char *route, proxy_node_table *node_table)
{
    proxy_balancer *balancer = NULL;
    char *sessionid;
    char *uri;
    char *sticky_used;
    char *sticky;
    int i;
    proxy_server_conf *conf;
    nodeinfo_t *node;
    int sizeb;
    char *ptr;

    /* well we already have it */
    if (route != NULL && (*route != '\0'))
        return 1;

    /* read the node */
    node = table_get_node(node_table, nodeid);
    if (node == NULL)
        return 0; /* failed */

    conf = (proxy_server_conf *)ap_get_module_config(r->server->module_config, &proxy_module);
    sizeb = conf->balancers->elt_size;
    ptr = conf->balancers->elts;
    for (i = 0; i < conf->balancers->nelts; i++, ptr = ptr + sizeb) {
        balancer = (proxy_balancer *)ptr;
        if (strlen(balancer->s->name) > 11 && strcasecmp(&balancer->s->name[11], node->mess.balancer) == 0)
            break;
    }
    if (i == conf->balancers->nelts)
        balancer = NULL;

    /* XXX: We don't find the balancer, that is BAD */
    if (balancer == NULL)
        return 0;

    sticky = apr_psprintf(r->pool, "%s|%s", balancer->s->sticky, balancer->s->sticky_path);
    if (sticky == NULL)
        return 0;

    if (r->filename)
        uri = r->filename + 6;
    else {
        /* We are coming from proxy_cluster_trans */
        uri = r->unparsed_uri;
    }

    sessionid = cluster_get_sessionid(r, sticky, uri, &sticky_used);
    if (sessionid) {
#if HAVE_CLUSTER_EX_DEBUG
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "mod_proxy_cluster: found sessionid %s", sessionid);
#endif
        return 1;
    }
    return 0;
}

/**
 * Find the best nodes for a request (check host and context (and balancer))
 * @param r the request_rec
 * @param balancer the balancer (balancer to use in that case we check it).
 * @param route from the sessionid if we have one.
 * @param use_alias compare alias with server_name
 * @return a pointer to a list of nodes.
 */
node_context *find_node_context_host(request_rec *r, proxy_balancer *balancer, const char *route, int use_alias,
                                     proxy_vhost_table *vhost_table, proxy_context_table *context_table,
                                     proxy_node_table *node_table)
{
    int sizecontext = context_table->sizecontext;
    int *contexts;
    int *length;
    int *status;
    int i, j, max;
    node_context *best;
    int nbest;
    const char *uri = NULL;
    const char *luri = NULL;

    /* use r->uri (trans) or r->filename (after canon or rewrite) */
    if (r->filename) {
        const char *scheme = strstr(r->filename, "://");
        if (scheme)
            luri = ap_strchr_c(scheme + 3, '/');
    }
    if (!luri)
        luri = r->uri;
    uri = ap_strchr_c(luri, '?');
    if (uri)
        uri = apr_pstrndup(r->pool, luri, uri - luri);
    else {
        uri = ap_strchr_c(luri, ';');
        if (uri)
            uri = apr_pstrndup(r->pool, luri, uri - luri);
        else
            uri = luri;
    }

    /* read the contexts */
    if (sizecontext == 0)
        return NULL;
    contexts = apr_palloc(r->pool, sizeof(int) * sizecontext);
    for (i = 0; i < sizecontext; i++)
        contexts[i] = i;
    length = apr_pcalloc(r->pool, sizeof(int) * sizecontext);
    status = apr_palloc(r->pool, sizeof(int) * sizecontext);
    /* Check the virtual host */
    if (use_alias) {
        /* read the hosts */
        int sizevhost;
        int *contextsok = apr_pcalloc(r->pool, sizeof(int) * sizecontext);
        const char *hostname = ap_get_server_name(r);
#if HAVE_CLUSTER_EX_DEBUG
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "find_node_context_host: Host: %s", hostname);
#endif
        sizevhost = vhost_table->sizevhost;
        for (i = 0; i < sizevhost; i++) {
            hostinfo_t *vhost = vhost_table->vhost_info + i;
            if (strcmp(hostname, vhost->host) == 0) {
                /* add the contexts that match */
                for (j = 0; j < sizecontext; j++) {
                    contextinfo_t *context = &context_table->context_info[j];
                    if (context->vhost == vhost->vhost && context->node == vhost->node)
                        contextsok[j] = 1;
                }
            }
        }
        for (j = 0; j < sizecontext; j++) {
            if (!contextsok[j])
                contexts[j] = -1;
        }
    }
#if HAVE_CLUSTER_EX_DEBUG
    for (j = 0; j < sizecontext; j++) {
        contextinfo_t *context;
        if (contexts[j] == -1)
            continue;
        context = &context_table->context_info[j];
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "find_node_context_host: %s node: %d vhost: %d context: %s",
                     uri, context->node, context->vhost, context->context);
    }
#endif

    /* Check the contexts */
    max = 0;
    for (j = 0; j < sizecontext; j++) {
        contextinfo_t *context;
        int len;
        if (contexts[j] == -1)
            continue;
        context = &context_table->context_info[j];

        /* keep only the contexts corresponding to our balancer */
        if (balancer != NULL) {

            nodeinfo_t *node = table_get_node(node_table, context->node);
            if (node == NULL)
                continue;
            if (strlen(balancer->s->name) > 11 && strcasecmp(&balancer->s->name[11], node->mess.balancer) != 0)
                continue;
        }
        len = strlen(context->context);
        if (strncmp(uri, context->context, len) == 0) {
            if (uri[len] == '\0' || uri[len] == '/' || len == 1) {
                status[j] = context->status;
                length[j] = len;
                if (len > max) {
                    max = len;
                }
            }
        }
    }
    if (max == 0)
        return NULL;


    /* find the best matching contexts */
    nbest = 1;
    for (j = 0; j < sizecontext; j++)
        if (length[j] == max)
            nbest++;
    best = apr_palloc(r->pool, sizeof(node_context) * nbest);
    nbest = 0;
    for (j = 0; j < sizecontext; j++)
        if (length[j] == max) {
            contextinfo_t *context;
            int ok = 0;
            context = &context_table->context_info[j];
            /* Check status */
            switch (status[j]) {
            case ENABLED:
                ok = -1;
                break;
            case DISABLED:
                /* Only the request with sessionid ok for it */
                if (hassession_byname(r, context->node, route, node_table)) {
                    ok = -1;
                }
                break;
            }
            if (ok) {
                best[nbest].node = context->node;
                best[nbest].context = context->id;
                nbest++;
            }
        }
    if (nbest == 0)
        return NULL;
    best[nbest].node = -1;
    return best;
}

/* Given the route find the corresponding domain (if there is a domain) */
static apr_status_t find_nodedomain(request_rec *r, char **domain, char *route, const char *balancer,
                                    proxy_node_table *node_table)
{
    int i;
    (void)r;

    /* XXX JFCLERE!!!! domaininfo_t *dom; */
#if HAVE_CLUSTER_EX_DEBUG
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "find_nodedomain: finding node for %s: %s", route, balancer);
#endif
    for (i = 0; i < node_table->sizenode; i++) {
        if (strcmp(node_table->node_info[i].mess.JVMRoute, route) == 0) {
            nodeinfo_t *ou = &node_table->node_info[i];
            if (!strcasecmp(balancer, ou->mess.balancer)) {
                if (ou->mess.Domain[0] != '\0') {
                    *domain = ou->mess.Domain;
                }
                return APR_SUCCESS;
            }
        }
    }

#if HAVE_CLUSTER_EX_DEBUG
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "find_nodedomain: finding domain for %s: %s", route, balancer);
#endif
    /* We can't find the node, because it was removed... */
    /* XXX: we need a proxy_node_domain for that too!!!
       if (domain_storage->find_domain(&dom, route, balancer ) == APR_SUCCESS) {
       *domain = dom->domain;
       return APR_SUCCESS;
       }
     */
    return APR_NOTFOUND;
}

/**
 * Find the balancer corresponding to the node information
 */
const char *get_route_balancer(request_rec *r, proxy_server_conf *conf, proxy_vhost_table *vhost_table,
                               proxy_context_table *context_table, proxy_balancer_table *balancer_table,
                               proxy_node_table *node_table, int use_alias)
{
    char *route = NULL;
    char *sessionid = NULL;
    char *sticky_used;
    char *sticky;
    int i;
    char *ptr = conf->balancers->elts;
    int sizeb = conf->balancers->elt_size;
    (void)balancer_table;

    for (i = 0; i < conf->balancers->nelts; i++, ptr = ptr + sizeb) {
        proxy_balancer *balancer = (proxy_balancer *)ptr;

        if (balancer->s->sticky[0] == '\0' || balancer->s->sticky_path[0] == '\0')
            continue;
        if (strlen(balancer->s->name) <= 11)
            continue;
        sticky = apr_psprintf(r->pool, "%s|%s", balancer->s->sticky, balancer->s->sticky_path);
        /* XXX ; that looks fishy, lb needs to start with MC? */
        if (strncmp(balancer->s->lbpname, "MC", 2))
            continue;

        sessionid = cluster_get_sessionid(r, sticky, r->uri, &sticky_used);
        if (sessionid) {
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                         "cluster: %s Found value %s for "
                         "stickysession %s",
                         balancer->s->name, sessionid, sticky);
            apr_table_setn(r->notes, "session-id", sessionid);
            if ((route = strchr(sessionid, '.')) != NULL)
                route++;
            if (route && *route) {
                /* Nice we have a route, but make sure we have to serve it */
                node_context *nodes =
                    find_node_context_host(r, balancer, route, use_alias, vhost_table, context_table, node_table);
                if (nodes == NULL)
                    continue; /* we can't serve context/host for the request with this balancer */
            }
            if (route && *route) {
                char *domain = NULL;
#if HAVE_CLUSTER_EX_DEBUG
                ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "cluster: Found route %s", route);
#endif
                if (find_nodedomain(r, &domain, route, &balancer->s->name[11], node_table) == APR_SUCCESS) {
#if HAVE_CLUSTER_EX_DEBUG
                    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "cluster: Found balancer %s for %s",
                                 &balancer->s->name[11], route);
#endif
                    /* here we have the route and domain for find_session_route ... */
                    apr_table_setn(r->notes, "session-sticky", sticky_used);
                    apr_table_setn(r->notes, "session-route", route);

                    apr_table_setn(r->subprocess_env, "BALANCER_SESSION_ROUTE", route);
                    apr_table_setn(r->subprocess_env, "BALANCER_SESSION_STICKY", sticky_used);
                    if (domain) {
#if HAVE_CLUSTER_EX_DEBUG
                        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "cluster: Found domain %s for %s", domain,
                                     route);
#endif
                        apr_table_setn(r->notes, "CLUSTER_DOMAIN", domain);
                    }
                    return &balancer->s->name[11];
                }
            }
        }
    }
    return NULL;
}

/* Read a node from the table using its it */
nodeinfo_t *table_get_node(proxy_node_table *node_table, int id)
{
    int i;
    for (i = 0; i < node_table->sizenode; i++) {
        if (node_table->nodes[i] == id)
            return &node_table->node_info[i];
    }
    return NULL;
}

/* Read a node from the table using the route */
nodeinfo_t *table_get_node_route(proxy_node_table *node_table, char *route, int *id)
{
    int i;
    for (i = 0; i < node_table->sizenode; i++) {
        if (!strcmp(node_table->node_info[i].mess.JVMRoute, route)) {
            *id = node_table->nodes[i];
            return &node_table->node_info[i];
        }
    }
    return NULL;
}

/**
 * Search the balancer that corresponds to the pair context/host
 * @param r the request_rec.
 * @vhost_table table of host virtual hosts.
 * @context_table table of contexts.
 * @return the balancer name or NULL if not found.
 */
const char *get_context_host_balancer(request_rec *r, proxy_vhost_table *vhost_table,
                                      proxy_context_table *context_table, proxy_node_table *node_table, int use_alias)
{
    void *sconf = r->server->module_config;
    proxy_server_conf *conf = (proxy_server_conf *)ap_get_module_config(sconf, &proxy_module);

    node_context *nodes = find_node_context_host(r, NULL, NULL, use_alias, vhost_table, context_table, node_table);
    if (nodes == NULL)
        return NULL;
    while (nodes->node != -1) {
        /* look for the node information */
        nodeinfo_t *node = table_get_node(node_table, nodes->node);
        if (node != NULL) {
            if (node->mess.balancer[0] != '\0') {
                /* Check that it is in our proxy_server_conf */
                char *name = apr_pstrcat(r->pool, "balancer://", node->mess.balancer, NULL);
                proxy_balancer *balancer = ap_proxy_get_balancer(r->pool, conf, name, 0);
                if (balancer)
                    return node->mess.balancer;
                else
                    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                                 "get_context_host_balancer: balancer %s not found", name);
            }
        }
        nodes++;
    }
    return NULL;
}


/*
 * Return the node cotenxt Check that the worker will handle the host/context.
 * de
 * The id of the worker is used to find the (slot) node in the shared
 * memory.
 * (See get_context_host_balancer too).
 */
node_context *context_host_ok(request_rec *r, proxy_balancer *balancer, int node, int use_alias,
                              proxy_vhost_table *vhost_table, proxy_context_table *context_table,
                              proxy_node_table *node_table)
{
    const char *route;
    node_context *best;
    route = apr_table_get(r->notes, "session-route");
    best = find_node_context_host(r, balancer, route, use_alias, vhost_table, context_table, node_table);
    if (best == NULL)
        return NULL;
    while (best->node != -1) {
        if (best->node == node)
            break;
        best++;
    }
    if (best->node == -1)
        return NULL; /* not found */
    return best;
}
