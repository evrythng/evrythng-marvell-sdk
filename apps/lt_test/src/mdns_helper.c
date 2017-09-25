/*
 *  Copyright (C) 2008-2017, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <wmstdio.h>
#include <app_framework.h>
#include <mdns_helper.h>
#include <appln_cb.h>
#include <appln_dbg.h>
#include <wm_utils.h>

#define MDNS_TXT_REC_SIZE	32
#define MDNS_DOMAIN_NAME	"local"

static struct mdns_service my_service = {
	.servname = appln_cfg.servname,
	.servtype = "http",
	.domain = MDNS_DOMAIN_NAME,
	.proto = MDNS_PROTO_TCP,
	.port = 80,
};

static char mytxt[MDNS_TXT_REC_SIZE];
static bool mdns_announced;

void hp_mdns_announce(void *iface)
{
	if (!mdns_announced) {
		snprintf(mytxt, MDNS_TXT_REC_SIZE, "api_ver=2|model=ABC");
		mdns_set_txt_rec(&my_service, mytxt, '|');
		mdns_announce_service(&my_service, iface);
		mdns_announced = true;
	} else {
		/* mDNS should re-probe and announce services if device is
		 * re-connected to network after some network abruptions */
		mdns_announce_service_all(iface);
	}
}

void hp_mdns_deannounce(void *iface)
{
	mdns_deannounce_service_all(iface);
	mdns_announced = false;
}

void hp_mdns_reannounce(void *iface)
{
	mdns_reannounce_service_all(iface);
}

/*
 * Start mDNS and advertize our hostname using mDNS
 */
void hp_mdns_start()
{
	dbg("Starting mDNS");
	mdns_start(MDNS_DOMAIN_NAME, appln_cfg.hostname);
}

void hp_mdns_stop()
{
	dbg("Stoping mDNS");
	mdns_stop();
}
