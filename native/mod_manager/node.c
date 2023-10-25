/*
 *  mod_cluster.
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

/**
 * @file  node.c
 * @brief node description Storage Module for Apache
 *
 * @defgroup MEM nodes
 * @ingroup  APACHE_MODS
 * @{
 */

#include "apr.h"
#include "apr_strings.h"
#include "apr_pools.h"
#include "apr_time.h"

/* for debug */
#include "httpd.h"
#include "http_main.h"

#include "ap_slotmem.h"
#include "node.h"

#include "mod_manager.h"

#include "common.h"

static mem_t *create_attach_mem_node(char *string, unsigned *num, int type, int create, apr_pool_t *p,
                                     slotmem_storage_method *storage)
{
    mem_t *ptr;
    const char *storename;
    apr_status_t rv;

    ptr = apr_pcalloc(p, sizeof(mem_t));
    if (!ptr) {
        return NULL;
    }
    ptr->storage = storage;
    storename = apr_pstrcat(p, string, NODEEXE, NULL);
    if (create) {
        rv = ptr->storage->create(&ptr->slotmem, storename, sizeof(nodeinfo_t), *num, type, p);
    } else {
        apr_size_t size = sizeof(nodeinfo_t);
        rv = ptr->storage->attach(&ptr->slotmem, storename, &size, num, p);
    }
    if (rv != APR_SUCCESS) {
        ptr->laststatus = rv;
        return ptr;
    }
    ptr->laststatus = APR_SUCCESS;
    ptr->num = *num;
    ptr->p = p;
    return ptr;
}

apr_status_t get_last_mem_error(mem_t *mem)
{
    return mem->laststatus;
}


/**
 * Update a node record in the shared table
 * @param mem pointer to the shared table
 * @param data node to store in the shared table
 * @param p unused argument
 * @return APR_EEXIST if the record was updated, APR_SUCCESS otherwise
 */
static apr_status_t update(void *mem, void *data, apr_pool_t *pool)
{
    nodeinfo_t *in = (nodeinfo_t *)data;
    nodeinfo_t *ou = (nodeinfo_t *)mem;
    (void)pool;

    if (strcmp(in->mess.JVMRoute, ou->mess.JVMRoute) == 0) {
        /*
         * The node information is made of several pieces:
         * Information from the cluster (nodemess_t).
         * updatetime (time of last received message).
         * offset (of the area shared with the proxy logic).
         * stat (shared area with the proxy logic we shouldn't modify it here).
         */
        in->mess.id = ou->mess.id;
        memcpy(ou, in, sizeof(nodemess_t));
        ou->updatetime = apr_time_now();
        ou->offset = sizeof(nodemess_t) + sizeof(apr_time_t) + sizeof(int);
        ou->offset = APR_ALIGN_DEFAULT(ou->offset);
        return APR_EEXIST; /* it exists so we are done */
    }
    return APR_SUCCESS;
}

apr_status_t insert_update_node(mem_t *s, nodeinfo_t *node, int *id, int clean)
{
    apr_status_t rv;
    nodeinfo_t *ou;
    apr_time_t now;

    now = apr_time_now();
    rv = s->storage->doall(s->slotmem, update, node, s->p);
    if (rv == APR_EEXIST) {
        *id = node->mess.id;
        return APR_SUCCESS; /* updated */
    }

    /*
     * we have to insert it, there are 2 cases:
     *  *id == -1 we have to find where to put it.
     *  *id != -1 we know where to put it.
     */
    if (*id == -1) {
        unsigned tmp_id;
        rv = s->storage->grab(s->slotmem, &tmp_id);
        if (rv != APR_SUCCESS) {
            return rv;
        }
        *id = (int)tmp_id;
    } else {
        rv = s->storage->fgrab(s->slotmem, *id);
        if (rv != APR_SUCCESS) {
            return rv;
        }
    }
    rv = s->storage->dptr(s->slotmem, *id, (void **)&ou);
    if (rv != APR_SUCCESS) {
        return rv;
    }

    memcpy(ou, node, sizeof(nodeinfo_t));
    ou->mess.id = *id;
    ou->updatetime = now;

    /* set of offset to the proxy_worker_stat */
    ou->offset = sizeof(nodemess_t) + sizeof(apr_time_t) + sizeof(int);
    ou->offset = APR_ALIGN_DEFAULT(ou->offset);

    /* blank the proxy status information */
    if (clean) {
        memset(&(ou->stat), '\0', SIZEOFSCORE);
    }

    return APR_SUCCESS;
}

/**
 * Read a node record from the shared table
 * @param mem pointer to the shared table
 * @param data node node to read from the shared table
 * @return address of the read node or NULL if error
 */
