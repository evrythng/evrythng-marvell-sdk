# Copyright (C) 2008-2017, Marvell International Ltd.
# All Rights Reserved.

exec-y += lt_test
lt_test-objs-y   := src/main.c src/lt_test.c src/reset_prov_helper.c
lt_test-cflags-y := -I$(d)/src
lt_test-prebuilt-libs-y += -lm

UAP_PROV_MDNS_ENABLE=y
lt_test-objs-$(UAP_PROV_MDNS_ENABLE)   += src/mdns_helper.c
lt_test-cflags-$(UAP_PROV_MDNS_ENABLE) += -DAPPCONFIG_MDNS_ENABLE

# Enable for debugging
lt_test-cflags-y += -DAPPCONFIG_DEBUG_ENABLE

lt_test-ftfs-y := lt_test.ftfs
lt_test-ftfs-dir-y := $(d)/www
lt_test-ftfs-api-y := 100

# lt_test-linkerscript-y := $(d)/mc200-xip.ld
