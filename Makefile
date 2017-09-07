
EVRYTHNG_MARVELL_SDK_PATH=$(shell pwd)
PROJECT_ROOT=$(shell pwd)

export PROJECT_ROOT

all: demo

include $(PROJECT_ROOT)/config.mk
include common.mk

ifeq ($(BOARD),mw300)
BOARD_BIN_DIR=$(WMSDK_BUNDLE_DIR)/bin/$(BOARD)_defconfig/mw300_rd
else
BOARD_BIN_DIR=$(WMSDK_BUNDLE_DIR)/bin/$(BOARD)_defconfig/$(BOARD)
endif


.PHONY: all audio_prototype demo demo_clean tests tests_clean clean 

audio_prototype: wmsdk 
	$(AT)$(MAKE) -C $(WMSDK_BUNDLE_DIR) APP=$(PROJECT_ROOT)/apps/audio_prototype XIP=1

audio_prototype_clean:
	$(AT)$(MAKE) -C $(WMSDK_BUNDLE_DIR) APP=$(PROJECT_ROOT)/apps/audio_prototype clean

audio_prototype_flashprog: audio_prototype
	cd $(WMSDK_PATH)/tools/OpenOCD; \
	sudo ./flashprog.py --$(BOARD_FW_PARTITION) $(BOARD_BIN_DIR)/audio_prototype.bin

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

target_reboot: 
	cd $(WMSDK_PATH)/tools/OpenOCD; \
	sudo ./flashprog.py -r


tests: wmsdk
	@cp $(PROJECT_ROOT)/lib/core/tests/tests.c $(PROJECT_ROOT)/apps/tests/src/
	@$(PROJECT_ROOT)/lib/core/tests/gen_header.sh $(PROJECT_ROOT)/test_config $(PROJECT_ROOT)/apps/tests/src/evrythng_config.h
	$(AT)$(MAKE) -C $(WMSDK_BUNDLE_DIR) APP=$(PROJECT_ROOT)/apps/tests XIP=1

tests_clean:
	$(AT)$(MAKE) -C $(WMSDK_BUNDLE_DIR) APP=$(PROJECT_ROOT)/apps/tests clean

tests_ramload: tests
	cd $(WMSDK_PATH)/tools/OpenOCD; \
	sudo ./ramload.py $(BOARD_BIN_DIR)/evrythng_tests.axf

tests_flashprog: tests
	cd $(WMSDK_PATH)/tools/OpenOCD; \
	sudo ./flashprog.py --$(BOARD_FW_PARTITION) $(BOARD_BIN_DIR)/evrythng_tests.bin

clean: demo_clean tests_clean wmsdk_clean 

