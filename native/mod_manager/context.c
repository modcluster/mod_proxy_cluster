/*
 * Copyright The mod_cluster Project Authors
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file  context.c
 * @brief context description Storage Module for Apache
 * @author Jean-Frederic Clere
 *
 * @defgroup MEM contexts
 * @ingroup  APACHE_MODS
 * @{
 */

#include "mod_manager.h"


static mem_t *create_attach_mem_context(char *string, unsigned *num, int type, int create, apr_pool_t *p,
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
    storename = apr_pstrcat(p, string, CONTEXTEXE, NULL);
    if (create) {
        rv = ptr->storage->create(&ptr->slotmem, storename, sizeof(contextinfo_t), *num, type, p);
    } else {
        apr_size_t size = sizeof(contextinfo_t);
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
 * Update a context record in the shared table
 * @param mem pointer to the shared table.
 * @param context context to store in the shared table.
 * @param pool unused argument
 * @return APR_EEXIST if the record was updated, APR_SUCCESS otherwise
 */
static apr_status_t update(void *mem, void *data, apr_pool_t *pool)
{
    contextinfo_t *in = (contextinfo_t *)data;
    contextinfo_t *ou = (contextinfo_t *)mem;
    (void)pool;

    if (strcmp(in->context, ou->context) == 0 && in->vhost == ou->vhost && in->node == ou->node) {
        /* We don't update nbrequests it belongs to mod_proxy_cluster logic */
        ou->status = in->status;
        ou->updatetime = apr_time_sec(apr_time_now());
        return APR_EEXIST; /* it exists so we are done */
    }
    return APR_SUCCESS;
}

apr_status_t insert_update_context(mem_t *s, contextinfo_t *context)
{
    apr_status_t rv;
    contextinfo_t *ou;
    unsigned id = 0;

    rv = s->storage->doall(s->slotmem, update, context, s->p);
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
    memcpy(ou, context, sizeof(contextinfo_t));
    ou->id = id;
    ou->nbrequests = 0;
    ou->updatetime = apr_time_sec(apr_time_now());

    return APR_SUCCESS;
}

/**
 * Read a context record from the shared table
 * @param pointer to the shared table.
 * @param context context to read from the shared table.
 * @param pool unused argument
 * @return address of the read context or NULL if error.
 */
static apr_status_t loc_read_context(void *mem, void *data, apr_pool_t *pool)
{
    contextinfo_t *in = (contextinfo_t *)data;
    contextinfo_t *ou = (contextinfo_t *)mem;
    (void)pool;

    if (strcmp(in->context, ou->context) == 0 && in->vhost == ou->vhost && ou->node == in->node) {
        in->id = ou->id;
        return APR_EEXIST;
    }
    return APR_SUCCESS;
}

contextinfo_t *read_context(mem_t *s, contextinfo_t *context)
{
    apr_status_t rv;
    contextinfo_t *ou;

    if (!context->id) {
        rv = s->storage->doall(s->slotmem, loc_read_context, context, s->p);
        if (rv != APR_EEXIST) {
            return NULL;
        }
    }
    rv = s->storage->dptr(s->slotmem, context->id, (void **)&ou);
    if (rv == APR_SUCCESS) {
        return ou;
    }
    return NULL;
}

apr_status_t get_context(mem_t *s, contextinfo_t **context, int id)
{
    return s->storage->dptr(s->slotmem, id, (void **)context);
}

apr_status_t remove_context(mem_t *s, int id)
{
    return s->storage->release(s->slotmem, id);
}


int get_ids_used_context(mem_t *s, int *ids)
{
    struct counter count;
    count.count = 0;
    count.values = ids;
    if (s->storage->doall(s->slotmem, loc_get_id, &count, s->p) != APR_SUCCESS) {
        return 0;
    }
    return count.count;
}

int get_max_size_context(mem_t *s)
{
    return s->storage->num_slots(s->slotmem);
}

mem_t *get_mem_context(char *string, unsigned *num, apr_pool_t *p, slotmem_storage_method *storage)
{
    return create_attach_mem_context(string, num, 0, 0, p, storage);
}

mem_t *create_mem_context(char *string, unsigned *num, int persist, apr_pool_t *p, slotmem_storage_method *storage)
{
    return create_attach_mem_context(string, num, persist, 1, p, storage);
}
