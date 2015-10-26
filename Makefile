
EVRYTHNG_MARVELL_SDK_PATH=$(shell pwd)
PROJECT_ROOT=$(shell pwd)

export PROJECT_ROOT

all: demo tests

include common.mk

.PHONY: all demo demo_clean tests tests_clean clean 

demo: libevrythng
	$(AT)$(MAKE) -C $(WMSDK_BUNDLE_DIR) APPS=$(PROJECT_ROOT)/apps/demo

demo_clean:
	$(AT)$(MAKE) -C $(WMSDK_BUNDLE_DIR) APPS=$(PROJECT_ROOT)/apps/demo clean

demo_flashprog: demo
	cd $(WMSDK_PATH)/tools/OpenOCD; \
	sudo ./flashprog.sh --$(BOARD_FW_PARTITION) $(PROJECT_ROOT)/apps/demo/bin/evrythng_demo.bin \

demo_ramload: demo
	cd $(WMSDK_PATH)/tools/OpenOCD; \
	sudo ./ramload.sh $(PROJECT_ROOT)/apps/demo/bin/evrythng_demo.axf \

demo_footprint:
	$(AT)$(WMSDK_BUNDLE_DIR)/wmsdk/tools/bin/footprint.pl -m apps/demo/bin/evrythng_demo.map



gen_config:
	$(MAKE) -C $(PROJECT_ROOT)/lib/core gen_config

tests: gen_config libevrythng
	$(AT)$(MAKE) -C $(WMSDK_BUNDLE_DIR) APPS=$(PROJECT_ROOT)/apps/tests

tests_clean:
	$(AT)$(MAKE) -C $(WMSDK_BUNDLE_DIR) APPS=$(PROJECT_ROOT)/apps/tests clean

tests_ramload: tests
	cd $(WMSDK_PATH)/tools/OpenOCD; \
	sudo ./ramload.sh $(PROJECT_ROOT)/apps/tests/bin/evrythng_tests.axf \


clean: demo_clean tests_clean libevrythng_clean wmsdk_clean 

