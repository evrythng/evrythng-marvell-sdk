/*
 *  Copyright (C) 2008-2017, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef MDNS_HELPER_H_
#define MDNS_HELPER_H_

#include <mdns.h>

/* mDNS */
#ifdef APPCONFIG_MDNS_ENABLE
void hp_mdns_start(void);
void hp_mdns_announce(void *iface);
void hp_mdns_deannounce(void *iface);
void hp_mdns_reannounce(void *iface);
#else
static inline void hp_mdns_start(void)
{}

static inline void hp_mdns_stop(void)
{}

static inline void hp_mdns_announce(void *iface)
{}

static inline void hp_mdns_deannounce(void *iface)
{}

static inline void hp_mdns_reannounce(void *iface)
{}
#endif   /* APPCONFIG_MDNS_ENABLE */

#endif /* ! _HELPERS_H_ */
