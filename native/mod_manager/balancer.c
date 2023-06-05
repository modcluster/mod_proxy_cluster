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
 * @file  balancer.c
 * @brief balancer description Storage Module for Apache
 *
 * @defgroup MEM balancers
 * @ingroup  APACHE_MODS
 * @{
 */

#include "apr.h"
#include "apr_strings.h"
#include "apr_pools.h"
#include "apr_time.h"

#include "ap_slotmem.h"
#include "balancer.h"

#include "mod_manager.h"

static mem_t *create_attach_mem_balancer(char *string, unsigned *num, int type, int create, apr_pool_t *p,
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
    storename = apr_pstrcat(p, string, BALANCEREXE, NULL);
    if (create) {
        rv = ptr->storage->create(&ptr->slotmem, storename, sizeof(balancerinfo_t), *num, type, p);
    }
    else {
        apr_size_t size = sizeof(balancerinfo_t);
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
 * Update a balancer record in the shared table
 * @param pointer to the shared table.
 * @param balancer balancer to store in the shared table.
 * @return APR_EEXIST if the record was updated, APR_SUCCESS otherwise
 *
 */
static apr_status_t update(void *mem, void *data, apr_pool_t *pool)
{
    balancerinfo_t *in = (balancerinfo_t *)data;
    balancerinfo_t *ou = (balancerinfo_t *)mem;
    (void)pool;

    if (strcmp(in->balancer, ou->balancer) == 0) {
        in->id = ou->id;
        memcpy(ou, in, sizeof(balancerinfo_t));
        ou->updatetime = apr_time_sec(apr_time_now());
        return APR_EEXIST; /* it exists so we are done */
    }
    return APR_SUCCESS;
}

apr_status_t insert_update_balancer(mem_t *s, balancerinfo_t *balancer)
{
    apr_status_t rv;
    balancerinfo_t *ou;
    unsigned id = 0;

    rv = s->storage->doall(s->slotmem, update, balancer, s->p);
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
    memcpy(ou, balancer, sizeof(balancerinfo_t));
    ou->id = id;
    ou->updatetime = apr_time_sec(apr_time_now());

    return APR_SUCCESS;
}

/**
 * read a balancer record from the shared table
 * @param pointer to the shared table.
 * @param balancer balancer to read from the shared table.
 * @return address of the read balancer or NULL if error.
 */
static apr_status_t loc_read_balancer(void *mem, void *data, apr_pool_t *pool)
{
    balancerinfo_t *in = (balancerinfo_t *)data;
    balancerinfo_t *ou = (balancerinfo_t *)mem;
    (void)pool;

    if (strcmp(in->balancer, ou->balancer) == 0) {
        in->id = ou->id;
        return APR_EEXIST;
    }
    return APR_SUCCESS;
}

balancerinfo_t *read_balancer(mem_t *s, balancerinfo_t *balancer)
{
    apr_status_t rv;
    balancerinfo_t *ou;

    if (!balancer->id) {
        rv = s->storage->doall(s->slotmem, loc_read_balancer, balancer, s->p);
        if (rv != APR_EEXIST) {
            return NULL;
        }
    }
    rv = s->storage->dptr(s->slotmem, balancer->id, (void **)&ou);
    if (rv == APR_SUCCESS) {
        return ou;
    }
    return NULL;
}

/**
 * get a balancer record from the shared table
 * @param pointer to the shared table.
 * @param balancer address where the balancer is locate in the shared table.
 * @param id  in the balancer table.
 * @return APR_SUCCESS if all went well
 */
apr_status_t get_balancer(mem_t *s, balancerinfo_t **balancer, int id)
{
    return s->storage->dptr(s->slotmem, id, (void **)balancer);
}

/**
 * remove(free) a balancer record from the shared table
 * @param pointer to the shared table.
 * @param balancer balancer to remove from the shared table.
 * @return APR_SUCCESS if all went well
 */
apr_status_t remove_balancer(mem_t *s, balancerinfo_t *balancer)
{
    apr_status_t rv;
    balancerinfo_t *ou = balancer;
    if (balancer->id) {
        rv = s->storage->release(s->slotmem, balancer->id);
    }
    else {
        /* XXX: for the moment January 2007 ap_slotmem_free only uses ident to remove */
        rv = s->storage->doall(s->slotmem, loc_read_balancer, &ou, s->p);
        if (rv == APR_EEXIST) {
            rv = s->storage->release(s->slotmem, ou->id);
        }
    }
    return rv;
}

static apr_status_t loc_get_id(void *mem, void *data, apr_pool_t *pool)
{
    struct counter *count = (struct counter *)data;
    balancerinfo_t *ou = (balancerinfo_t *)mem;
    (void)pool;
    *count->values = ou->id;
    count->values++;
    count->count++;
    return APR_SUCCESS;
}

/*
 * get the ids for the used (not free) balancers in the table
 * @param pointer to the shared table.
 * @param ids array of int to store the used id (must be big enough).
 * @return number of balancer existing or 0.
 */
int get_ids_used_balancer(mem_t *s, int *ids)
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
 * @return the max number of balancers that the slotmem can contain.
 */
int get_max_size_balancer(mem_t *s)
{
    return s->storage->num_slots(s->slotmem);
}

/**
 * attach to the shared balancer table
 * @param name of an existing shared table.
 * @param address to store the size of the shared table.
 * @param p pool to use for allocations.
 * @param storage slotmem logic provider.
 * @return address of struct used to access the table.
 */
mem_t *get_mem_balancer(char *string, unsigned *num, apr_pool_t *p, slotmem_storage_method *storage)
{
    return create_attach_mem_balancer(string, num, 0, 0, p, storage);
}

/**
 * create a shared balancer table
 * @param name to use to create the table.
 * @param size of the shared table.
 * @param persist tell if the slotmem element are persistent.
 * @param p pool to use for allocations.
 * @param storage slotmem logic provider.
 * @return address of struct used to access the table.
 */
mem_t *create_mem_balancer(char *string, unsigned *num, int persist, apr_pool_t *p, slotmem_storage_method *storage)
{
    return create_attach_mem_balancer(string, num, persist, 1, p, storage);
}
