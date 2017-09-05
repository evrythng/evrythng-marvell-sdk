# Copyright (C) 2008-2017, Marvell International Ltd.
# All Rights Reserved.

UAP_PROV_CONFIG_MICRO_AP_SECURITY_ENABLE=n
UAP_PROV_CONFIG_APP_LEVEL_SECURITY_ENABLE=n
UAP_PROV_CONFIG_HTTPS_ENABLE=n

exec-y += audio_prototype
audio_prototype-objs-y   := src/main.c src/reset_prov_helper.c
audio_prototype-cflags-y := -I$(d)/src

UAP_PROV_MDNS_ENABLE=y
audio_prototype-objs-$(UAP_PROV_MDNS_ENABLE)   += src/mdns_helper.c
audio_prototype-cflags-$(UAP_PROV_MDNS_ENABLE) += -DAPPCONFIG_MDNS_ENABLE

audio_prototype-cflags-$(UAP_PROV_CONFIG_MICRO_AP_SECURITY_ENABLE) \
	+= -DUAP_PROV_CONFIG_MICRO_AP_SECURITY_ENABLE
audio_prototype-cflags-$(UAP_PROV_CONFIG_APP_LEVEL_SECURITY_ENABLE) \
	+= -DUAP_PROV_CONFIG_APP_LEVEL_SECURITY_ENABLE
ifeq ($(CONFIG_ENABLE_HTTPS_SERVER),y)
audio_prototype-cflags-$(UAP_PROV_CONFIG_HTTPS_ENABLE) \
	+= -DUAP_PROV_CONFIG_HTTPS_ENABLE
endif

# Enable for debuggin
#audio_prototype-cflags-y += -DAPPCONFIG_DEBUG_ENABLE

audio_prototype-ftfs-y := audio_prototype.ftfs
audio_prototype-ftfs-dir-y := $(d)/www
audio_prototype-ftfs-api-y := 100

audio_prototype-supported-toolchain-y := arm_gcc iar
