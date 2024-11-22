/*
 * Copyright The mod_cluster Project Authors
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file  host.c
 * @brief host description Storage Module for Apache
 * @author Jean-Frederic Clere
 *
 * @defgroup MEM hosts
 * @ingroup  APACHE_MODS
 * @{
 */

#include "mod_manager.h"
#include "host.h"


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
    } else {
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

apr_status_t get_host(mem_t *s, hostinfo_t **host, int id)
{
    return s->storage->dptr(s->slotmem, id, (void **)host);
}

apr_status_t remove_host(mem_t *s, int id)
{
    return s->storage->release(s->slotmem, id);
}

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

int get_max_size_host(mem_t *s)
{
    return s->storage->num_slots(s->slotmem);
}

mem_t *get_mem_host(char *string, unsigned *num, apr_pool_t *p, slotmem_storage_method *storage)
{
    return create_attach_mem_host(string, num, 0, 0, p, storage);
}

mem_t *create_mem_host(char *string, unsigned *num, int persist, apr_pool_t *p, slotmem_storage_method *storage)
{
    return create_attach_mem_host(string, num, persist, 1, p, storage);
}
