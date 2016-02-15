
ifeq ($(NOISY),1)
AT=
else
AT=@
endif

-include $(SDK_PATH)/.config
ARCH_DRIVER_DIR-$(CONFIG_CPU_MW300) := mw300
ARCH_DRIVER_DIR-$(CONFIG_CPU_MC200) := mc200

# Directory for output files (lst, obj, dep, elf, sym, map, hex, bin etc.).
OUTDIR = build

# Toolchain definition.
CC=arm-none-eabi-gcc
AR=arm-none-eabi-ar
COPY=cp

# Optimization level, can be [0, 1, 2, 3, s].
# Compiler flags definition.
CFLAGS+=-ggdb \
		-Os \
		-D inline= \
		-mthumb \
		-MMD \
		-ffunction-sections \
		-fdata-sections \
		-fno-common \
		-Wall \
		-Wno-unused-function \
		-Wno-address \
		-I $(CURDIR)/core/evrythng/include \
		-I $(CURDIR)/core/embedded-mqtt/MQTTClient-C/src \
		-I $(CURDIR)/core/embedded-mqtt/MQTTPacket/src \
		-I $(CURDIR)/platform/marvell

ifeq ($(CONFIG_CPU_MC200), y)
CFLAGS += -mcpu=cortex-m3
endif

ifeq ($(CONFIG_CPU_MW300), y)
CFLAGS += -mcpu=cortex-m4
endif

CFLAGS += -isystem $(SDK_PATH)/incl \
          -isystem $(SDK_PATH)/incl/libc \
          -isystem $(SDK_PATH)/incl/lwip \
          -isystem $(SDK_PATH)/incl/lwip/ipv4 \
          -isystem $(SDK_PATH)/incl/lwip/ipv6 \
          -isystem $(SDK_PATH)/incl/cyassl \
          -isystem $(SDK_PATH)/incl/cyassl/ctaocrypt \
		  -isystem $(SDK_PATH)/incl/freertos \
		  -isystem $(SDK_PATH)/incl/platform/os/freertos \
		  -isystem $(SDK_PATH)/incl/platform/net/lwip \
		  -isystem $(SDK_PATH)/incl/platform/arch \
		  -isystem $(SDK_PATH)/incl/sdk \
		  -isystem $(SDK_PATH)/incl/sdk/drivers \
		  -isystem $(SDK_PATH)/incl/sdk/drivers/wlan \
		  -isystem $(SDK_PATH)/incl/sdk/drivers/mc200 \
		  -isystem $(SDK_PATH)/incl/sdk/drivers/$(ARCH_DRIVER_DIR-y) \
		  -isystem $(SDK_PATH)/incl/sdk/drivers/$(ARCH_DRIVER_DIR-y)/regs

EVRYTHNG_SOURCE := core/evrythng/src/evrythng_core.c \
	core/evrythng/src/evrythng_api.c \
	core/embedded-mqtt/MQTTClient-C/src/MQTTClient.c \
	core/embedded-mqtt/MQTTPacket/src/MQTTConnectClient.c \
	core/embedded-mqtt/MQTTPacket/src/MQTTConnectServer.c \
	core/embedded-mqtt/MQTTPacket/src/MQTTDeserializePublish.c \
	core/embedded-mqtt/MQTTPacket/src/MQTTFormat.c \
	core/embedded-mqtt/MQTTPacket/src/MQTTPacket.c \
	core/embedded-mqtt/MQTTPacket/src/MQTTSerializePublish.c \
	core/embedded-mqtt/MQTTPacket/src/MQTTSubscribeClient.c \
	core/embedded-mqtt/MQTTPacket/src/MQTTSubscribeServer.c \
	core/embedded-mqtt/MQTTPacket/src/MQTTUnsubscribeClient.c \
	core/embedded-mqtt/MQTTPacket/src/MQTTUnsubscribeServer.c \
	platform/marvell/marvell.c

SOURCE+=$(EVRYTHNG_SOURCE)

# List of all source files without directory and file-extension.
ALLSRCBASE = $(notdir $(basename $(SOURCE)))


# List of all objects files.
OBJS = $(addprefix $(OUTDIR)/, $(addsuffix .o, $(ALLSRCBASE)))
D_LIST = $(wildcard $(addprefix $(CURDIR)/$(OUTDIR)/, $(addsuffix .dep, $(ALLSRCBASE))))


# Define Messages.
# English
MSG_BEGIN = -------- begin --------
MSG_END = --------  end  --------

.PHONY: all begin sdk_artifacts end libs install

# Rules definition. ***********************************************************

LIBS_WM = $(OUTDIR)/libevrythng.a 

all: begin sdk_artifacts end

ifneq ($(D_LIST),)
  include $(D_LIST)
endif

sdk_artifacts: libs install

install:
	$(AT)rm -fr $(SDK_PATH)/incl/evrythng
	$(AT)mkdir -p $(SDK_PATH)/incl/evrythng/marvell
	$(AT)$(COPY) -pfr $(CURDIR)/core/evrythng/include/evrythng/*.h $(SDK_PATH)/incl/evrythng
	$(AT)$(COPY) -pfr $(CURDIR)/platform/marvell/*.h $(SDK_PATH)/incl/evrythng
	$(AT)$(COPY) -pfr $(CURDIR)/build/libevrythng.a $(SDK_PATH)/libs

libs: $(LIBS_WM)

EVRYTHNG_SRC_BASE = $(notdir $(basename $(EVRYTHNG_SOURCE)))
EVRYTHNG_OBJS = $(addprefix $(OUTDIR)/, $(addsuffix .o, $(EVRYTHNG_SRC_BASE)))
$(OUTDIR)/libevrythng.a: $(EVRYTHNG_OBJS)  
	@echo " [ar] $@"
	@echo ""
	$(AT)$(AR) cru $@ $^

# Compile: create object files from C source files.
define COMPILE_C_TEMPLATE
ifneq ($(findstring CYGWIN, ${OS}), )
$(OUTDIR)/$(notdir $(basename $(1))).o : $(1)
##	@echo
	@echo " [cc] $$@"
	$(AT)(`echo $(CC) -c  $$(CFLAGS) $$< -o $$@ | sed -e 's/\/cygdrive\/\([a-zA-Z]\)\//\1:\//g'`)
	$(AT)(sed -i -e 's/ \([a-zA-Z]\):/ \/cygdrive\/\1/g' $(OUTDIR)/$(notdir $(basename $(1))).d && mv $(OUTDIR)/$(notdir $(basename $(1))).d $(OUTDIR)/$(notdir $(basename $(1))).dep)
else
$(OUTDIR)/$(notdir $(basename $(1))).o : $(1)
##	@echo
	@echo " [cc] $$@"
	$(AT)($(CC) -c  $$(CFLAGS) $$< -o $$@)
	$(AT)(mv $(OUTDIR)/$(notdir $(basename $(1))).d $(OUTDIR)/$(notdir $(basename $(1))).dep)
endif
endef
$(foreach src, $(SOURCE), $(eval $(call COMPILE_C_TEMPLATE, $(src))))


clean :
	-@rm -f $(OBJS)
	-@rm -f $(D_LIST)
	-@rm -rf $(OUTDIR)


# Eye candy.
begin:
	@echo $(MSG_BEGIN)
	@mkdir -p $(CURDIR)/build

end:
	@echo $(MSG_END)
