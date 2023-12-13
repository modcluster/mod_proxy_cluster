/*
 * Copyright The mod_cluster Project Authors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * @author Jean-Frederic Clere
 */

#ifndef MOD_CLUSTERSIZE_H
#define MOD_CLUSTERSIZE_H

/* For host.h */
#define HOSTALIASZ  100

/* For context.h */
#define CONTEXTSZ   80

/* For node.h */
#define BALANCERSZ  40
#define JVMROUTESZ  64
#define DOMAINNDSZ  20
#define HOSTNODESZ  64
#define PORTNODESZ  7
#define SCHEMENDSZ  16
#define AJPSECRETSZ 64

/* For balancer.h */
#define COOKNAMESZ  30
#define PATHNAMESZ  30

/* For sessionid.h */
#define SESSIONIDSZ 128

#endif /* MOD_CLUSTERSIZE_H */