static apr_status_t loc_read_node(void *mem, void *data, apr_pool_t *pool)
{
    nodeinfo_t *in = (nodeinfo_t *)data;
    nodeinfo_t *ou = (nodeinfo_t *)mem;
    (void)pool;

    if (strcmp(in->mess.JVMRoute, ou->mess.JVMRoute) == 0) {
        in->mess.id = ou->mess.id;
        return APR_EEXIST;
    }
    return APR_SUCCESS;
}

nodeinfo_t *read_node(mem_t *s, nodeinfo_t *node)
{
    apr_status_t rv;
    nodeinfo_t *ou;

    if (node->mess.id == -1) {
        rv = s->storage->doall(s->slotmem, loc_read_node, node, s->p);
        if (rv == APR_EEXIST) {
            return node;
        }
    } else {
        rv = s->storage->dptr(s->slotmem, node->mess.id, (void **)&ou);
        if (rv == APR_SUCCESS) {
            return ou;
        }
    }
    return NULL;
}

apr_status_t get_node(mem_t *s, nodeinfo_t **node, int ids)
{
    return s->storage->dptr(s->slotmem, ids, (void **)node);
}

apr_status_t remove_node(mem_t *s, int id)
{
    return s->storage->release(s->slotmem, id);
}

apr_status_t find_node(mem_t *s, nodeinfo_t **node, const char *route)
{
    nodeinfo_t ou;
    apr_status_t rv;

    strncpy(ou.mess.JVMRoute, route, sizeof(ou.mess.JVMRoute));
    ou.mess.JVMRoute[sizeof(ou.mess.JVMRoute) - 1] = '\0';
    rv = s->storage->doall(s->slotmem, loc_read_node, &ou, s->p);
    if (rv == APR_SUCCESS) {
        return APR_NOTFOUND;
    }
    if (rv == APR_EEXIST) {
        rv = s->storage->dptr(s->slotmem, ou.mess.id, (void **)node);
    }
    return rv;
}

int get_ids_used_node(mem_t *s, int *ids)
{
    struct counter count;
    count.count = 0;
    count.values = ids;
    if (s->storage->doall(s->slotmem, loc_get_id, &count, s->p) != APR_SUCCESS) {
        return 0;
    }
    return count.count;
}

int get_max_size_node(mem_t *s)
{
    return s->storage == NULL ? 0 : s->storage->num_slots(s->slotmem);
}

/**
 * Attach to the shared node table
 * @param string name of an existing shared table
 * @param num address to store the size of the shared table
 * @param p pool to use for allocations
 * @param storage slotmem logic provider
 * @return address of struct used to access the table
 */
mem_t *get_mem_node(char *string, unsigned *num, apr_pool_t *p, slotmem_storage_method *storage)
{
    return create_attach_mem_node(string, num, 0, 0, p, storage);
}

mem_t *create_mem_node(char *string, unsigned *num, int persist, apr_pool_t *p, slotmem_storage_method *storage)
{
    return create_attach_mem_node(string, num, (unsigned)persist, 1, p, storage);
}

static apr_status_t loc_read_node_byhostport(void *mem, void *data, apr_pool_t *pool)
{
    nodeinfo_t *in = (nodeinfo_t *)data;
    nodeinfo_t *ou = (nodeinfo_t *)mem;
    (void)pool;

    if (strcmp(in->mess.Host, ou->mess.Host) == 0 && strcmp(in->mess.Port, ou->mess.Port) == 0) {
        in->mess.id = ou->mess.id;
        return APR_EEXIST;
    }
    return APR_SUCCESS;
}

/**
 * Find the first node that corresponds to host/port
 * @param s pointer to the shared table
 * @param node node to read from the shared table
 * @param host string containing the host
 * @param port string containing the port
 * @return APR_SUCCESS if all went well
 */
apr_status_t find_node_byhostport(mem_t *s, nodeinfo_t **node, const char *host, const char *port)
{
    nodeinfo_t ou;
    apr_status_t rv;

    strncpy(ou.mess.Host, host, sizeof(ou.mess.Host));
    ou.mess.Host[sizeof(ou.mess.Host) - 1] = '\0';
    strncpy(ou.mess.Port, port, sizeof(ou.mess.Port));
    ou.mess.Port[sizeof(ou.mess.Port) - 1] = '\0';

    rv = s->storage->doall(s->slotmem, loc_read_node_byhostport, &ou, s->p);
    if (rv == APR_EEXIST) {
        rv = s->storage->dptr(s->slotmem, ou.mess.id, (void **)node);
        return rv;
    }
    if (rv == APR_SUCCESS) {
        return APR_NOTFOUND;
    }
    return rv;
}
