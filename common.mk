ifeq ($(NOISY),1)
AT=
else
AT=@
endif

UNTAR=tar --skip-old-files -xf
RMRF=rm -rf
MAKE=make
PATCH=patch -p1 -N -s <
CHDIR=cd
COPY=cp

WMSDK_BUNDLE=$(notdir $(WMSDK_BUNDLE_PATH))
WMSDK_BUNDLE_DIR=$(EVRYTHNG_MARVELL_SDK_PATH)/$(patsubst %.tar.gz,%,$(WMSDK_BUNDLE))
WMSDK_PATH=$(WMSDK_BUNDLE_DIR)/sdk
WMSDK_DIR=$(notdir $(WMSDK_BUNDLE_DIR))

SDK_PATH=$(WMSDK_PATH)
SDK_DIR=$(WMSDK_BUNDLE_DIR)/sdk

WMSDK_FW_GENERATOR_DIR=$(SDK_DIR)/tools/host-tools/fw_generator
WMSDK_FW_GENERATOR_BIN=$(WMSDK_FW_GENERATOR_DIR)/ed_chacha_fw_generator

WMSDK_USER_PATCHES ?=
ifneq ($(WMSDK_PATCHES_PATH),)
WMSDK_USER_PATCHES = $(wildcard $(WMSDK_PATCHES_PATH)/*.patch)
endif
WMSDK_PATCHES = $(EVRYTHNG_MARVELL_SDK_PATH)/remove_old_evt_lib.patch $(WMSDK_USER_PATCHES)

WMSDK_CYASSL_FEATURE_PACK ?= fp0
WMSDK_CONFIG ?= $(BOARD)_defconfig
BOARD ?= mw300

ifeq ($(BOARD),mw300)
BOARD_FW_PARTITION=mcufw
BOARD_FILE=$(SDK_PATH)/src/boards/mw300_rd.c
endif

ifeq ($(BOARD),mc200_8801)
BOARD_FW_PARTITION=mc200fw
endif

export WMSDK_BUNDLE_DIR BOARD SDK_PATH SDK_DIR

ifeq ($(BOARD),mw300)
export BOARD_FILE
endif

.PHONY: wmsdk wmsdk_clean lib lib_clean wmsdk_fw_generator

wmsdk: $(WMSDK_BUNDLE_DIR) 
	$(AT)$(MAKE) -C $(WMSDK_BUNDLE_DIR) BOARD=$(BOARD) sdk

wmsdk_fw_generator: wmsdk
	$(AT) SDK_DIR=$(WMSDK_BUNDLE_DIR) $(MAKE) -C $(WMSDK_FW_GENERATOR_DIR)
	#$(AT)$(COPY) $(WMSDK_FW_GENERATOR_DIR)/fw_generator $(WMSDK_FW_GENERATOR_BIN)

wmsdk_clean:
	$(AT)$(RMRF) $(WMSDK_BUNDLE_DIR)


$(WMSDK_BUNDLE_DIR):
	$(AT)$(UNTAR) $(WMSDK_BUNDLE_PATH) -C $(EVRYTHNG_MARVELL_SDK_PATH)
	$(foreach patch, $(WMSDK_PATCHES), $(CHDIR) $(WMSDK_BUNDLE_DIR) && $(PATCH) $(patch);)
	$(AT)$(MAKE) -C $(WMSDK_BUNDLE_DIR) CYASSL_FEATURE_PACK=$(WMSDK_CYASSL_FEATURE_PACK) $(WMSDK_CONFIG)
