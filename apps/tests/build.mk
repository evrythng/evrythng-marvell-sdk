#
# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.


#$(shell cp $(PROJECT_ROOT)/lib/core/tests/tests.c src)
#$(shell cp $(PROJECT_ROOT)/lib/core/tests/CuTest.c src)

exec-y += evrythng_tests

evrythng_tests-objs-y := src/main.c src/CuTest.c src/tests.c 

evrythng_tests-cflags-y := -D APPCONFIG_DEBUG_ENABLE=1

