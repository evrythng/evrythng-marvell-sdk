#install:
#	$(AT)rm -fr $(SDK_PATH)/incl/evrythng
#	$(AT)mkdir -p $(SDK_PATH)/incl/evrythng/marvell
#	$(AT)$(COPY) -pfr $(CURDIR)/core/evrythng/include/evrythng/*.h $(SDK_PATH)/incl/evrythng
#	$(AT)$(COPY) -pfr $(CURDIR)/platform/marvell/*.h $(SDK_PATH)/incl/evrythng
#	$(AT)$(COPY) -pfr $(CURDIR)/build/libevrythng.a $(SDK_PATH)/libs

libs-y += libevrythng

global-cflags-y += -I$(d)/core/evrythng/include -I$(d)/platform/marvell

libevrythng-cflags-y := \
	-I $(d)/core/evrythng/include \
	-I $(d)/core/embedded-mqtt/MQTTClient-C/src \
	-I $(d)/core/embedded-mqtt/MQTTPacket/src \
	-I $(d)/platform/marvell

libevrythng-objs-y := \
	core/evrythng/src/evrythng_core.c \
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
