
EVRYTHNG_MARVELL_SDK_PATH=$(shell pwd)
PROJECT_ROOT=$(shell pwd)

export PROJECT_ROOT

all: demo tests

include common.mk

ifeq ($(BOARD),mw300)
BOARD_BIN_DIR=$(WMSDK_BUNDLE_DIR)/bin/$(BOARD)_defconfig/mw300_rd
else
BOARD_BIN_DIR=$(WMSDK_BUNDLE_DIR)/bin/$(BOARD)_defconfig/$(BOARD)
endif


.PHONY: all demo demo_clean tests tests_clean clean 

demo: wmsdk 
	$(AT)$(MAKE) -C $(WMSDK_BUNDLE_DIR) APP=$(PROJECT_ROOT)/apps/demo

demo_clean:
	$(AT)$(MAKE) -C $(WMSDK_BUNDLE_DIR) APP=$(PROJECT_ROOT)/apps/demo clean

demo_flashprog: demo
	cd $(WMSDK_PATH)/tools/OpenOCD; \
	sudo ./flashprog.py --$(BOARD_FW_PARTITION) $(BOARD_BIN_DIR)/evrythng_demo.bin

demo_ramload: demo
	cd $(WMSDK_PATH)/tools/OpenOCD; \
	sudo ./ramload.py $(BOARD_BIN_DIR)/evrythng_demo.axf

demo_footprint:
	$(AT)$(SDK_DIR)/tools/bin/footprint.pl -m $(BOARD_BIN_DIR)/evrythng_demo.map



gen_config:
	$(MAKE) -C $(PROJECT_ROOT)/lib/core gen_config

tests: gen_config wmsdk
	$(AT)$(MAKE) -C $(WMSDK_BUNDLE_DIR) APP=$(PROJECT_ROOT)/apps/tests

tests_clean:
	$(AT)$(MAKE) -C $(WMSDK_BUNDLE_DIR) APP=$(PROJECT_ROOT)/apps/tests clean

tests_ramload: tests
	cd $(WMSDK_PATH)/tools/OpenOCD; \
	sudo ./ramload.py $(BOARD_BIN_DIR)/evrythng_tests.axf

tests_flashprog: tests
	cd $(WMSDK_PATH)/tools/OpenOCD; \
	sudo ./flashprog.py --$(BOARD_FW_PARTITION) $(BOARD_BIN_DIR)/evrythng_tests.bin

clean: demo_clean tests_clean wmsdk_clean 

