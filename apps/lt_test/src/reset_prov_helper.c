/*
 *  Copyright (C) 2008-2017, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <wmstdio.h>
#include <wmlog.h>
#include <app_framework.h>
#include <reset_prov_helper.h>
#include <appln_cb.h>
#include <appln_dbg.h>
#include <mdev_gpio.h>
#include "push_button.h"

static void reset_prov_pb_callback(int pin, void *data)
{
	ll_log("[reset_prov] Reset to prov pushbutton press\r\n");
	app_reset_saved_network();
}

void hp_configure_reset_prov_pushbutton()
{
	input_gpio_cfg_t gcfg = {
		.gpio = appln_cfg.reset_prov_pb_gpio,
		.type = GPIO_ACTIVE_LOW
	};

	push_button_set_cb(gcfg, reset_prov_pb_callback,
				0, 0, NULL);
}

void hp_unconfigure_reset_prov_pushbutton()
{
	input_gpio_cfg_t gcfg = {
		.gpio = appln_cfg.reset_prov_pb_gpio,
		.type = GPIO_ACTIVE_LOW
	};

	push_button_reset_cb(gcfg, 0);
}

