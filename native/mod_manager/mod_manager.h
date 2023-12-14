/*
 * Copyright The mod_cluster Project Authors
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file  mod_manager.h
 * @brief Manager module for Apache
 * @author Jean-Frederic Clere
 *
 * @defgroup MOD_MANAGER mod_manager
 * @ingroup  APACHE_MODS
 * @{
 */

struct mem
{
    ap_slotmem_instance_t *slotmem;
    const slotmem_storage_method *storage;
    int num;
    apr_pool_t *p;
    apr_status_t laststatus;
};
