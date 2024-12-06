/*
 * Copyright The mod_cluster Project Authors
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file  domain.c
 * @brief domain description Storage Module for Apache
 * @author Jean-Frederic Clere
 * @defgroup MEM domains
 * @ingroup  APACHE_MODS
 * @{
 */

#include "mod_manager.h"
#include "domain.h"

static mem_t *create_attach_mem_domain(char *string, unsigned *num, int type, int create, apr_pool_t *p,
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
    storename = apr_pstrcat(p, string, DOMAINEXE, NULL);
    if (create) {
        rv = ptr->storage->create(&ptr->slotmem, storename, sizeof(domaininfo_t), *num, type, p);
    } else {
        apr_size_t size = sizeof(domaininfo_t);
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
 * Update a domain record in the shared table
 * @param mem pointer to the shared table
 * @param domain domain to store in the shared table
 * @param pool unused argument
 * @return APR_EEXIST if the record was updated, APR_SUCCESS otherwise
 */
static apr_status_t update(void *mem, void *data, apr_pool_t *pool)
{
    domaininfo_t *in = (domaininfo_t *)data;
    domaininfo_t *ou = (domaininfo_t *)mem;
    (void)pool;

    if (strcmp(in->JVMRoute, ou->JVMRoute) == 0 && strcmp(in->balancer, ou->balancer) == 0) {
        in->id = ou->id;
        memcpy(ou, in, sizeof(domaininfo_t));
        ou->updatetime = apr_time_sec(apr_time_now());
        return APR_EEXIST;
    }
    return APR_SUCCESS;
}

apr_status_t insert_update_domain(mem_t *s, domaininfo_t *domain)
{
    apr_status_t rv;
    domaininfo_t *ou;
    unsigned id = 0;

    rv = s->storage->doall(s->slotmem, update, domain, s->p);
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
    memcpy(ou, domain, sizeof(domaininfo_t));
    ou->id = id;
    ou->updatetime = apr_time_sec(apr_time_now());

    return APR_SUCCESS;
}

/**
 * Read a domain record from the shared table
 * @param mem pointer to the shared table
 * @param domain domain to read from the shared table
 * @param pool unused argument
 * @return address of the read domain or NULL if error
 */
static apr_status_t loc_read_domain(void *mem, void *data, apr_pool_t *pool)
{
    domaininfo_t *in = (domaininfo_t *)data;
    domaininfo_t *ou = (domaininfo_t *)mem;
    (void)pool;

    if (strcmp(in->JVMRoute, ou->JVMRoute) == 0 && strcmp(in->balancer, ou->balancer) == 0) {
        in->id = ou->id;
        return APR_EEXIST;
    }
    return APR_SUCCESS;
}

domaininfo_t *read_domain(mem_t *s, domaininfo_t *domain)
{
    apr_status_t rv;
    domaininfo_t *ou = domain;

    if (!domain->id) {
        rv = s->storage->doall(s->slotmem, loc_read_domain, domain, s->p);
        if (rv != APR_EEXIST) {
            return NULL;
        }
    }
    rv = s->storage->dptr(s->slotmem, domain->id, (void **)&ou);
    if (rv == APR_SUCCESS) {
        return ou;
    }
    return NULL;
}

apr_status_t get_domain(mem_t *s, domaininfo_t **domain, int ids)
{
    return s->storage->dptr(s->slotmem, ids, (void **)domain);
}

apr_status_t remove_domain(mem_t *s, domaininfo_t *domain)
{
    apr_status_t rv;
    domaininfo_t *ou = domain;
    if (domain->id) {
        rv = s->storage->release(s->slotmem, domain->id);
    } else {
        /* XXX: for the moment January 2007 ap_slotmem_free only uses ident to remove */
        rv = s->storage->doall(s->slotmem, loc_read_domain, &ou, s->p);
        if (rv == APR_EEXIST) {
            rv = s->storage->release(s->slotmem, ou->id);
        }
    }
    return rv;
}

apr_status_t find_domain(mem_t *s, domaininfo_t **domain, const char *route, const char *balancer)
{
    domaininfo_t ou;
    apr_status_t rv;

    strncpy(ou.JVMRoute, route, sizeof(ou.JVMRoute));
    ou.JVMRoute[sizeof(ou.JVMRoute) - 1] = '\0';
    strncpy(ou.balancer, balancer, sizeof(ou.balancer));
    ou.balancer[sizeof(ou.balancer) - 1] = '\0';
    *domain = &ou;
    rv = s->storage->doall(s->slotmem, loc_read_domain, domain, s->p);
    if (rv == APR_EEXIST) {
        rv = s->storage->dptr(s->slotmem, ou.id, (void **)domain);
        return rv;
    }
    if (rv == APR_SUCCESS) {
        return APR_NOTFOUND;
    }
    return rv;
}

int get_ids_used_domain(mem_t *s, int *ids)
{
    struct counter count;
    count.count = 0;
    count.values = ids;
    if (s->storage->doall(s->slotmem, loc_get_id, &count, s->p) != APR_SUCCESS) {
        return 0;
    }
    return count.count;
}

int get_max_size_domain(mem_t *s)
{
    return s->storage->num_slots(s->slotmem);
}

mem_t *get_mem_domain(char *string, unsigned *num, apr_pool_t *p, slotmem_storage_method *storage)
{
    return create_attach_mem_domain(string, num, 0, 0, p, storage);
}

mem_t *create_mem_domain(char *string, unsigned *num, int persist, apr_pool_t *p, slotmem_storage_method *storage)
{
    return create_attach_mem_domain(string, num, persist, 1, p, storage);
}
