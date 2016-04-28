#
# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

exec-y += evrythng_demo

evrythng_demo-objs-y := src/main.c

evrythng_demo-cflags-y := -DAPPCONFIG_DEBUG_ENABLE=1 

