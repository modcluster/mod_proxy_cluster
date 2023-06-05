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

/**
 * @file  host.c
 * @brief host description Storage Module for Apache
 *
 * @defgroup MEM hosts
 * @ingroup  APACHE_MODS
 * @{
 */

#include "apr.h"
#include "apr_strings.h"
#include "apr_pools.h"
#include "apr_time.h"

#include "ap_slotmem.h"
#include "host.h"

#include "mod_manager.h"

static mem_t *create_attach_mem_host(char *string, unsigned *num, int type, int create, apr_pool_t *p,
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
    storename = apr_pstrcat(p, string, HOSTEXE, NULL);
    if (create) {
        rv = ptr->storage->create(&ptr->slotmem, storename, sizeof(hostinfo_t), *num, type, p);
    }
    else {
        apr_size_t size = sizeof(hostinfo_t);
        rv = ptr->storage->attach(&ptr->slotmem, storename, &size, num, p);
    }
    if (rv != APR_SUCCESS) {
        return NULL;
    }
    ptr->num = *num;
    ptr->p = p;
    return ptr;
}

/**
 * Insert(alloc) and update a host record in the shared table
 * @param pointer to the shared table.
 * @param host host to store in the shared table.
 * @return APR_EEXIST if the record was updated, APR_SUCCESS otherwise
 *
 */
static apr_status_t update(void *mem, void *data, apr_pool_t *pool)
{
    hostinfo_t *in = (hostinfo_t *)data;
    hostinfo_t *ou = (hostinfo_t *)mem;
    (void)pool;

    if (strcmp(in->host, ou->host) == 0 && in->vhost == ou->vhost && in->node == ou->node) {
        in->id = ou->id;
        memcpy(ou, in, sizeof(hostinfo_t));
        ou->updatetime = apr_time_sec(apr_time_now());
        return APR_EEXIST; /* it exists so we are done */
    }
    return APR_SUCCESS;
}

apr_status_t insert_update_host(mem_t *s, hostinfo_t *host)
{
    apr_status_t rv;
    hostinfo_t *ou;
    unsigned id = 0;

    rv = s->storage->doall(s->slotmem, update, host, s->p);
    if (rv == APR_EEXIST) {
        return APR_SUCCESS; /* updated */
    }

    /* we have to insert it */
    rv = s->storage->grab(s->slotmem, &id);
    if (rv != APR_SUCCESS) {
        return rv;
    }
    rv = s->storage->dptr(s->slotmem, id, (void **)&ou);
    if (rv != APR_SUCCESS) {
        return rv;
    }
    memcpy(ou, host, sizeof(hostinfo_t));
    ou->id = id;
    ou->updatetime = apr_time_sec(apr_time_now());

    return APR_SUCCESS;
}

/**
 * read a host record from the shared table
 * @param pointer to the shared table.
 * @param host host to read from the shared table.
 * @return address of the read host or NULL if error.
 */
static apr_status_t loc_read_host(void *mem, void *data, apr_pool_t *pool)
{
    hostinfo_t *in = (hostinfo_t *)data;
    hostinfo_t *ou = (hostinfo_t *)mem;
    (void)pool;

    if (strcmp(in->host, ou->host) == 0 && in->node == ou->node) {
        in->id = ou->id;
        return APR_EEXIST;
    }
    return APR_SUCCESS;
}

hostinfo_t *read_host(mem_t *s, hostinfo_t *host)
{
    apr_status_t rv;
    hostinfo_t *ou;

    if (!host->id) {
        rv = s->storage->doall(s->slotmem, loc_read_host, host, s->p);
        if (rv != APR_EEXIST) {
            return NULL;
        }
    }
    rv = s->storage->dptr(s->slotmem, host->id, (void **)&ou);
    if (rv == APR_SUCCESS) {
        return ou;
    }

    return NULL;
}

/**
 * get a host record from the shared table
 * @param pointer to the shared table.
 * @param host address where the host is locate in the shared table.
 * @param id  in the host table.
 * @return APR_SUCCESS if all went well
 */
apr_status_t get_host(mem_t *s, hostinfo_t **host, int id)
{
    return s->storage->dptr(s->slotmem, id, (void **)host);
}

/**
 * remove(free) a host record from the shared table
 * @param pointer to the shared table.
 * @param id id of the host to remove from the shared table.
 * @return APR_SUCCESS if all went well
 */
apr_status_t remove_host(mem_t *s, int id)
{
    return s->storage->release(s->slotmem, id);
}

static apr_status_t loc_get_id(void *mem, void *data, apr_pool_t *pool)
{
    struct counter *count = (struct counter *)data;
    hostinfo_t *ou = (hostinfo_t *)mem;
    *count->values = ou->id;
    (void)pool;
    count->values++;
    count->count++;
    return APR_SUCCESS;
}

/*
 * get the ids for the used (not free) hosts in the table
 * @param pointer to the shared table.
 * @param ids array of int to store the used id (must be big enough).
 * @return number of host existing or 0.
 */
int get_ids_used_host(mem_t *s, int *ids)
{
    struct counter count;
    count.count = 0;
    count.values = ids;
    if (s->storage->doall(s->slotmem, loc_get_id, &count, s->p) != APR_SUCCESS) {
        return 0;
    }
    return count.count;
}

/*
 * read the size of the table.
 * @param pointer to the shared table.
 * @return the max number of host that the slotmem can contain.
 */
int get_max_size_host(mem_t *s)
{
    return s->storage->num_slots(s->slotmem);
}

/**
 * attach to the shared host table
 * @param name of an existing shared table.
 * @param address to store the size of the shared table.
 * @param p pool to use for allocations.
 * @return address of struct used to access the table.
 */
mem_t *get_mem_host(char *string, unsigned *num, apr_pool_t *p, slotmem_storage_method *storage)
{
    return create_attach_mem_host(string, num, 0, 0, p, storage);
}

/**
 * create a shared host table
 * @param name to use to create the table.
 * @param size of the shared table.
 * @param persist tell if the slotmem element are persistent.
 * @param p pool to use for allocations.
 * @return address of struct used to access the table.
 */
mem_t *create_mem_host(char *string, unsigned *num, int persist, apr_pool_t *p, slotmem_storage_method *storage)
{
    return create_attach_mem_host(string, num, persist, 1, p, storage);
}
