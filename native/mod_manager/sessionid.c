/*
 * Copyright The mod_cluster Project Authors
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file  sessionid.c
 * @brief sessionid description Storage Module for Apache
 * @author Jean-Frederic Clere
 *
 * @defgroup MEM sessionids
 * @ingroup  APACHE_MODS
 * @{
 */

#include "mod_manager.h"
#include "sessionid.h"


static mem_t *create_attach_mem_sessionid(char *string, unsigned *num, int type, int create, apr_pool_t *p,
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
    storename = apr_pstrcat(p, string, SESSIONIDEXE, NULL);
    if (create) {
        rv = ptr->storage->create(&ptr->slotmem, storename, sizeof(sessionidinfo_t), *num, type, p);
    } else {
        apr_size_t size = sizeof(sessionidinfo_t);
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
 * Update a sessionid record in the shared table
 * @param mem pointer to the shared table
 * @param data sessionid to store in the shared table
 * @param pool unused argument
 * @return APR_EEXIST if the record was updated, APR_SUCCESS otherwise
 */
static apr_status_t update(void *mem, void *data, apr_pool_t *pool)
{
    sessionidinfo_t *in = (sessionidinfo_t *)data;
    sessionidinfo_t *ou = (sessionidinfo_t *)mem;
    (void)pool;

    if (strcmp(in->sessionid, ou->sessionid) == 0) {
        in->id = ou->id;
        memcpy(ou, in, sizeof(sessionidinfo_t));
        ou->updatetime = apr_time_sec(apr_time_now());
        return APR_EEXIST; /* it exists so we are done */
    }
    return APR_SUCCESS;
}

apr_status_t insert_update_sessionid(mem_t *s, sessionidinfo_t *sessionid)
{
    apr_status_t rv;
    sessionidinfo_t *ou;
    unsigned id = 0;

    rv = s->storage->doall(s->slotmem, update, sessionid, s->p);
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
    memcpy(ou, sessionid, sizeof(sessionidinfo_t));
    ou->id = id;
    ou->updatetime = apr_time_sec(apr_time_now());

    return APR_SUCCESS;
}

/**
 * read a sessionid record from the shared table
 * @param pointer to the shared table.
 * @param sessionid sessionid to read from the shared table.
 * @return address of the read sessionid or NULL if error.
 */
static apr_status_t loc_read_sessionid(void *mem, void *data, apr_pool_t *pool)
{
    sessionidinfo_t *in = (sessionidinfo_t *)data;
    sessionidinfo_t *ou = (sessionidinfo_t *)mem;
    (void)pool;

    if (strcmp(in->sessionid, ou->sessionid) == 0) {
        in->id = ou->id;
        return APR_EEXIST;
    }
    return APR_SUCCESS;
}

sessionidinfo_t *read_sessionid(mem_t *s, sessionidinfo_t *sessionid)
{
    apr_status_t rv;
    sessionidinfo_t *ou;

    if (!sessionid->id) {
        rv = s->storage->doall(s->slotmem, loc_read_sessionid, sessionid, s->p);
        if (rv != APR_EEXIST) {
            return NULL;
        }
    }
    rv = s->storage->dptr(s->slotmem, sessionid->id, (void **)&ou);
    if (rv == APR_SUCCESS) {
        return ou;
    }
    return NULL;
}

apr_status_t get_sessionid(mem_t *s, sessionidinfo_t **sessionid, int id)
{
    return s->storage->dptr(s->slotmem, id, (void **)sessionid);
}

apr_status_t remove_sessionid(mem_t *s, sessionidinfo_t *sessionid)
{
    apr_status_t rv;
    sessionidinfo_t *ou = sessionid;
    if (sessionid->id) {
        rv = s->storage->release(s->slotmem, sessionid->id);
    } else {
        /* XXX: for the moment January 2007 ap_slotmem_free only uses ident to remove */
        rv = s->storage->doall(s->slotmem, loc_read_sessionid, &ou, s->p);
        if (rv == APR_EEXIST) {
            rv = s->storage->release(s->slotmem, ou->id);
        }
    }
    return rv;
}

int get_ids_used_sessionid(mem_t *s, int *ids)
{
    struct counter count;
    count.count = 0;
    count.values = ids;
    if (s->storage->doall(s->slotmem, loc_get_id, &count, s->p) != APR_SUCCESS) {
        return 0;
    }
    return count.count;
}

int get_max_size_sessionid(mem_t *s)
{
    return s->storage->num_slots(s->slotmem);
}

mem_t *get_mem_sessionid(char *string, unsigned *num, apr_pool_t *p, slotmem_storage_method *storage)
{
    return create_attach_mem_sessionid(string, num, 0, 0, p, storage);
}

mem_t *create_mem_sessionid(char *string, unsigned *num, int persist, apr_pool_t *p, slotmem_storage_method *storage)
{
    return create_attach_mem_sessionid(string, num, persist, 1, p, storage);
}
